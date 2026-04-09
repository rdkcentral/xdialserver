##########################################################################
# If not stated otherwise in this file or this component's LICENSE
# file the following copyright and licenses apply:
#
# Copyright 2019 RDK Management
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

AC_INIT([xdialserver], [1.0.0])
AM_INIT_AUTOMAKE([-Wall -Werror foreign])

AC_PROG_CC
AC_PROG_CXX
AC_PROG_RANLIB
AM_PROG_AR

# Check for pkg-config
PKG_CHECK_MODULES([GLIB], [glib-2.0])
PKG_CHECK_MODULES([GOBJECT], [gobject-2.0])
PKG_CHECK_MODULES([GIO], [gio-2.0])

# Check for DIAL protocol dependencies
PKG_CHECK_MODULES([GSSDP], [gssdp-1.0], [], 
    [PKG_CHECK_MODULES([GSSDP], [gssdp-1.2], [], 
        [AC_MSG_WARN([gssdp library not found])])])

PKG_CHECK_MODULES([SOUP], [libsoup-2.4], [], 
    [PKG_CHECK_MODULES([SOUP], [libsoup-3.0], [], 
        [AC_MSG_WARN([libsoup library not found])])])

PKG_CHECK_MODULES([XML2], [libxml-2.0])

# Optional RDK-specific packages
PKG_CHECK_MODULES([WPEFRAMEWORK], [WPEFramework], [], 
    [AC_MSG_WARN([WPEFramework development headers not found - platform tests may be limited])])
PKG_CHECK_MODULES([IARMBUS], [libIARMBus], [], 
    [AC_MSG_WARN([libIARMBus development headers not found - IARM tests may be limited])])

# L1 Tests support
AC_ARG_ENABLE([l1tests],
    [AS_HELP_STRING([--enable-l1tests], [Enable L1 unit tests])],
    [enable_l1tests="$enableval"],
    [enable_l1tests="no"])

AM_CONDITIONAL([ENABLE_L1TESTS], [test "x$enable_l1tests" = "xyes"])

if test "x$enable_l1tests" = "xyes"; then
    PKG_CHECK_MODULES([GTEST], [gtest], [], [AC_MSG_WARN([gtest not found in pkg-config])])
    PKG_CHECK_MODULES([GMOCK], [gmock], [], [AC_MSG_WARN([gmock not found in pkg-config])])
fi

AC_CONFIG_FILES([
    Makefile
    tests/Makefile
    tests/L1Tests/Makefile
    tests/L1Tests/mocks/Makefile
])

AC_OUTPUT
