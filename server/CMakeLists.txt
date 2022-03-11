##########################################################################
# If not stated otherwise in this file or this component's Licenses.txt
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

cmake_minimum_required (VERSION 3.5)
project (gdial)

set (GDIAL_VERSION_MAJOR 1)
set (GDIAL_VERSION_MINOR 0)

find_package (PkgConfig REQUIRED)
pkg_search_module (GLIB REQUIRED glib-2.0)
pkg_search_module (GIO REQUIRED gio-2.0)
pkg_search_module (GSSDP REQUIRED gssdp-1.0)
pkg_search_module (SOUP REQUIRED libsoup-2.4)
pkg_search_module (XML2 REQUIRED libxml-2.0)
pkg_search_module (JSON-C REQUIRED json-c)

set (CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wall -g")
set (CMAKE_C_FLAGS_DEBUG "${CMAKE_C_FLAGS} ")
set (CMAKE_C_FLAGS_RELEASE "${CMAKE_C_FLAGS} ")

include (${CMAKE_ROOT}/Modules/CheckLibraryExists.cmake)
include (${CMAKE_ROOT}/Modules/CheckSymbolExists.cmake)
include (${CMAKE_ROOT}/Modules/CheckFunctionExists.cmake)
include (${CMAKE_ROOT}/Modules/TestBigEndian.cmake)

set (CMAKE_VERBOSE_MAKEFILE ON)
check_function_exists (malloc HAVE_MALLOC)

add_definitions(-DGDIAL_STATIC=extern -DGDIAL_STATIC_INLINE=extern)

#add_subdirectory (plat) that builds libgdial-plat.so
add_subdirectory (${PROJECT_SOURCE_DIR}/plat)

include_directories (
  ${GLIB_INCLUDE_DIRS}
  ${GSSDP_INCLUDE_DIRS}
  ${SOUP_INCLUDE_DIRS}
  ${XML2_INCLUDE_DIRS}
)

include_directories (
  ${PROJECT_SOURCE_DIR}/include
  ${PROJECT_SOURCE_DIR}
  ${PROJECT_GLIB_INCLUDE_DIRS}
)

set (GDIAL_EXEC_SOURCE_FILES
  ${CMAKE_CURRENT_SOURCE_DIR}/gdial-util.c
  ${CMAKE_CURRENT_SOURCE_DIR}/gdial-app.c
  ${CMAKE_CURRENT_SOURCE_DIR}/gdial-rest.c
  ${CMAKE_CURRENT_SOURCE_DIR}/gdial-ssdp.c
  ${CMAKE_CURRENT_SOURCE_DIR}/gdial-shield.c
  ${CMAKE_CURRENT_SOURCE_DIR}/main.c
)

link_directories (
  ${GLIB_LIBRARY_DIRS}
  ${GSSDP_LIBRARY_DIRS}
  ${SOUP_LIBRARY_DIRS}
)

add_executable (gdial-server ${GDIAL_EXEC_SOURCE_FILES})

target_link_libraries (gdial-server
  ${GLIB_LIBRARIES}
  ${GIO_LIBRARIES}
  ${GSSDP_LIBRARIES}
  ${SOUP_LIBRARIES}
  ${XML2_LIBRARIES}
  ${JSON-C_LIBRARIES}
  gdial-plat
)
