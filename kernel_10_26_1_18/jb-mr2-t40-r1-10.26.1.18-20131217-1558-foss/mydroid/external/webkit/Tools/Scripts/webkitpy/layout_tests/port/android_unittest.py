# Copyright (C) 2010 Google Inc. All rights reserved.
# Copyright (c) 2013, NVIDIA CORPORATION. All rights reserved.
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions are
# met:
#
#    * Redistributions of source code must retain the above copyright
# notice, this list of conditions and the following disclaimer.
#    * Redistributions in binary form must reproduce the above
# copyright notice, this list of conditions and the following disclaimer
# in the documentation and/or other materials provided with the
# distribution.
#    * Neither the name of Google Inc. nor the names of its
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

import string
import unittest
import os
import android
import base
from webkitpy.tool import mocktool

class AdbServerProcessTest(unittest.TestCase):
    def make_port(self):
        port_obj = android.AndroidPort()
        return port_obj

    def test_pidof_service(self):
        class MockExecute:
            def run_command(self,
                            args,
                            cwd=None,
                            input=None,
                            error_handler=None,
                            return_exit_code=False,
                            return_stderr=True,
                            decode_output=False):
                cmd = string.join(args, " ")

                if return_exit_code:
                    return 0

                if cmd.endswith("adb shell ps"):
                    return """
USER     PID   PPID  VSIZE  RSS     WCHAN    PC         NAME
root      1     0     328    196   c00f7c18 000086cc S /init
root      2     0     0      0     c008fbbc 00000000 S kthreadd
root      3     2     0      0     c007dd74 00000000 S ksoftirqd/0
root      6     2     0      0     c00b08cc 00000000 S migration/0
root      7     2     0      0     c00b08cc 00000000 S migration/1
root      9     2     0      0     c007dd74 00000000 S ksoftirqd/1
root      10    2     0      0     c007dd74 00000000 S org.webkit.dumprendertree
"""
                raise RuntimeError("Unexpected cmd" + cmd)
        port = android.AndroidPort()

        port._executive = MockExecute()
        adb_server = android.AdbServerProcess(port, port._executive, 1, 2)
        self.assertEquals(10, adb_server._pid_of_service())


class AndroidTestBaselines(unittest.TestCase):
    def test_baselines(self):
        port_obj = android.AndroidPort()

        test_filename = port_obj._filesystem.join(port_obj.layout_tests_dir(), "fast/encoding/hebrew/8859-8-e.html")
        expected_filename = port_obj.expected_filename(test_filename, ".txt")
        exp = port_obj._filesystem.join(port_obj.layout_tests_dir(), "fast/encoding/hebrew/8859-8-e-expected.txt")
        self.assertEquals(exp, expected_filename)

        test_filename = port_obj._filesystem.join(port_obj.layout_tests_dir(), "android/fast/encoding/denormalised-voiced-japanese-chars.html")
        expected_filename = port_obj.expected_filename(test_filename, ".txt")
        exp = port_obj._filesystem.join(port_obj.layout_tests_dir(), "android/fast/encoding/denormalised-voiced-japanese-chars-expected.txt")
        self.assertEquals(exp, expected_filename)

class AndroidTestEnv(unittest.TestCase):
    def test_env(self):
        old = os.environ['ANDROID_PRODUCT_OUT']

        os.environ['ANDROID_PRODUCT_OUT'] = '/a/b/c/d'
        port_obj = android.AndroidPort()
        self.assertEquals(port_obj._build_path("build"), "/a/b/c/d/build")

        os.environ['ANDROID_PRODUCT_OUT'] = old

    def test_missing_env(self):
        old = os.environ['ANDROID_PRODUCT_OUT']

        del os.environ['ANDROID_PRODUCT_OUT']
        port_obj = android.AndroidPort()
        self.assertEquals(port_obj.check_sys_deps(False), False)

        os.environ['ANDROID_PRODUCT_OUT'] = old

class AndroidTestName(unittest.TestCase):
    def test_name(self):
        port_obj = android.AndroidPort()
        self.assertEquals(port_obj.name(), "android")

class AndroidTestBuildType(unittest.TestCase):
    def test_build_type(self):
        port_obj = android.AndroidPort()
        # Only release currently supported
        self.assertEquals(port_obj.test_configuration().build_type, "release")

class AndroidDeviceTestCase(unittest.TestCase):
    saved_env_names = ["ANDROID_SERIAL", "TARGET_PRODUCT"]
    def setUp(self):
        self.saved_env = {}

        for name in AndroidDeviceTestCase.saved_env_names:
            if os.environ.get(name):
                self.saved_env[name] = os.environ[name]
                del os.environ[name]

    def tearDown(self):
        for name, value in self.saved_env.items():
            os.environ[name] = value

class AndroidTestVersion(AndroidDeviceTestCase):
    def adb(self, cmd, ver, func):
        if cmd == ['version']:
            return "Android Debug Bridge version " + '.'.join([str(x) for x in ver])
        return func(cmd)

    def test_next_major(self):
        port_obj = android.AndroidPort()
        real_func = port_obj._run_adb
        v = android.AndroidPort.min_adb_version[:]
        v[0] = v[0] + 1
        v[1] = 0
        v[2] = 0
        port_obj._run_adb = lambda cmd: self.adb(cmd, v, real_func)
        self.assertTrue(port_obj._check_adb_version())

    def test_next_mid(self):
        port_obj = android.AndroidPort()
        real_func = port_obj._run_adb
        v = android.AndroidPort.min_adb_version[:]
        v[1] = v[1] + 1
        v[2] = 0
        port_obj._run_adb = lambda cmd: self.adb(cmd, v, real_func)
        self.assertTrue(port_obj._check_adb_version())

    def test_next_minor(self):
        port_obj = android.AndroidPort()
        real_func = port_obj._run_adb
        v = android.AndroidPort.min_adb_version[:]
        v[2] = v[2] + 1
        port_obj._run_adb = lambda cmd: self.adb(cmd, v, real_func)
        self.assertTrue(port_obj._check_adb_version())

    def test_too_old(self):
        port_obj = android.AndroidPort()
        real_func = port_obj._run_adb
        v = [1, 0, 29]
        port_obj._run_adb = lambda cmd: self.adb(cmd, v, real_func)
        self.assertFalse(port_obj._check_adb_version())

class AndroidTestSingleDevice(AndroidDeviceTestCase):
    def devices_default(self, cmd):
        self.assertEqual(cmd, ['devices', '-l'])
        return """List of devices attached
015cd4ed14580e03       device usb:2-6 product:prod1 model:Prod1 device:prod1
"""

    def test_guess_serial1(self):
        port_obj = android.AndroidPort()
        port_obj._run_adb = lambda cmd: self.devices_default(cmd)

        os.environ["ANDROID_SERIAL"] = ""
        os.environ["TARGET_PRODUCT"] = "prod1"
        self.assertTrue(port_obj._ensure_target_device_name())
        self.assertEqual(os.environ["ANDROID_SERIAL"], "015cd4ed14580e03")

    def test_guess_serial2(self):
        port_obj = android.AndroidPort()
        port_obj._run_adb = lambda cmd: self.devices_default(cmd)

        os.environ["ANDROID_SERIAL"] = ""
        os.environ["TARGET_PRODUCT"] = ""
        self.assertTrue(port_obj._ensure_target_device_name())
        self.assertEqual(os.environ["ANDROID_SERIAL"], "015cd4ed14580e03")


    def test_guess_serial3(self):
        port_obj = android.AndroidPort()
        port_obj._run_adb = lambda cmd: self.devices_default(cmd)

        os.environ["ANDROID_SERIAL"] = ""
        os.environ["TARGET_PRODUCT"] = "prod2"
        self.assertFalse(port_obj._ensure_target_device_name())
        self.assertEqual(os.environ["ANDROID_SERIAL"], "")


class AndroidTestMultileDevices(AndroidDeviceTestCase):
    def devices_default(self, cmd):
        self.assertEqual(cmd, ['devices', '-l'])

        return """List of devices attached
015cd4ed14580e03       device usb:2-6 product:prod1 model:Prod1 device:prod1
15c20c1041c0000000e0186c0 device usb:2-4.2 product:prod2 model:Prod2 device:prod2
"""

    def test_guess_serial_prod1(self):
        port_obj = android.AndroidPort()
        port_obj._run_adb = lambda cmd: self.devices_default(cmd)
        os.environ["ANDROID_SERIAL"] = ""
        os.environ["TARGET_PRODUCT"] = "prod1"
        self.assertTrue(port_obj._ensure_target_device_name())
        self.assertEqual(os.environ["ANDROID_SERIAL"], "015cd4ed14580e03")

    def test_guess_serial_prod2(self):
        port_obj = android.AndroidPort()
        port_obj._run_adb = lambda cmd: self.devices_default(cmd)
        os.environ["ANDROID_SERIAL"] = ""
        os.environ["TARGET_PRODUCT"] = "prod2"
        self.assertTrue(port_obj._ensure_target_device_name())
        self.assertEqual(os.environ["ANDROID_SERIAL"], "15c20c1041c0000000e0186c0")

    def test_guess_serial_fail_unknown_product(self):
        port_obj = android.AndroidPort()
        port_obj._run_adb = lambda cmd: self.devices_default(cmd)
        os.environ["ANDROID_SERIAL"] = ""
        os.environ["TARGET_PRODUCT"] = "prod3"
        self.assertFalse(port_obj._ensure_target_device_name())
        self.assertEqual(os.environ["ANDROID_SERIAL"], "")

    def test_guess_serial_fail_multiple_devices(self):
        port_obj = android.AndroidPort()
        port_obj._run_adb = lambda cmd: self.devices_default(cmd)
        os.environ["ANDROID_SERIAL"] = ""
        os.environ["TARGET_PRODUCT"] = ""
        self.assertFalse(port_obj._ensure_target_device_name())
        self.assertEqual(os.environ["ANDROID_SERIAL"], "")

    def test_existing_android_serial(self):
        port_obj = android.AndroidPort()
        port_obj._run_adb = lambda cmd: self.devices_default(cmd)
        os.environ["ANDROID_SERIAL"] = "015cd4ed14580e03"
        os.environ["TARGET_PRODUCT"] = "prod1"
        self.assertTrue(port_obj._ensure_target_device_name())
        self.assertEqual(os.environ["ANDROID_SERIAL"], "015cd4ed14580e03")

    def test_existing_android_serial_fail(self):
        port_obj = android.AndroidPort()
        port_obj._run_adb = lambda cmd: self.devices_default(cmd)

        os.environ["ANDROID_SERIAL"] = "015cd4ed14580e03"
        os.environ["TARGET_PRODUCT"] = "prod2"
        self.assertFalse(port_obj._ensure_target_device_name())
        self.assertEqual(os.environ["ANDROID_SERIAL"], "015cd4ed14580e03")

class AndroidTestMultileDevicesSameProduct(AndroidDeviceTestCase):
    def devices_default(self, cmd):
        self.assertEqual(cmd, ['devices', '-l'])

        return """List of devices attached
015cd4ed14580e03       device usb:2-6 product:prod1 model:Prod1 device:prod1
015cd4ed14580e04       device usb:2-36 product:prod1 model:Prod1 device:prod1
15c20c1041c0000000e0186c0 device usb:22-4 product:prod2 model:Prod2 device:prod2
15c20c1041c0000000e0186c1 device usb:11-14 product:prod2 model:Prod2 device:prod2
"""

    def test_existing_android_serial(self):
        port_obj = android.AndroidPort()
        port_obj._run_adb = lambda cmd: self.devices_default(cmd)
        os.environ["ANDROID_SERIAL"] = "015cd4ed14580e03"
        os.environ["TARGET_PRODUCT"] = "prod1"
        self.assertTrue(port_obj._ensure_target_device_name())
        self.assertEqual(os.environ["ANDROID_SERIAL"], "015cd4ed14580e03")

    def test_existing_android_serial_prod2(self):
        port_obj = android.AndroidPort()
        port_obj._run_adb = lambda cmd: self.devices_default(cmd)
        os.environ["ANDROID_SERIAL"] = "15c20c1041c0000000e0186c1"
        os.environ["TARGET_PRODUCT"] = "prod2"
        self.assertTrue(port_obj._ensure_target_device_name())
        self.assertEqual(os.environ["ANDROID_SERIAL"], "15c20c1041c0000000e0186c1")

    def test_existing_android_serial_fail_product_mismatch(self):
        port_obj = android.AndroidPort()
        port_obj._run_adb = lambda cmd: self.devices_default(cmd)
        os.environ["ANDROID_SERIAL"] = "015cd4ed14580e03"
        os.environ["TARGET_PRODUCT"] = "prod2"
        self.assertFalse(port_obj._ensure_target_device_name())
        self.assertEqual(os.environ["ANDROID_SERIAL"], "015cd4ed14580e03")

    def test_fail_multiple_same_products(self):
        port_obj = android.AndroidPort()
        port_obj._run_adb = lambda cmd: self.devices_default(cmd)
        os.environ["ANDROID_SERIAL"] = ""
        os.environ["TARGET_PRODUCT"] = "prod2"
        self.assertFalse(port_obj._ensure_target_device_name())
        self.assertEqual(os.environ["ANDROID_SERIAL"], "")


class AndroidTestAdbLsOutputParse(unittest.TestCase):
    def test_parse(self):
        output="""
        /sdcard/webkit/layout-tests:
drwxrwxr-x root     sdcard_rw          2011-09-20 10:25 dom
drwxrwxr-x root     sdcard_rw          2011-09-20 10:23 fast
drwxrwxr-x root     sdcard_rw          2011-09-20 10:30 http
drwxrwxr-x root     sdcard_rw          2011-09-20 10:21 platform
drwxrwxr-x root     sdcard_rw          2011-09-20 10:22 storage

/sdcard/webkit/layout-tests/dom:
drwxrwxr-x root     sdcard_rw          2011-09-20 10:25 html
drwxrwxr-x root     sdcard_rw          2011-09-20 10:29 xhtml

/sdcard/webkit/layout-tests/dom/html:
drwxrwxr-x root     sdcard_rw          2011-09-20 10:25 level1
drwxrwxr-x root     sdcard_rw          2011-09-20 10:23 level2

/sdcard/webkit/layout-tests/dom/html/level1:
drwxrwxr-x root     sdcard_rw          2011-09-20 10:25 core

/sdcard/webkit/layout-tests/dom/html/level1/core:
-rw-rw-r-- root     sdcard_rw       96 2011-09-06 06:49 documentgetdoctypenodtd-expected.txt
-rw-rw-r-- root     sdcard_rw      598 2011-09-06 06:49 documentgetdoctypenodtd.html
-rw-rw-r-- root     sdcard_rw     2922 2011-09-06 06:49 documentgetdoctypenodtd.js
"""
        def files_not_synched(filename):
            return filename.endswith('-expected.txt')

        files = android.parse_android_lsRl("/sdcard/webkit/layout-tests", output, files_not_synched)
        self.assertEquals(files["dom/html/level1/core/documentgetdoctypenodtd.html"], 1315291799)
        self.assertEquals(files.get("dom/html/level1/core"), None)
        self.assertEquals(files.get("dom/html/level1/core/documentgetdoctypenodtd-expected.txt"), None)

class AndroidTestPaths(AndroidDeviceTestCase):
    def test_result_directory(self):
        os.environ["TARGET_PRODUCT"] = "prod"
        port_obj = android.AndroidPort()

        port_obj.get_option = lambda name: (True if name == "sw_compositing" else None)
        self.assertTrue(port_obj.default_results_directory().endswith("android-sw-prod"))

        port_obj.get_option = lambda name: (False if name == "sw_compositing" else None)
        self.assertTrue(port_obj.default_results_directory().endswith("android-hw-prod"))

    def test_android_platform_dir_cascade(self):
        os.environ["TARGET_PRODUCT"] = "prod"
        port_obj = android.AndroidPort()

        port_obj.get_option = lambda name: (True if name == "sw_compositing" else None)
        dirs = port_obj._android_platform_dir_cascade()
        self.assertEquals(dirs[0], "android-sw-prod")
        self.assertEquals(dirs[1], "android-sw")
        self.assertEquals(dirs[2], "android")
        self.assertEquals(len(dirs), 3)

        port_obj.get_option = lambda name: (False if name == "sw_compositing" else None)
        dirs = port_obj._android_platform_dir_cascade()
        self.assertEquals(dirs[0], "android-prod")
        self.assertEquals(dirs[1], "android")
        self.assertEquals(len(dirs), 2)

class AndroidTestSWCompositingSWForce(unittest.TestCase):
    def test_sw_force(self):
        port_obj = android.AndroidPort(options=mocktool.MockOptions(sw_compositing=True, force_sw_pixel_tests=True))
        self.assertFalse(port_obj.check_build(True))

if __name__ == '__main__':
    unittest.main()
