#
# Copyright (C) 2010 The Android Open Source Project
# Copyright (c) 2013 NVIDIA CORPORATION.
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#      http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.
#

# Makefile to export layout tests, layout test results and layout test
# runners to the test package.
# FIXME: exports everything always. The intention would be to export only
# when needed, eg when making "nvidia-tests" target.
LOCAL_PATH := $(call my-dir)
include $(NVIDIA_DEFAULTS)

LOCAL_MODULE := libwebcore_layout_tests
LOCAL_MODULE_CLASS := FAKE
LOCAL_MODULE_TAGS := nvidia_tests tests
LOCAL_UNINSTALLABLE_MODULE := true

# Include every file in layout test dirs except the ones we don't have
# use for.  Also don't list files with spaces in the file name for the
# dependecy resolution, as make can not really handle such
# files. Trust that such files are rarely modified and they're copied
# as part of some directory copy command.
WEBKIT_LAYOUT_TESTS_PRUNES := -regex '.*LayoutTests/platform/chromium.*' -prune -o -name '* *' -prune

WEBKIT_PATH := $(LOCAL_PATH)/../../..
WEBKIT_TARGET_PATH := $(PRODUCT_OUT)/nvidia_tests/webkit

TESTS_TIMESTAMP := $(WEBKIT_TARGET_PATH)/LayoutTests/dom/html/level1/core/documentgetdoctypenodtd.html

$(TESTS_TIMESTAMP): $(shell find $(WEBKIT_PATH)/LayoutTests $(WEBKIT_LAYOUT_TESTS_PRUNES) -o -type f -a -print) | $(ACP)
	@mkdir -p $(WEBKIT_TARGET_PATH)
	@$(ACP) -pr $(WEBKIT_PATH)/LayoutTests $(WEBKIT_TARGET_PATH)/LayoutTests # FIXME: copies too much.

WEBKIT_SCRIPTS := $(WEBKIT_PATH)/Tools/Scripts/VCSUtils.pm \
	$(WEBKIT_PATH)/Tools/Scripts/new-run-webkit-httpd \
	$(WEBKIT_PATH)/Tools/Scripts/new-run-webkit-tests \
	$(WEBKIT_PATH)/Tools/Scripts/new-run-webkit-websocketserver \
	$(WEBKIT_PATH)/Tools/Scripts/old-run-webkit-tests \
	$(WEBKIT_PATH)/Tools/Scripts/webkitdirs.pm \
	$(WEBKIT_PATH)/Tools/Scripts/webkitperl \
	$(WEBKIT_PATH)/Tools/Scripts/webkitpy \
	$(WEBKIT_PATH)/Tools/Scripts/run-webkit-tests

SCRIPTS_TIMESTAMP := $(WEBKIT_TARGET_PATH)/Tools/Scripts/VCSUtils.pm

$(SCRIPTS_TIMESTAMP): $(shell find $(WEBKIT_SCRIPTS)) | $(ACP)
	@mkdir -p $(WEBKIT_TARGET_PATH)/Tools/Scripts
	@$(ACP) -pr $(WEBKIT_SCRIPTS) $(WEBKIT_TARGET_PATH)/Tools/Scripts #FIXME: copies too much.

$(LOCAL_MODULE): $(TESTS_TIMESTAMP) $(SCRIPTS_TIMESTAMP)

include $(NVIDIA_BASE)
include $(BUILD_PHONY_PACKAGE)
