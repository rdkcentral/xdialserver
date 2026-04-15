/*
 * If not stated otherwise in this file or this component's LICENSE file the
 * following copyright and licenses apply:
 *
 * Copyright 2024 RDK Management
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#pragma once

/**
 * @file xdialserver_test_stubs.h
 * @brief Combined test stub declarations for xdialserver unit tests
 *
 * This header aggregates all test stub and mock function declarations
 * needed to test xdialserver components in isolation.
 *
 * Include paths:
 * - CI-generated wrapper headers in tests/L1Tests/stubs/ that include this file
 * - tests/L1Tests/stubs/gdial_cpp_test_stubs.hpp (C++ shim types for gdial.cpp)
 * - tests/L1Tests/mocks/IarmBusMock.h
 */

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @defgroup RestDialStubs REST/DIAL Protocol Stubs
 * @{
 *
 * Stub implementations for libsoup HTTP server and GSSDP/SSDP
 * functionality.
 */

/* Add REST/DIAL stub declarations here */

/**@}*/

/**
 * @defgroup PlatformStubs Platform-Specific Stubs
 * @{
 *
 * Stub implementations for WPEFramework, device discovery, and
 * platform-specific functionality.
 */

/* Add platform stub declarations here */

/**@}*/

/**
 * @defgroup UtilityStubs Utility Function Stubs
 * @{
 *
 * Stub implementations for utility functions (string manipulation,
 * config parsing, etc.)
 */

/* Add utility stub declarations here */

/**@}*/

/**
 * @defgroup IarmBusMocks IARM Bus Mocks
 * @{
 *
 * Mock implementations of IARM bus functionality for testing
 * without the full IARM daemon.
 */

/* Add IARM bus mock declarations here */

/**@}*/

#ifdef __cplusplus
}
#endif
