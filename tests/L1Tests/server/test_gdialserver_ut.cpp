/*
 * If not stated otherwise in this file or this component's LICENSE file the
 * following copyright and licenses apply:
 *
 * Copyright 2026 RDK Management
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

#include <gtest/gtest.h>

#include <sstream>
#include <string>
#include <vector>

#include "gdialservice.h"

namespace {

int g_get_instance_calls = 0;
int g_destroy_instance_calls = 0;
int g_activation_calls = 0;
int g_register_calls = 0;
std::vector<std::string> g_activation_values;
std::vector<std::string> g_activation_friendly_names;
std::vector<size_t> g_register_sizes;

void reset_stub_state() {
    g_get_instance_calls = 0;
    g_destroy_instance_calls = 0;
    g_activation_calls = 0;
    g_register_calls = 0;
    g_activation_values.clear();
    g_activation_friendly_names.clear();
    g_register_sizes.clear();
}

}  // namespace

gdialService* gdialService::getInstance(GDialNotifier* observer, const std::vector<std::string>& gdial_args, const std::string& actualprocessName) {
    (void)observer;
    (void)gdial_args;
    (void)actualprocessName;
    ++g_get_instance_calls;
    return reinterpret_cast<gdialService*>(0x1);
}

void gdialService::destroyInstance() {
    ++g_destroy_instance_calls;
}

GDIAL_SERVICE_ERROR_CODES gdialService::ApplicationStateChanged(std::string applicationName, std::string appState, std::string applicationId, std::string error) {
    (void)applicationName;
    (void)appState;
    (void)applicationId;
    (void)error;
    return GDIAL_SERVICE_ERROR_NONE;
}

GDIAL_SERVICE_ERROR_CODES gdialService::ActivationChanged(std::string activation, std::string friendlyname) {
    ++g_activation_calls;
    g_activation_values.push_back(activation);
    g_activation_friendly_names.push_back(friendlyname);
    return GDIAL_SERVICE_ERROR_NONE;
}

GDIAL_SERVICE_ERROR_CODES gdialService::FriendlyNameChanged(std::string friendlyname) {
    (void)friendlyname;
    return GDIAL_SERVICE_ERROR_NONE;
}

std::string gdialService::getProtocolVersion(void) {
    return "2.2";
}

GDIAL_SERVICE_ERROR_CODES gdialService::RegisterApplications(RegisterAppEntryList* appConfigList) {
    ++g_register_calls;
    if (appConfigList) {
        g_register_sizes.push_back(appConfigList->getValues().size());
        delete appConfigList;
    }
    else {
        g_register_sizes.push_back(0);
    }
    return GDIAL_SERVICE_ERROR_NONE;
}

void gdialService::setNetworkStandbyMode(bool nwStandbymode) {
    (void)nwStandbymode;
}

GDIAL_SERVICE_ERROR_CODES gdialService::setManufacturerName(std::string manufacturer) {
    (void)manufacturer;
    return GDIAL_SERVICE_ERROR_NONE;
}

GDIAL_SERVICE_ERROR_CODES gdialService::setModelName(std::string model) {
    (void)model;
    return GDIAL_SERVICE_ERROR_NONE;
}

#define main gdialserver_ut_main
#include "../../../server/gdialserver_ut.cpp"
#undef main

class GDialServerUTMainTest : public ::testing::Test {
protected:
    void SetUp() override {
        reset_stub_state();
        running = true;
    }

    void TearDown() override {
        running = true;
    }

    int run_main_with_input(const std::string& input, int argc = 1, char** argv = nullptr) {
        std::istringstream input_stream(input);
        std::streambuf* old_buf = std::cin.rdbuf(input_stream.rdbuf());

        char default_arg0[] = "gdialserver_ut";
        char* default_argv[] = { default_arg0, nullptr };
        char** use_argv = argv ? argv : default_argv;

        int ret = gdialserver_ut_main(argc, use_argv);

        std::cin.rdbuf(old_buf);
        return ret;
    }
};

TEST_F(GDialServerUTMainTest, SignalHandlerStopsRunningLoop) {
    running = true;
    signalHandler(SIGINT);
    EXPECT_FALSE(running.load());
}

TEST_F(GDialServerUTMainTest, MainQuitCommandExitsAndCleansUp) {
    int ret = run_main_with_input("q\n");

    EXPECT_EQ(ret, 0);
    EXPECT_EQ(g_get_instance_calls, 1);
    EXPECT_EQ(g_destroy_instance_calls, 1);
    EXPECT_EQ(g_activation_calls, 0);
    EXPECT_EQ(g_register_calls, 0);
}

TEST_F(GDialServerUTMainTest, MainEnableDisableRegisterRestartFlow) {
    int ret = run_main_with_input("enable\ndisable\nregister\nrestart\nq\n");

    EXPECT_EQ(ret, 0);
    EXPECT_EQ(g_get_instance_calls, 2);
    EXPECT_EQ(g_destroy_instance_calls, 2);
    EXPECT_EQ(g_activation_calls, 2);
    EXPECT_EQ(g_register_calls, 1);

    ASSERT_EQ(g_activation_values.size(), 2u);
    EXPECT_EQ(g_activation_values[0], "true");
    EXPECT_EQ(g_activation_values[1], "false");

    ASSERT_EQ(g_activation_friendly_names.size(), 2u);
    EXPECT_EQ(g_activation_friendly_names[0], "SampleTest");
    EXPECT_EQ(g_activation_friendly_names[1], "SampleTest");

    ASSERT_EQ(g_register_sizes.size(), 1u);
    EXPECT_EQ(g_register_sizes[0], 9u);
}
