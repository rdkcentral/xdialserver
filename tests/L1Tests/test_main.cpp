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

/**
 * @file test_main.cpp
 * @brief Main entry point for the xdialserver L1 unit test runner
 *
 * Initializes Google Test and Google Mock, then runs all registered test suites.
 * Individual component tests live under subdirectories (e.g. server/, plat/, utils/)
 * and are compiled into this single binary.
 */

#include <gtest/gtest.h>
#include <gmock/gmock.h>

/**
 * Main test entry point
 * Initializes the test framework and runs all registered tests
 */
int main(int argc, char **argv) {
    ::testing::InitGoogleMock(&argc, argv);
    return RUN_ALL_TESTS();
}
