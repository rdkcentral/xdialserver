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

#include <cstdlib>
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
static int g_manufacturer_cb_calls = 0;
static int g_model_cb_calls = 0;
static int g_activation_cb_calls = 0;
static bool g_last_activation_state = false;
static std::string g_last_activation_friendlyname;
static int g_friendlyname_cb_calls = 0;
static std::string g_last_friendlyname;
static int g_registerapps_cb_calls = 0;
static gpointer g_last_registerapps_payload = nullptr;
static int g_nwstandby_cb_calls = 0;
static bool g_last_nwstandby_mode = false;

static void test_power_cb(const char *state)
{
    ++g_power_cb_calls;
    g_last_power_state = state ? state : "";
}

static void test_manufacturer_cb(const char *)
{
    ++g_manufacturer_cb_calls;
}

static void test_model_cb(const char *)
{
    ++g_model_cb_calls;
}

static void test_activation_cb(bool state, const gchar *friendlyname)
{
    ++g_activation_cb_calls;
    g_last_activation_state = state;
    g_last_activation_friendlyname = friendlyname ? friendlyname : "";
}

static void test_friendlyname_cb(const gchar *friendlyname)
{
    ++g_friendlyname_cb_calls;
    g_last_friendlyname = friendlyname ? friendlyname : "";
}

static void test_registerapps_cb(gpointer payload)
{
    ++g_registerapps_cb_calls;
    g_last_registerapps_payload = payload;
}

static void test_nwstandby_cb(const bool mode)
{
    ++g_nwstandby_cb_calls;
    g_last_nwstandby_mode = mode;
}

class DummyNotifier : public GDialNotifier {
public:
    int launch_calls = 0;
    int launch_with_params_calls = 0;
    int stop_calls = 0;
    int hide_calls = 0;
    int resume_calls = 0;
    int state_calls = 0;

    void onApplicationLaunchRequest(std::string, std::string) override
    {
        ++launch_calls;
    }
    void onApplicationLaunchRequestWithLaunchParam(std::string, std::string, std::string, std::string) override
    {
        ++launch_with_params_calls;
    }
    void onApplicationStopRequest(std::string, std::string) override
    {
        ++stop_calls;
    }
    void onApplicationHideRequest(std::string, std::string) override
    {
        ++hide_calls;
    }
    void onApplicationResumeRequest(std::string, std::string) override
    {
        ++resume_calls;
    }
    void onApplicationStateRequest(std::string, std::string) override
    {
        ++state_calls;
    }
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
        g_manufacturer_cb_calls = 0;
        g_model_cb_calls = 0;
        g_activation_cb_calls = 0;
        g_last_activation_state = false;
        g_last_activation_friendlyname.clear();
        g_friendlyname_cb_calls = 0;
        g_last_friendlyname.clear();
        g_registerapps_cb_calls = 0;
        g_last_registerapps_payload = nullptr;
        g_nwstandby_cb_calls = 0;
        g_last_nwstandby_mode = false;

        unsetenv("SYSTEM_SLEEP_REQUEST_KEY");
        unsetenv("ENABLE_NETFLIX_STOP");

        gdail_plat_dev_register_powerstate_cb(test_power_cb);
        gdail_plat_dev_register_nwstandbymode_cb(test_nwstandby_cb);

        gdial_cpp_test_register_activation_cb(nullptr);
        gdial_cpp_test_register_friendlyname_cb(nullptr);
        gdial_cpp_test_register_registerapps_cb(nullptr);
        gdial_cpp_test_register_manufacturername_cb(nullptr);
        gdial_cpp_test_register_modelname_cb(nullptr);
    }

    void TearDown() override
    {
        gdail_plat_dev_register_powerstate_cb(nullptr);
        gdail_plat_dev_register_nwstandbymode_cb(nullptr);

        gdial_cpp_test_register_activation_cb(nullptr);
        gdial_cpp_test_register_friendlyname_cb(nullptr);
        gdial_cpp_test_register_registerapps_cb(nullptr);
        gdial_cpp_test_register_manufacturername_cb(nullptr);
        gdial_cpp_test_register_modelname_cb(nullptr);

        unsetenv("SYSTEM_SLEEP_REQUEST_KEY");
        unsetenv("ENABLE_NETFLIX_STOP");

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
    std::map<std::string, std::string> out = gdial_cpp_test_parse_query("k=100%");
    ASSERT_EQ(out.size(), 1u);
    EXPECT_EQ(out["k"], "100%");
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
    EXPECT_EQ(gdial_cpp_test_os_application_update_manufacturer_name(nullptr), GDIAL_CAST_ERROR_INTERNAL);
    EXPECT_EQ(gdial_cpp_test_os_application_update_manufacturer_name("Acme"), GDIAL_CAST_ERROR_INTERNAL);
}

TEST_F(GDialCppTest, OsUpdateModelName_NullAndUninitializedReturnInternal)
{
    EXPECT_EQ(gdial_cpp_test_os_application_update_model_name(nullptr), GDIAL_CAST_ERROR_INTERNAL);
    EXPECT_EQ(gdial_cpp_test_os_application_update_model_name("ModelX"), GDIAL_CAST_ERROR_INTERNAL);
}

TEST_F(GDialCppTest, OsUpdateManufacturerAndModel_AfterInitReturnNone)
{
    ASSERT_TRUE(gdial_cpp_test_init(ctx));
    gdial_cpp_test_register_manufacturername_cb(test_manufacturer_cb);
    gdial_cpp_test_register_modelname_cb(test_model_cb);

    EXPECT_EQ(gdial_cpp_test_os_application_update_manufacturer_name("Acme"), GDIAL_CAST_ERROR_NONE);
    EXPECT_EQ(gdial_cpp_test_os_application_update_model_name("ModelX"), GDIAL_CAST_ERROR_NONE);
    EXPECT_EQ(g_manufacturer_cb_calls, 1);
    EXPECT_EQ(g_model_cb_calls, 1);
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

TEST_F(GDialCppTest, OsApplicationStart_SystemSleepWithWrongKeyReturnsInternal)
{
    ASSERT_TRUE(gdial_cpp_test_init(ctx));
    setenv("SYSTEM_SLEEP_REQUEST_KEY", "expected", 1);
    int instance_id = 0;
    EXPECT_EQ(gdial_cpp_test_os_application_start("system", "", "action=sleep&key=wrong", "", &instance_id),
              GDIAL_APP_ERROR_INTERNAL);
}

TEST_F(GDialCppTest, OsApplicationStart_SystemToggleWithWrongKeyReturnsInternal)
{
    ASSERT_TRUE(gdial_cpp_test_init(ctx));
    setenv("SYSTEM_SLEEP_REQUEST_KEY", "expected", 1);
    int instance_id = 0;
    EXPECT_EQ(gdial_cpp_test_os_application_start("system", "", "action=togglepower&key=wrong", "", &instance_id),
              GDIAL_APP_ERROR_INTERNAL);
}

TEST_F(GDialCppTest, OsApplicationStart_NonSystemWithNotifierLaunchesWithParams)
{
    DummyNotifier n;
    ASSERT_TRUE(gdial_cpp_test_init(ctx));
    ASSERT_EQ(gdial_cpp_test_os_application_service_notification(TRUE, &n), GDIAL_APP_ERROR_NONE);

    int instance_id = 0;
    EXPECT_EQ(gdial_cpp_test_os_application_start("Netflix", "payload", "k=v", "url", &instance_id),
              GDIAL_APP_ERROR_NONE);
    EXPECT_EQ(n.launch_with_params_calls, 1);
}

TEST_F(GDialCppTest, OsApplicationStateChanged_InitializedReturnsNoneAndUpdatesState)
{
    ASSERT_TRUE(gdial_cpp_test_init(ctx));

    EXPECT_EQ(gdial_cpp_test_os_application_state_changed("App", "id", "running", "none"), GDIAL_APP_ERROR_NONE);

    GDialAppState state = GDIAL_APP_STATE_MAX;
    EXPECT_EQ(gdial_cpp_test_os_application_state("App", 1, &state), GDIAL_APP_ERROR_NONE);
    EXPECT_EQ(state, GDIAL_APP_STATE_RUNNING);
}

TEST_F(GDialCppTest, OsApplicationState_MapsHiddenAndStopped)
{
    ASSERT_TRUE(gdial_cpp_test_init(ctx));

    EXPECT_EQ(gdial_cpp_test_os_application_state_changed("App", "id", "hidden", "none"), GDIAL_APP_ERROR_NONE);
    GDialAppState state = GDIAL_APP_STATE_MAX;
    EXPECT_EQ(gdial_cpp_test_os_application_state("App", 1, &state), GDIAL_APP_ERROR_NONE);
    EXPECT_EQ(state, GDIAL_APP_STATE_HIDE);

    EXPECT_EQ(gdial_cpp_test_os_application_state_changed("App", "id", "stopped", "none"), GDIAL_APP_ERROR_NONE);
    EXPECT_EQ(gdial_cpp_test_os_application_state("App", 1, &state), GDIAL_APP_ERROR_NONE);
    EXPECT_EQ(state, GDIAL_APP_STATE_STOPPED);
}

TEST_F(GDialCppTest, OsApplicationHideResumeStop_NonSystemPaths)
{
    DummyNotifier n;
    ASSERT_TRUE(gdial_cpp_test_init(ctx));
    ASSERT_EQ(gdial_cpp_test_os_application_service_notification(TRUE, &n), GDIAL_APP_ERROR_NONE);

    /* Running -> hide succeeds and notifies observer. */
    ASSERT_EQ(gdial_cpp_test_os_application_state_changed("App", "id", "running", "none"), GDIAL_APP_ERROR_NONE);
    EXPECT_EQ(gdial_cpp_test_os_application_hide("App", 7), GDIAL_APP_ERROR_NONE);
    EXPECT_EQ(n.hide_calls, 1);

    /* Running -> resume returns bad request per implementation. */
    EXPECT_EQ(gdial_cpp_test_os_application_resume("App", 7), GDIAL_APP_ERROR_BAD_REQUEST);

    /* Stop path currently always issues request (failsafe strategy). */
    EXPECT_EQ(gdial_cpp_test_os_application_stop("App", 7), GDIAL_APP_ERROR_NONE);
    EXPECT_EQ(n.stop_calls, 1);
}

TEST_F(GDialCppTest, OsApplicationRegisterApplications_InitializedInvokesCallback)
{
    ASSERT_TRUE(gdial_cpp_test_init(ctx));
    gdial_cpp_test_register_registerapps_cb(test_registerapps_cb);

    auto *list = new RegisterAppEntryList;
    auto *entry = new RegisterAppEntry;
    entry->Names = "YouTube";
    entry->prefixes = "com.google";
    entry->cors = ".youtube.com";
    entry->allowStop = true;
    list->pushBack(entry);

    EXPECT_EQ(gdial_cpp_test_os_application_register_applications(list), GDIAL_APP_ERROR_NONE);
    EXPECT_EQ(g_registerapps_cb_calls, 1);
    EXPECT_NE(g_last_registerapps_payload, nullptr);
}

TEST_F(GDialCppTest, OsApplicationActivationAndFriendlyName_InitializedCallbacks)
{
    ASSERT_TRUE(gdial_cpp_test_init(ctx));
    gdial_cpp_test_register_activation_cb(test_activation_cb);
    gdial_cpp_test_register_friendlyname_cb(test_friendlyname_cb);

    EXPECT_EQ(gdial_cpp_test_os_application_activation_changed("true", "LivingRoom"), GDIAL_APP_ERROR_NONE);
    EXPECT_EQ(g_activation_cb_calls, 1);
    EXPECT_TRUE(g_last_activation_state);
    EXPECT_EQ(g_last_activation_friendlyname, "LivingRoom");

    EXPECT_EQ(gdial_cpp_test_os_application_activation_changed("false", "Kitchen"), GDIAL_APP_ERROR_NONE);
    EXPECT_EQ(g_activation_cb_calls, 2);
    EXPECT_FALSE(g_last_activation_state);
    EXPECT_EQ(g_last_activation_friendlyname, "Kitchen");

    EXPECT_EQ(gdial_cpp_test_os_application_friendlyname_changed("Bedroom"), GDIAL_APP_ERROR_NONE);
    EXPECT_EQ(g_friendlyname_cb_calls, 1);
    EXPECT_EQ(g_last_friendlyname, "Bedroom");
}

TEST_F(GDialCppTest, OsApplicationUpdateNetworkStandbyMode_InitializedInvokesDevCb)
{
    ASSERT_TRUE(gdial_cpp_test_init(ctx));

    gdial_cpp_test_os_application_update_network_standby_mode(TRUE);
    EXPECT_EQ(g_nwstandby_cb_calls, 1);
    EXPECT_TRUE(g_last_nwstandby_mode);

    gdial_cpp_test_os_application_update_network_standby_mode(FALSE);
    EXPECT_EQ(g_nwstandby_cb_calls, 2);
    EXPECT_FALSE(g_last_nwstandby_mode);
}

TEST_F(GDialCppTest, OsApplicationState_NetflixEnableStopBranchExecutes)
{
    ASSERT_TRUE(gdial_cpp_test_init(ctx));
    ASSERT_EQ(gdial_cpp_test_os_application_state_changed("Netflix", "id", "running", "none"), GDIAL_APP_ERROR_NONE);
    setenv("ENABLE_NETFLIX_STOP", "true", 1);

    GDialAppState state = GDIAL_APP_STATE_MAX;
    EXPECT_EQ(gdial_cpp_test_os_application_state("Netflix", 1, &state), GDIAL_APP_ERROR_NONE);
    /* Stubbed GetCurrentState() returns empty string -> state forced to running. */
    EXPECT_EQ(state, GDIAL_APP_STATE_RUNNING);
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
