#!/usr/bin/env python
# Copyright (C) 2010 Google Inc. All rights reserved.
# Copyright (c) 2011-2013, NVIDIA CORPORATION. All rights reserved.
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions are
# met:
#
#     * Redistributions of source code must retain the above copyright
# notice, this list of conditions and the following disclaimer.
#     * Redistributions in binary form must reproduce the above
# copyright notice, this list of conditions and the following disclaimer
# in the documentation and/or other materials provided with the
# distribution.
#     * Neither the name of Google Inc. nor the names of its
# contributors may be used to endorse or promote products derived from
# this software without specific prior written permission.
#
# THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
# "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
# LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
# A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
# OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
# SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
# LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
# DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
# THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
# (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
# OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
"""Android implementations of the Port interface."""

from __future__ import with_statement

import codecs
import errno
import logging
import os
import re
import shutil
import select
import signal
import subprocess
import sys
import tempfile
import time
import webbrowser
import zipfile

from webkitpy.common.system.path import cygpath
from webkitpy.common.system import ospath
from webkitpy.layout_tests.layout_package import test_expectations

import base
import http_server

import webkit

import websocket_server
import socket
import calendar
import hashlib
_log = logging.getLogger("webkitpy.layout_tests.port.android")

def drtActivityNameForCompositingMode(use_sw_compositing):
    if use_sw_compositing:
        return 'org.webkit.dumprendertree/.DumpRenderTreeSWActivity'
    return 'org.webkit.dumprendertree/.DumpRenderTreeActivity'

def find_in_path(bin_name):
    path = os.environ['PATH']
    for dir in path.split(os.pathsep):
        bin_path = os.path.join(dir, bin_name)
        if os.path.exists(bin_path):
            return os.path.abspath(bin_path)
    return None


def parse_android_lsl_line(line):
    line = line.strip()
    if len(line) == 0:
        return None

    parts = line.split(None, 6)

    if len(parts) < 6:
        return None

    is_dir = parts[0][0] == 'd'
    size = int(parts[3]) if not is_dir else 0
    # Directories do not have size, so access fields counting from the last one.
    fn = parts[-1]

    # Use :59 to avoid having the loss of precision cause files uploaded.
    mtime = int(calendar.timegm(time.strptime("%s %s:59 UTC" %(parts[-3], parts[-2]), "%Y-%m-%d %H:%M:%S %Z")))

    return {"file_name": fn, "mtime": mtime, "is_dir": is_dir, "size": size}

def parse_android_lsRl(test_base_dir, output, filter_out_func):
    files_times = {}
    try:
        lines = iter(output.splitlines())
        lines.next()                # empty
        while (True):
            path = lines.next().strip()
            path = path[len(test_base_dir)+1:-1]
            while (True):
                file_entry = parse_android_lsl_line(lines.next())
                if not file_entry:
                    break

                if file_entry["is_dir"]:
                    continue

                filename = os.path.join(path, file_entry["file_name"])
                if filter_out_func(filename):
                    continue

                files_times[filename] = file_entry["mtime"]
    except StopIteration, e:
        pass
    return files_times

class AndroidPort(base.Port):
    """Android implementation of the Port class."""

    min_adb_version = [1, 0, 31]

    def __init__(self, **kwargs):
        kwargs.setdefault('port_name', 'android')
        base.Port.__init__(self, **kwargs)
        self._adb_path = None
        self._compare_path = None
        self._build_base_dir = None
        self._target_product_name = None

    def _check_file_exists(self, path_to_file, file_description,
                           override_step=None, logging=True):
        """Verify the file is present where expected or log an error.

        Args:
            file_name: The (human friendly) name or description of the file
                you're looking for (e.g., "HTTP Server"). Used for error logging.
            override_step: An optional string to be logged if the check fails.
            logging: Whether or not log the error messages."""
        if not self._filesystem.exists(path_to_file):
            if logging:
                _log.error('Unable to find %s' % file_description)
                _log.error('    at %s' % path_to_file)
                if override_step:
                    _log.error('    %s' % override_step)
                    _log.error('')
            return False
        return True

    def default_child_processes(self):
        return 1

    def baseline_path(self):
        return self._webkit_baseline_path(self._name)

    def check_build(self, needs_http):
        result = self.check_android_env()

        if result:
            result = self._check_file_exists(self._dump_render_tree_package_path(), "DumpRenderTree package") and result

        if self.get_option('pixel_tests'):
            result = self.check_image_diff() and result

        if self.get_option('sw_compositing') and self.get_option('force_sw_pixel_tests'):
            _log.error("--sw-compositing cannot be used with --force-sw-pixel-tests")
            result = False

        if self.get_option('sw_compositing') and self.get_option('force_single_surface_rendering'):
            _log.error("--sw-compositing cannot be used with --force-single-surface-mode")
            result = False

        # It's okay if pretty patch isn't available, but we will at
        # least log a message.
        self.check_pretty_patch()

        if not result:
            return result

        if needs_http:
            if self.get_option('use_apache'):
                result = self._check_apache_install() and result
            else:
                result = self._check_lighttpd_install() and result
        result = self._check_wdiff_install() and result

        if not result:
            _log.error('For complete Linux build requirements, please see:')
            _log.error('')
            _log.error('    apt-get install apache2 libapache2-mod-php5 imagemagick'
                       '')
        return result

    def _check_adb_version(self):
        adb_version = self._run_adb(["version"]).strip()
        matches = re.match(".*version (\d+).(\d+).(\d+)", adb_version)
        if not matches:
            _log.error('Cannot determine adb version. Got: "%s"' % adb_version)
            return False


        adb_version = [int(x) for x in matches.group(1, 2, 3)]

        for v in zip(adb_version, AndroidPort.min_adb_version):
            if v[0] > v[1]:
                break
            if v[0] < v[1]:
                _log.error('The version of adb is too old. Required: %s, got: %s. Check PATH or use newer adb.'
                           % (".".join(str(x) for x in AndroidPort.min_adb_version), ".".join(str(x) for x in adb_version)))
                return False
        return True

    def check_sys_deps(self, needs_http):
        if not self.check_android_env():
            return False

        adb_path = self._path_to_adb()
        if not (adb_path and self._check_file_exists(adb_path, 'adb', None, True)):
            _log.error('Cannot find adb in path. Check Android build.')
            return False

        if not self._check_adb_version():
            return False

        if not self._ensure_target_device_name():
            return False

        output = self._run_on_device(["echo", "ok"]).strip()

        if output != "ok":
            _log.error('Cannot execute adb shell. Check permissions and the device.')
            return False

        package_suffixes = ["", "-1", "-2"]
        for suffix in package_suffixes:
            (apk_needs_update, apk_exists) = self._need_update_file_on_device(
                self._dump_render_tree_package_path(),
                "/data/app/org.webkit.dumprendertree" + suffix + ".apk")

            if apk_exists:
                break

        if apk_needs_update:
            cmd = ["install"]
            if apk_exists:
                cmd.append("-r")
            cmd.append(self._dump_render_tree_package_path())
            exit_code = self._run_adb(cmd,  return_exit_code=True)
            if exit_code != 0:
                _log.error('Cannot install DumpRenderTree package. Check the build.')
                return False

        return True

    def check_image_diff(self, override_step=None, logging=True):
        compare_path = self._path_to_image_diff()
        result = compare_path and self._check_file_exists(compare_path, 'image diff exe (compare)', override_step, logging)
        return result

    def diff_image(self, expected_contents, actual_contents,
                   diff_filename=None):

        def compare_error_handler(error):
            _log.error("image diff failed:\n" + error.message_with_output())

        compare_exe = self._path_to_image_diff()

        tempdir = tempfile.mkdtemp()
        expected_filename = os.path.join(tempdir, "expected.png")
        with open(expected_filename, 'w+b') as file:
            file.write(expected_contents)
        actual_filename = os.path.join(tempdir, "actual.png")
        with open(actual_filename, 'w+b') as file:
            file.write(actual_contents)

        output_filename = "/dev/null"
        if diff_filename:
            output_filename = diff_filename

        cmd = [compare_exe, '-metric', 'RMSE', '-dissimilarity-threshold', '1', expected_filename, actual_filename, output_filename]

        are_different = True
        try:
            output = self._executive.run_command(cmd, error_handler=compare_error_handler)
            rmse = float(output.strip().split()[0])
            are_different = (rmse > 0)
        except OSError, e:
            if e.errno == errno.ENOENT or e.errno == errno.EACCES:
                _compare_available = False
            else:
                raise e
        finally:
            shutil.rmtree(tempdir, ignore_errors=True)
        return are_different

    def expected_checksum(self, test):
        result = super(AndroidPort, self).expected_checksum(test)
        if result is not None:
            return result

        png_path = self.expected_filename(test, '.png')

        if self.path_exists(png_path):
            with self._filesystem.open_binary_file_for_reading(png_path) as filehandle:
                return hashlib.md5(filehandle.read()).hexdigest()
        return None

    def path_to_test_expectations_file(self):
        return self.path_from_webkit_base('LayoutTests', 'platform',
            'android', 'test_expectations_nwrt.txt')

    def test_expectations_overrides(self):
        FILE_NAME = 'test_expectations_overrides_nwrt.txt'

        dirs = self._android_platform_dir_cascade()
        # The last directory in the list cannot have an override file
        for dir in dirs[:-1]:
            file = self.path_from_webkit_base('LayoutTests', 'platform', dir, FILE_NAME)
            if os.path.exists(file):
                _log.debug("Using override file %s" % file)
                return self._filesystem.read_text_file(file)
        _log.debug("No override file")
        return None

    def default_results_directory(self):
        compositing = ("sw" if self._is_using_sw_pixel_tests() else "hw")
        dir = "android-%s-%s" % (compositing, self._target_product())
        _log.debug("Result directory %s" % dir)
        return self._build_path('layout-test-results', dir)

    def default_worker_model(self):
        # 'threads' and 'processes' cause deadlock at KeyboardInterrupt
        return 'inline'

    def create_driver(self, worker_number):
        """Starts a new Driver and returns a handle to it."""
        sw_compositing = self.get_option('sw_compositing')
        force_sw_pixel_tests = False
        if sw_compositing:
            _log.debug("using software compositing mode")
        else:
            force_sw_pixel_tests = self.get_option('force_sw_pixel_tests')
            if force_sw_pixel_tests:
                _log.debug("using software pixel tests with hardware compositing mode")
        force_single_surface_rendering = False
        if not sw_compositing:
            force_single_surface_rendering = self.get_option('force_single_surface_rendering')
            if force_single_surface_rendering:
                _log.debug("forcing single surface mode")
        return AndroidDriver(self, worker_number, sw_compositing, force_sw_pixel_tests, force_single_surface_rendering)

    def test_base_platform_names(self):
        return ("android")

    def test_platform_name(self):
        return self._name + self.version()

    def test_platform_names(self):
        return self.test_base_platform_names() + ("android")

    def test_platform_name_to_name(self, test_platform_name):
        if test_platform_name in self.test_platform_names():
            return test_platform_name
        raise ValueError('Unsupported test_platform_name: %s' %
                         test_platform_name)

    def version(self):
        return ''


    def layout_tests_dir_on_device(self):
        return '/data/webkit/layout-tests'

    def filename_to_uri_for_test_command(self, filename):
        uri = self.filename_to_uri(filename)
        if uri.startswith("http://") or uri.startswith("https://"):
            return uri

        relative_path = self.relative_test_filename(filename)
        return self._filesystem.join(self.layout_tests_dir_on_device(), relative_path)


    def setup_test_run(self):
        self._run_on_device(['date', '-u', '%s' % int(time.mktime(time.gmtime()))])

        if self.get_option('no_sync'):
            _log.debug("Not pushing changed tests to device.")
        else:
            self.sync_tests_to_device()

    def sync_tests_to_device(self):
        output = self._run_on_device(['ls', '-Rl', '%s' % self.layout_tests_dir_on_device()])

        def is_not_test_file(testname):
            return testname.endswith('-expected.txt') or testname.endswith('-expected.png') or testname.endswith('-expected.checksum') or testname.startswith('http/')

        files_on_device = parse_android_lsRl(self.layout_tests_dir_on_device(), output, is_not_test_file)

        files_to_update = []

        for root, dirs, files in os.walk(self.layout_tests_dir()):
            for filename in files:
                fullname = self._filesystem.join(root, filename)
                testname = self.relative_test_filename(fullname)
                if is_not_test_file(testname):
                    continue

                if testname in files_on_device:
                    host_mtime = os.stat(fullname).st_mtime
                    device_mtime = files_on_device[testname]
                    if host_mtime <= device_mtime:
                        continue

                files_to_update.append(fullname)

        files_to_update.sort()

        if len(files_to_update) > 0:
            zip_basename = "rlt-files.zip"
            zip_name = self._filesystem.join(tempfile.gettempdir(), zip_basename)
            with zipfile.ZipFile(zip_name, "w") as zf:
                for full_name in files_to_update:
                    archive_name = self.relative_test_filename(full_name)
                    zf.write(full_name, archive_name)

            _log.debug('Pushing %s layout test files to device (use --no-sync to disable)' % len(files_to_update))

            local_drtunzip = self._filesystem.join(self._build_path(), "system/bin/drtunzip")
            device_drtunzip = "/system/bin/drtunzip"
            if self._need_update_file_on_device(local_drtunzip, device_drtunzip)[0]:
                self._run_adb(["remount"])
                self._push_file_to_device(local_drtunzip, device_drtunzip)
            self._run_on_device(["mkdir", "-p", self.layout_tests_dir_on_device()])
            self._run_on_device(["touch", "%s/.nomedia" % self.layout_tests_dir_on_device()])
            self._push_file_to_device(zip_name, self.layout_tests_dir_on_device() + "/")
            self._run_on_device(["cd", self.layout_tests_dir_on_device(), "&&", "drtunzip", zip_basename, "&&", "rm", zip_basename])
            self._filesystem.remove(zip_name)

    #
    # PROTECTED METHODS
    #
    # These routines should only be called by other methods in this file
    # or any subclasses.
    #
    def check_android_env(self):
        env_names = ['ANDROID_PRODUCT_OUT', 'TARGET_PRODUCT']
        for env_name in env_names:
            if not os.environ.get(env_name):
                _log.error('You need to define Android environment. '
                           'Missing environment variable ' + env_name)
                return False
        return True

    def _run_adb(self, cmd, **kwargs):
        cmd = [self._path_to_adb()] + cmd
        return self._executive.run_command(cmd, **kwargs)

    def _run_on_device(self, cmd, **kwargs):
        return self._run_adb(["shell"] + cmd, **kwargs)

    def _push_file_to_device(self, host_filename, device_filename):
        return self._run_adb(["push", host_filename, device_filename], return_exit_code=True);

    def _need_update_file_on_device(self, local_filename, device_filename):
        result = self._file_size_on_device(device_filename)
        if result == None:
            return (True, False)

        need_update = result != os.stat(local_filename).st_size
        if not need_update:
            device_md5 = self._run_on_device(['md5', device_filename, "2>/dev/null"]).split(" ", 1)[0]
            with self._filesystem.open_binary_file_for_reading(local_filename) as filehandle:
                local_md5 = hashlib.md5(filehandle.read()).hexdigest()
            need_update = device_md5 != local_md5

        return (need_update, True)

    def _file_size_on_device(self, device_filename):
        """Return the size of a file on device if it exists or None if it does not.
        """
        output = self._run_on_device(['ls', '-l', device_filename , "2>/dev/null"])
        result = parse_android_lsl_line(output)
        if not result:
            return None

        return result["size"]

    def _android_platform_dir_cascade(self):
        """ Returns the list of platform directories:
            Composition mode    Board       Search order
            hw                  pluto       android-pluto/ -> android/
            sw                  pluto       android-sw-pluto/ -> android-sw/ -> android/
            hw                  cardhu      android-cardhu/ -> android/
            sw                  cardhu      android-sw-cardhu/ -> android-sw/ -> android/
            """
        product_dir = "android"
        product_dir += ("-sw" if self._is_using_sw_pixel_tests() else "")
        product_dir += ("-" + self._target_product() if self._target_product() else "")

        dirs = [product_dir]
        if self._is_using_sw_pixel_tests():
            dirs += ["android-sw"]
        dirs += ["android"]
        return dirs

    def baseline_search_path(self):
        return map(self._webkit_baseline_path, self._android_platform_dir_cascade())

    def _dump_render_tree_package_path(self):
        return self._build_path("data", "app", "org.webkit.dumprendertree.apk")

    def _path_to_image_diff(self):
        if not self._compare_path:
            self._compare_path = find_in_path("compare")
        return self._compare_path

    def _path_to_adb(self):
        if not self._adb_path:
            self._adb_path = find_in_path("adb")

        return self._adb_path

    def _target_product(self):
        if not self._target_product_name:
            self._target_product_name = os.environ.get('TARGET_PRODUCT')
        return self._target_product_name

    def _ensure_target_device_name(self):
        """ Makes sure environment contains ANDROID_SERIAL variable with a serial of the intended device.
            The device is selected based on TARGET_PRODUCT if it is available.

            Returns False if couldn't determine correct serial. True on success.
        """
        online_candidates = self._run_adb(["devices", "-l"]).strip()
        pattern = re.compile(r"(\S+).*?\bproduct:(\w+)")

        online_devices = dict(a.group(1, 2) for a in re.finditer(pattern, online_candidates) if a)

        if len(online_devices) == 0:
            _log.error('    No devices online. Check USB connection.')
            return False

        if os.environ.get('ANDROID_SERIAL'):
            target_device = os.environ['ANDROID_SERIAL']
            if not target_device in online_devices:
                _log.error('    Device ANDROID_SERIAL=%s is not online' % target_device)
                return False

            if self._target_product() and online_devices[target_device] != self._target_product():
                _log.error('    Device ANDROID_SERIAL=%s is wrong product: %s. Expected TARGET_PRODUCT=%s' % (target_device, online_devices[target_device], self._target_product()))
                return False
        else:
            candidates = [(device, product) for device, product in online_devices.items() if not self._target_product() or product == self._target_product()]

            if len(candidates) != 1:
                if self._target_product():
                    product_error_msg = "for TARGET_PRODUCT=%s" % self._target_product()
                else:
                    product_error_msg = ""

                if len(candidates) == 0:
                    _log.error('    No devices %s found. Use ANDROID_SERIAL=<serial> or check the USB connection.' % product_error_msg)
                else:
                    _log.error('    Multiple devices %s found. Use ANDROID_SERIAL=<serial> or check the USB connections.\n    Candidates: %s'  % (product_error_msg, ", ".join(map(lambda x: "%s:%s" % (x[0], x[1]), candidates))))
                return False

            # Set the ANDROID_SERIAL for any future child process.
            # This ensures that adb calls are going to the target
            # device.
            os.environ['ANDROID_SERIAL'] = candidates[0][0]
            _log.info("Using device ANDROID_SERIAL=%s" % candidates)

        return True

    def _build_path(self, *comps):
        if self._build_base_dir is None:
            self._build_base_dir = os.getenv('ANDROID_PRODUCT_OUT')
        return self._filesystem.join(self._build_base_dir, *comps)

    def _check_apache_install(self):
        result = self._check_file_exists(self._path_to_apache(),
            "apache2")
        result = self._check_file_exists(self._path_to_apache_config_file(),
            "apache2 config file") and result
        if not result:
            _log.error('    Please install using: "sudo apt-get install '
                       'apache2 libapache2-mod-php5"')
            _log.error('')
        return result

    def _check_lighttpd_install(self):
        result = self._check_file_exists(
            self._path_to_lighttpd(), "LigHTTPd executable")
        result = self._check_file_exists(self._path_to_lighttpd_php(),
            "PHP CGI executable") and result
        result = self._check_file_exists(self._path_to_lighttpd_modules(),
            "LigHTTPd modules") and result
        if not result:
            _log.error('    Please install using: "sudo apt-get install '
                       'lighttpd php5-cgi"')
            _log.error('')
        return result

    def _check_wdiff_install(self):
        result = self._check_file_exists(self._path_to_wdiff(), 'wdiff')
        if not result:
            _log.error('    Please install using: "sudo apt-get install '
                       'wdiff"')
            _log.error('')
        # FIXME: The AndroidMac port always returns True.
        return result

    def _path_to_apache(self):
        if self._is_redhat_based():
            return '/usr/sbin/httpd'
        else:
            return '/usr/sbin/apache2'

    def _path_to_apache_config_file(self):
        if self._is_redhat_based():
            config_name = 'fedora-httpd.conf'
        else:
            config_name = 'apache2-debian-httpd.conf'

        return os.path.join(self.layout_tests_dir(), 'http', 'conf',
                            config_name)

    def _path_to_lighttpd(self):
        return "/usr/sbin/lighttpd"

    def _path_to_lighttpd_modules(self):
        return "/usr/lib/lighttpd"

    def _path_to_lighttpd_php(self):
        return "/usr/bin/php-cgi"

    def _path_to_wdiff(self):
        if self._is_redhat_based():
            return '/usr/bin/dwdiff'
        else:
            return '/usr/bin/wdiff'

    def _is_redhat_based(self):
        return os.path.exists(os.path.join('/etc', 'redhat-release'))

    def _shut_down_http_server(self, server_pid):
        """Shut down the lighttpd web server. Blocks until it's fully
        shut down.

        Args:
            server_pid: The process ID of the running server.
        """
        # server_pid is not set when "http_server.py stop" is run manually.
        if server_pid is None:
            # TODO(mmoss) This isn't ideal, since it could conflict with
            # lighttpd processes not started by http_server.py,
            # but good enough for now.
            self._executive.kill_all("lighttpd")
            self._executive.kill_all("apache2")
        else:
            try:
                os.kill(server_pid, signal.SIGTERM)
                # TODO(mmoss) Maybe throw in a SIGKILL just to be sure?
            except OSError:
                # Sometimes we get a bad PID (e.g. from a stale httpd.pid
                # file), so if kill fails on the given PID, just try to
                # 'killall' web servers.
                self._shut_down_http_server(None)

    def _is_using_sw_pixel_tests(self):
        return self.get_option('sw_compositing') or self.get_option('force_sw_pixel_tests')


class AdbServerProcess():
    def __init__(self, port_obj, executive, adb_port_out, adb_port_err, use_sw_compositing=False, force_sw_pixel_tests=False, force_single_surface_rendering=False):
        self._port = port_obj
        self._executive = executive
        self._adb_port_out = adb_port_out
        self._adb_port_err = adb_port_err
        self._use_sw_compositing = use_sw_compositing
        self._force_sw_pixel_tests = force_sw_pixel_tests
        self._force_single_surface_rendering = force_single_surface_rendering
        self._reset()

    def _reset(self):
        self._socket_out = None
        self._socket_err = None
        self._output = ''
        self.crashed = False
        self.timed_out = False
        self.error = ''

    def _start(self):
        # Let's kill potential old drt instances first...
        if self._kill_after_grace_period(0):
            # Android does not start a new activity if we try to start
            # it immediately after the process of the activity was
            # killed. Thus sleep awhile.
            time.sleep(2)


        port_out_name = 'android-drt-%s' % self._adb_port_out
        port_err_name = 'android-drt-%s' % self._adb_port_err

        exit_code = self._port._run_adb(['forward', 'local:%s' % port_out_name, 'tcp:%s' % self._adb_port_out], return_exit_code=True)

        if exit_code != 0:
            raise RuntimeError("Failed to forward output socket")

        exit_code = self._port._run_adb(['forward', 'local:%s' % port_err_name, 'tcp:%s' % self._adb_port_err], return_exit_code=True)
        if exit_code != 0:
            raise RuntimeError("Failed to forward error socket")

        cmd = ['am', 'start', '-a', 'android.intent.action.VIEW',
               '--ei', 'outPort', '%s' % self._adb_port_out,
               '--ei', 'errPort', '%s' % self._adb_port_err]

        if self._force_sw_pixel_tests:
            cmd = cmd + ['--ez', 'forceSWPixelTests', 'true']

        if self._force_single_surface_rendering:
            cmd = cmd + ['--ez', 'forceSingleSurfaceRendering', 'true']

        cmd = cmd + ['-n', drtActivityNameForCompositingMode(self._use_sw_compositing)]

        result = self._port._run_on_device(cmd)
        if not result.startswith("Starting: Intent"):
            raise RuntimeError("Failed to start the service. Got: '%s'" % result)

        # Need to sleep so that the Android service has time to start.
        # Otherwise the first test will timeout because nobody listens
        # to the socket when the test is written to it.
        time.sleep(2)

        if self._pid_of_service() is None:
           _log.error("Unable to start DumpRenderTree")

        self._socket_out = socket.socket(socket.AF_UNIX, socket.SOCK_STREAM)
        self._socket_out.connect('/tmp/%s' % port_out_name)
        self._socket_out.settimeout(None)

        self._socket_err = socket.socket(socket.AF_UNIX, socket.SOCK_STREAM)
        self._socket_err.connect('/tmp/%s' % port_err_name)
        self._socket_err.settimeout(None)

    def poll(self):
        #FIXME: this is fake.
        # Running _pid_of_service is too intensive.
        return None

    def _pid_of_service(self):
        output = self._port._run_on_device(["ps"])
        processes = output.splitlines()
        for proc in processes:
            fields = proc.split(None, 9)
            if len(fields) != 9:
                continue
            pid = fields[1]
            name = fields[8]
            if name.startswith('org.webkit.dumprendertree'):
                return int(pid)
        return None

    def write(self, input):
        if not self._socket_out:
            self._start()
        self._socket_out.send(input.encode('utf-8'))


    def _read(self, timeout, size):
        """Internal routine that actually does the read."""
        index = -1
        select_fds = (self._socket_out.fileno(), self._socket_err.fileno())
        deadline = time.time() + timeout
        while not self.timed_out and not self.crashed:
            if self.poll() != None:
                self.crashed = True

            now = time.time()
            if now > deadline:
                self.timed_out = True

            # Check to see if we have any output we can return.
            if size and len(self._output) >= size:
                index = size
            elif size == 0:
                index = self._output.find('\n') + 1

            if index > 0 or self.crashed or self.timed_out:
                output = self._output[0:index]
                self._output = self._output[index:]
                return output

            # Nope - wait for more data.
            (read_fds, write_fds, err_fds) = select.select(select_fds, [],
                                                           select_fds,
                                                           deadline - now)
            try:
                if self._socket_out.fileno() in read_fds:
                    self._output += self._socket_out.recv(4096)
            except IOError, e:
                pass

            try:
                if self._socket_err.fileno() in read_fds:
                    self.error += self._socket_err.recv(4096)
            except IOError, e:
                pass

    def _kill_after_grace_period(self, timeout):
        """ Kills process if it hasn't quit during the grace period. Returns
        True if it was killed"""

        timeout = time.time() + timeout

        while self._pid_of_service() is not None and time.time() < timeout:
            time.sleep(0.1)

        pid = self._pid_of_service()
        if pid is None:
            return False
        else:
            self._port._run_on_device(["kill", "-s", "SIGKILL", pid])
            return True

    def stop(self):
        if self._socket_out:
            self._socket_out.close()
        if self._socket_err:
            self._socket_err.close()
        if self._pid_of_service() is None:
            return

        exit_code = self._port._run_on_device(['am', 'start', '-a', 'android.intent.action.ACTION_SHUTDOWN', '-n', drtActivityNameForCompositingMode(self._use_sw_compositing)], return_exit_code=True)
        if exit_code != 0:
            raise RuntimeError("Failed to shutdown the service")

        if self._kill_after_grace_period(3):
            _log.warning('Runner service timed out and it was killed')

        self._reset()

    def read(self, timeout, size):
        if size <= 0:
            raise ValueError('ServerProcess.read() called with a '
                             'non-positive size: %d ' % size)
        return self._read(timeout, size)

    def read_line(self, timeout):
        return self._read(timeout, size=0)


class AndroidDriver(webkit.WebKitDriver):
    """Driver for Android DumpRenderTree."""

    def __init__(self, port, worker_number, use_sw_compositing=False, force_sw_pixel_tests=False, force_single_surface_rendering=False):
        webkit.WebKitDriver.__init__(self, port, worker_number)
        self._adb_port_out = 50000 + worker_number
        self._adb_port_err = 60000 + worker_number
        self._use_sw_compositing = use_sw_compositing
        self._force_sw_pixel_tests = force_sw_pixel_tests
        self._force_single_surface_rendering = force_single_surface_rendering

    def start(self):
        self._server_process = AdbServerProcess(self._port, self._port._executive, self._adb_port_out, self._adb_port_err, self._use_sw_compositing, self._force_sw_pixel_tests, self._force_single_surface_rendering)

    def cmd_line(self):
        return "**not used**"
