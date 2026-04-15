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

/**
 * @file test_gdialPlatDev.cpp
 * @brief Unit tests for server/plat/gdial-plat-dev.c
 */

#include <gtest/gtest.h>

extern "C" {
#include <glib.h>
#include "gdial-plat-dev.h"
}

namespace {

static int g_power_cb_calls = 0;
static const char *g_last_power_state = nullptr;
static int g_nw_cb_calls = 0;
static bool g_last_nw_mode = false;

static void reset_cb_state()
{
    g_power_cb_calls = 0;
    g_last_power_state = nullptr;
    g_nw_cb_calls = 0;
    g_last_nw_mode = false;
}

static void power_cb(const char *state)
{
    ++g_power_cb_calls;
    g_last_power_state = state;
}

static void nwstandby_cb(const bool mode)
{
    ++g_nw_cb_calls;
    g_last_nw_mode = mode;
}

class GDialPlatDevTest : public ::testing::Test {
protected:
    void SetUp() override
    {
        reset_cb_state();
        gdail_plat_dev_register_powerstate_cb(nullptr);
        gdail_plat_dev_register_nwstandbymode_cb(nullptr);
    }

    void TearDown() override
    {
        gdail_plat_dev_register_powerstate_cb(nullptr);
        gdail_plat_dev_register_nwstandbymode_cb(nullptr);
    }
};

}  // namespace

TEST_F(GDialPlatDevTest, SetPowerStateOn_ReturnsTrueWithoutCallback)
{
    EXPECT_TRUE(gdial_plat_dev_set_power_state_on());
    EXPECT_EQ(g_power_cb_calls, 0);
}

TEST_F(GDialPlatDevTest, SetPowerStateOff_ReturnsTrueWithoutCallback)
{
    EXPECT_TRUE(gdial_plat_dev_set_power_state_off());
    EXPECT_EQ(g_power_cb_calls, 0);
}

TEST_F(GDialPlatDevTest, TogglePowerState_ReturnsTrueWithoutCallback)
{
    EXPECT_TRUE(gdial_plat_dev_toggle_power_state());
    EXPECT_EQ(g_power_cb_calls, 0);
}

TEST_F(GDialPlatDevTest, SetPowerStateOn_InvokesCallbackWithOn)
{
    gdail_plat_dev_register_powerstate_cb(power_cb);

    EXPECT_TRUE(gdial_plat_dev_set_power_state_on());
    ASSERT_EQ(g_power_cb_calls, 1);
    ASSERT_NE(g_last_power_state, nullptr);
    EXPECT_STREQ(g_last_power_state, "ON");
}

TEST_F(GDialPlatDevTest, SetPowerStateOff_InvokesCallbackWithStandby)
{
    gdail_plat_dev_register_powerstate_cb(power_cb);

    EXPECT_TRUE(gdial_plat_dev_set_power_state_off());
    ASSERT_EQ(g_power_cb_calls, 1);
    ASSERT_NE(g_last_power_state, nullptr);
    EXPECT_STREQ(g_last_power_state, "STANDBY");
}

TEST_F(GDialPlatDevTest, TogglePowerState_InvokesCallbackWithToggle)
{
    gdail_plat_dev_register_powerstate_cb(power_cb);

    EXPECT_TRUE(gdial_plat_dev_toggle_power_state());
    ASSERT_EQ(g_power_cb_calls, 1);
    ASSERT_NE(g_last_power_state, nullptr);
    EXPECT_STREQ(g_last_power_state, "TOGGLE");
}

TEST_F(GDialPlatDevTest, RegisterPowerCallback_NullClearsCallback)
{
    gdail_plat_dev_register_powerstate_cb(power_cb);
    EXPECT_TRUE(gdial_plat_dev_set_power_state_on());
    EXPECT_EQ(g_power_cb_calls, 1);

    gdail_plat_dev_register_powerstate_cb(nullptr);
    EXPECT_TRUE(gdial_plat_dev_set_power_state_on());
    EXPECT_EQ(g_power_cb_calls, 1);
}

TEST_F(GDialPlatDevTest, NwStandbyModeChange_WithoutCallback_NoCrash)
{
    gdial_plat_dev_nwstandby_mode_change(true);
    EXPECT_EQ(g_nw_cb_calls, 0);
}

TEST_F(GDialPlatDevTest, NwStandbyModeChange_InvokesRegisteredCallback)
{
    gdail_plat_dev_register_nwstandbymode_cb(nwstandby_cb);

    gdial_plat_dev_nwstandby_mode_change(true);
    ASSERT_EQ(g_nw_cb_calls, 1);
    EXPECT_TRUE(g_last_nw_mode);

    gdial_plat_dev_nwstandby_mode_change(false);
    ASSERT_EQ(g_nw_cb_calls, 2);
    EXPECT_FALSE(g_last_nw_mode);
}

TEST_F(GDialPlatDevTest, RegisterNwStandbyCallback_NullClearsCallback)
{
    gdail_plat_dev_register_nwstandbymode_cb(nwstandby_cb);
    gdial_plat_dev_nwstandby_mode_change(true);
    EXPECT_EQ(g_nw_cb_calls, 1);

    gdail_plat_dev_register_nwstandbymode_cb(nullptr);
    gdial_plat_dev_nwstandby_mode_change(true);
    EXPECT_EQ(g_nw_cb_calls, 1);
}
