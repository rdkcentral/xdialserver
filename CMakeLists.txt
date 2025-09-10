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

# Top-level CMakeLists.txt for xdialserver
cmake_minimum_required(VERSION 3.10)
project(xdialserver)

# Pass platform flags and security token to server subdirectory
if(NOT DEFINED PLATFORM)
	set(PLATFORM "-DNETFLIX_CALLSIGN_0=1" CACHE STRING "Platform flags")
endif()
if(NOT DEFINED DISABLE_SECURITY_TOKEN)
	set(DISABLE_SECURITY_TOKEN "-DDISABLE_SECURITY_TOKEN=1" CACHE STRING "Disable security token")
endif()
add_subdirectory(server)
