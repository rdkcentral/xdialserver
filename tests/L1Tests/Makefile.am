##########################################################################
# If not stated otherwise in this file or this component's LICENSE
# file the following copyright and licenses apply:
#
# Copyright 2024 RDK Management
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
# http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.
##########################################################################

# Include paths:
#   - tests/L1Tests/stubs        : shadow headers that redirect to test stubs
#   - tests/L1Tests/mocks        : shared mock implementations
#   - tests/mocks/xdialserver    : xdialserver-specific mocks
#   - server/include              : xdialserver public API headers
#   - server                      : xdialserver implementation headers
#   - server/plat                 : platform-specific headers
#
# Note: WPEFramework and IARM bus headers are optional and mocked in the test
# environment. The test stubs provide mock implementations, so the actual
# development packages (libwpeframework-dev, libiarmbus-dev) are not required
# for basic testing.

AM_CPPFLAGS = -I$(top_srcdir)/tests/L1Tests/stubs \
	      -I$(top_srcdir)/tests/L1Tests/mocks \
	      -I$(top_srcdir)/tests/mocks/xdialserver \
	      -I$(top_srcdir)/server/include \
	      -I$(top_srcdir)/server \
	      -I$(top_srcdir)/server/plat \
	      $(GLIB_CFLAGS) \
	      $(GIO_CFLAGS) \
	      $(GSSDP_CFLAGS) \
	      $(SOUP_CFLAGS) \
	      $(XML2_CFLAGS)

AM_CXXFLAGS = -Wall -std=c++14 -g

AM_LDFLAGS = -pthread

# ---------- Test binary ----------
noinst_PROGRAMS = run_L1Tests

run_L1Tests_SOURCES = \
	test_main.cpp \
	server/test_gdialServer.cpp \
	plat/test_gdialPlat.cpp \
	utils/test_gdialUtil.cpp \
	stubs/xdialserver_test_stubs.cpp \
	plat/gdial_plat_stubs.cpp \
	$(top_srcdir)/server/gdial-util.c \
	$(top_srcdir)/server/gdial-app.c \
	$(top_srcdir)/server/plat/gdial_app_registry.c

run_L1Tests_LDADD = -lgtest -lgmock $(GLIB_LIBS) $(GIO_LIBS) $(XML2_LIBS) -luuid

# Run tests on 'make check'
TESTS = run_L1Tests

# Clean up test artifacts
CLEANFILES = *.log *.trs

SUBDIRS = mocks
