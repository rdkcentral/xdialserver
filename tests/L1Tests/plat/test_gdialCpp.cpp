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

#include <map>
#include <string>
#include <vector>

extern "C" {
#include "gdial-plat-dev.h"
}

#include "gdialservicecommon.h"

/*
 * Pull real gdial.cpp into this test TU, but rename exported symbols to avoid
 * collisions with gdial_os_stubs.cpp used by the rest of the L1 suite.
 */
#define gdial_register_activation_cb gdial_cpp_test_register_activation_cb
#define gdial_register_friendlyname_cb gdial_cpp_test_register_friendlyname_cb
#define gdial_register_registerapps_cb gdial_cpp_test_register_registerapps_cb
#define gdial_register_manufacturername_cb gdial_cpp_test_register_manufacturername_cb
#define gdial_register_modelname_cb gdial_cpp_test_register_modelname_cb
#define gdial_init gdial_cpp_test_init
#define gdial_term gdial_cpp_test_term
#define parse_query gdial_cpp_test_parse_query
#define gdial_os_application_start gdial_cpp_test_os_application_start
#define gdial_os_application_stop gdial_cpp_test_os_application_stop
#define gdial_os_application_hide gdial_cpp_test_os_application_hide
#define gdial_os_application_resume gdial_cpp_test_os_application_resume
#define gdial_os_application_state gdial_cpp_test_os_application_state
#define gdial_os_application_state_changed gdial_cpp_test_os_application_state_changed
#define gdial_os_application_activation_changed gdial_cpp_test_os_application_activation_changed
#define gdial_os_application_friendlyname_changed gdial_cpp_test_os_application_friendlyname_changed
#define gdial_os_application_get_protocol_version gdial_cpp_test_os_application_get_protocol_version
#define gdial_os_application_register_applications gdial_cpp_test_os_application_register_applications
#define gdial_os_application_update_network_standby_mode gdial_cpp_test_os_application_update_network_standby_mode
#define gdial_os_application_update_manufacturer_name gdial_cpp_test_os_application_update_manufacturer_name
#define gdial_os_application_update_model_name gdial_cpp_test_os_application_update_model_name
#define gdial_os_application_service_notification gdial_cpp_test_os_application_service_notification
#include "../../../server/plat/gdial.cpp"
#undef gdial_register_activation_cb
#undef gdial_register_friendlyname_cb
#undef gdial_register_registerapps_cb
#undef gdial_register_manufacturername_cb
#undef gdial_register_modelname_cb
#undef gdial_init
#undef gdial_term
#undef parse_query
#undef gdial_os_application_start
#undef gdial_os_application_stop
#undef gdial_os_application_hide
#undef gdial_os_application_resume
#undef gdial_os_application_state
#undef gdial_os_application_state_changed
#undef gdial_os_application_activation_changed
#undef gdial_os_application_friendlyname_changed
#undef gdial_os_application_get_protocol_version
#undef gdial_os_application_register_applications
#undef gdial_os_application_update_network_standby_mode
#undef gdial_os_application_update_manufacturer_name
#undef gdial_os_application_update_model_name
#undef gdial_os_application_service_notification

namespace {

static int g_power_cb_calls = 0;
static std::string g_last_power_state;

static void test_power_cb(const char *state)
{
    ++g_power_cb_calls;
    g_last_power_state = state ? state : "";
}

class DummyNotifier : public GDialNotifier {
public:
    int launch_calls = 0;

    void onApplicationLaunchRequest(std::string, std::string) override {}
    void onApplicationLaunchRequestWithLaunchParam(std::string, std::string, std::string, std::string) override
    {
        ++launch_calls;
    }
    void onApplicationStopRequest(std::string, std::string) override {}
    void onApplicationHideRequest(std::string, std::string) override {}
    void onApplicationResumeRequest(std::string, std::string) override {}
    void onApplicationStateRequest(std::string, std::string) override {}
    void updatePowerState(std::string) override {}
};

class GDialCppTest : public ::testing::Test {
protected:
    GMainContext *ctx = nullptr;

    void SetUp() override
    {
        ctx = g_main_context_new();
        g_power_cb_calls = 0;
        g_last_power_state.clear();
        gdail_plat_dev_register_powerstate_cb(test_power_cb);
    }

    void TearDown() override
    {
        gdail_plat_dev_register_powerstate_cb(nullptr);
        gdial_cpp_test_term();
        if (ctx) {
            g_main_context_unref(ctx);
            ctx = nullptr;
        }
    }
};

} // namespace

TEST_F(GDialCppTest, ParseQuery_NullReturnsEmptyMap)
{
    std::map<std::string, std::string> out = gdial_cpp_test_parse_query(nullptr);
    EXPECT_TRUE(out.empty());
}

TEST_F(GDialCppTest, ParseQuery_ValidKeyValuePairs)
{
    std::map<std::string, std::string> out = gdial_cpp_test_parse_query("action=sleep&key=abc");
    ASSERT_EQ(out.size(), 2u);
    EXPECT_EQ(out["action"], "sleep");
    EXPECT_EQ(out["key"], "abc");
}

TEST_F(GDialCppTest, ParseQuery_InvalidEscapeFallsBackToRaw)
{
    std::map<std::string, std::string> out = gdial_cpp_test_parse_query("k=100%bad");
    ASSERT_EQ(out.size(), 1u);
    EXPECT_EQ(out["k"], "100%bad");
}

TEST_F(GDialCppTest, InitAndTerm_Idempotent)
{
    EXPECT_TRUE(gdial_cpp_test_init(ctx));
    EXPECT_TRUE(gdial_cpp_test_init(ctx));
    gdial_cpp_test_term();
    /* Second term after cleanup is also safe. */
    gdial_cpp_test_term();
}

TEST_F(GDialCppTest, OsGetProtocolVersion_WhenUninitializedReturnsDefault)
{
    const char *ver = gdial_cpp_test_os_application_get_protocol_version();
    ASSERT_NE(ver, nullptr);
    EXPECT_STREQ(ver, GDIAL_PROTOCOL_VERSION_STR);
}

TEST_F(GDialCppTest, OsServiceNotification_UninitializedReturnsInternal)
{
    DummyNotifier n;
    EXPECT_EQ(gdial_cpp_test_os_application_service_notification(TRUE, &n), GDIAL_APP_ERROR_INTERNAL);
}

TEST_F(GDialCppTest, OsServiceNotification_AfterInitReturnsNone)
{
    DummyNotifier n;
    ASSERT_TRUE(gdial_cpp_test_init(ctx));
    EXPECT_EQ(gdial_cpp_test_os_application_service_notification(TRUE, &n), GDIAL_APP_ERROR_NONE);
    EXPECT_EQ(gdial_cpp_test_os_application_service_notification(FALSE, nullptr), GDIAL_APP_ERROR_NONE);
}

TEST_F(GDialCppTest, OsUpdateManufacturerName_NullAndUninitializedReturnInternal)
{
    EXPECT_EQ(gdial_cpp_test_os_application_update_manufacturer_name(nullptr), GDIAL_APP_ERROR_INTERNAL);
    EXPECT_EQ(gdial_cpp_test_os_application_update_manufacturer_name("Acme"), GDIAL_APP_ERROR_INTERNAL);
}

TEST_F(GDialCppTest, OsUpdateModelName_NullAndUninitializedReturnInternal)
{
    EXPECT_EQ(gdial_cpp_test_os_application_update_model_name(nullptr), GDIAL_APP_ERROR_INTERNAL);
    EXPECT_EQ(gdial_cpp_test_os_application_update_model_name("ModelX"), GDIAL_APP_ERROR_INTERNAL);
}

TEST_F(GDialCppTest, OsUpdateManufacturerAndModel_AfterInitReturnNone)
{
    ASSERT_TRUE(gdial_cpp_test_init(ctx));
    EXPECT_EQ(gdial_cpp_test_os_application_update_manufacturer_name("Acme"), GDIAL_APP_ERROR_NONE);
    EXPECT_EQ(gdial_cpp_test_os_application_update_model_name("ModelX"), GDIAL_APP_ERROR_NONE);
}

TEST_F(GDialCppTest, OsApplicationStart_SystemSleepTriggersPowerOff)
{
    ASSERT_TRUE(gdial_cpp_test_init(ctx));
    int instance_id = 0;
    EXPECT_EQ(gdial_cpp_test_os_application_start("system", "", "action=sleep", "", &instance_id), GDIAL_APP_ERROR_NONE);
    EXPECT_EQ(g_power_cb_calls, 1);
    EXPECT_EQ(g_last_power_state, "STANDBY");
}

TEST_F(GDialCppTest, OsApplicationStart_SystemTogglePowerTriggersToggle)
{
    ASSERT_TRUE(gdial_cpp_test_init(ctx));
    int instance_id = 0;
    EXPECT_EQ(gdial_cpp_test_os_application_start("system", "", "action=togglepower", "", &instance_id), GDIAL_APP_ERROR_NONE);
    EXPECT_EQ(g_power_cb_calls, 1);
    EXPECT_EQ(g_last_power_state, "TOGGLE");
}

TEST_F(GDialCppTest, OsApplicationState_SystemReturnsHide)
{
    GDialAppState state = GDIAL_APP_STATE_MAX;
    EXPECT_EQ(gdial_cpp_test_os_application_state("system", 1, &state), GDIAL_APP_ERROR_NONE);
    EXPECT_EQ(state, GDIAL_APP_STATE_HIDE);
}

TEST_F(GDialCppTest, OsApplicationActivationChanged_UninitializedReturnsInternal)
{
    EXPECT_EQ(gdial_cpp_test_os_application_activation_changed("true", "TV"), GDIAL_APP_ERROR_INTERNAL);
}

TEST_F(GDialCppTest, OsApplicationFriendlyNameChanged_UninitializedReturnsInternal)
{
    EXPECT_EQ(gdial_cpp_test_os_application_friendlyname_changed("LivingRoom"), GDIAL_APP_ERROR_INTERNAL);
}

TEST_F(GDialCppTest, OsApplicationStateChanged_UninitializedReturnsInternal)
{
    EXPECT_EQ(gdial_cpp_test_os_application_state_changed("App", "id", "running", "none"), GDIAL_APP_ERROR_INTERNAL);
}

TEST_F(GDialCppTest, OsApplicationRegisterApplications_UninitializedReturnsInternal)
{
    auto *list = new RegisterAppEntryList;
    EXPECT_EQ(gdial_cpp_test_os_application_register_applications(list), GDIAL_APP_ERROR_INTERNAL);
    /* Uninitialized path does not consume the list. */
    delete list;
}
