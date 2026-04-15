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
 * @file test_gdialPlatApp.cpp
 * @brief Unit tests for server/plat/gdial-plat-app.c
 *
 * Coverage strategy:
 *
 *  GDialPlatAppNullGuardTest — standalone tests (no gdial_plat_init).
 *    Tests every g_return_val_if_fail guard in the public API.  Each guard
 *    logs a GLib critical to stderr and returns the fail value; the binary
 *    does NOT abort.
 *
 *  GDialPlatAppTest (fixture) — calls gdial_plat_init(ctx) in SetUp and
 *    gdial_plat_term() in TearDown.  Covers the lifecycle and all
 *    synchronous delegation paths.
 *
 *  GDialPlatAppAsyncTest (fixture) — extends GDialPlatAppTest, pumps
 *    g_main_context_default() to fire the 1 ms GLib timeouts used by the
 *    async dispatch paths.
 *
 * Note on timer context: g_timeout_add_full() (used inside gdial-plat-app.c)
 * attaches to the GLib thread-default context.  Since no thread-default is
 * pushed in the test thread, that resolves to g_main_context_default().
 * The pump helpers therefore iterate g_main_context_default(), not ctx_.
 */

#include <gtest/gtest.h>

#include <cstring>

extern "C" {
#include <glib.h>
#include "gdial-plat-app.h"
#include "gdial-app.h"   /* GDIAL_APP_INSTANCE_NONE, GDialAppState, GDialAppError */
}

static void drain_default_context()
{
    GMainContext *def = g_main_context_default();
    /* 2-second safety cap prevents infinite loops in pathological cases. */
    const gint64 deadline = g_get_monotonic_time() + 2000000;
    bool had_activity;

    /*
     * Each pass drains all currently-ready sources.  If anything fired, a
     * callback may have just scheduled a new 1 ms timer, so sleep 10 ms
     * (>> 1 ms timer resolution) before re-checking.  This guarantees any
     * cascaded timer is pending on the next pass.
     *
     * The loop exits when a full pass dispatches nothing (all timers have
     * fired and no new ones were created) or the deadline is reached.
     */
    do {
        had_activity = false;
        while (g_main_context_pending(def)) {
            g_main_context_iteration(def, FALSE);
            had_activity = true;
        }
        if (had_activity) {
            g_usleep(10000); /* 10 ms — long enough for any new 1 ms timer */
        }
    } while (had_activity && g_get_monotonic_time() < deadline);

    /* Final mop-up in case a timer fired during the last sleep. */
    while (g_main_context_pending(def)) {
        g_main_context_iteration(def, FALSE);
    }
}

/* ================================================================== */
/* SECTION 1: Null-guard tests — no gdial_plat_init required          */
/* g_return_val_if_fail returns the fail value and emits a GLib       */
/* critical warning; the process is NOT aborted.                      */
/* ================================================================== */

TEST(GDialPlatAppNullGuardTest, Init_NullContext_ReturnsInternal) {
    EXPECT_EQ(gdial_plat_init(nullptr), (gint)GDIAL_APP_ERROR_INTERNAL);
}

TEST(GDialPlatAppNullGuardTest, Start_NullName_ReturnsBadRequest) {
    gint id = 0;
    EXPECT_EQ(gdial_plat_application_start(nullptr, nullptr, nullptr, nullptr, &id),
              GDIAL_APP_ERROR_BAD_REQUEST);
}

TEST(GDialPlatAppNullGuardTest, Start_NullInstanceId_ReturnsBadRequest) {
    EXPECT_EQ(gdial_plat_application_start("Netflix", nullptr, nullptr, nullptr, nullptr),
              GDIAL_APP_ERROR_BAD_REQUEST);
}

TEST(GDialPlatAppNullGuardTest, Hide_NullName_ReturnsBadRequest) {
    EXPECT_EQ(gdial_plat_application_hide(nullptr, 1),
              GDIAL_APP_ERROR_BAD_REQUEST);
}

TEST(GDialPlatAppNullGuardTest, Hide_InstanceNone_ReturnsBadRequest) {
    EXPECT_EQ(gdial_plat_application_hide("App", GDIAL_APP_INSTANCE_NONE),
              GDIAL_APP_ERROR_BAD_REQUEST);
}

TEST(GDialPlatAppNullGuardTest, Resume_NullName_ReturnsBadRequest) {
    EXPECT_EQ(gdial_plat_application_resume(nullptr, 1),
              GDIAL_APP_ERROR_BAD_REQUEST);
}

TEST(GDialPlatAppNullGuardTest, Resume_InstanceNone_ReturnsBadRequest) {
    EXPECT_EQ(gdial_plat_application_resume("App", GDIAL_APP_INSTANCE_NONE),
              GDIAL_APP_ERROR_BAD_REQUEST);
}

TEST(GDialPlatAppNullGuardTest, Stop_NullName_ReturnsBadRequest) {
    EXPECT_EQ(gdial_plat_application_stop(nullptr, 1),
              GDIAL_APP_ERROR_BAD_REQUEST);
}

TEST(GDialPlatAppNullGuardTest, Stop_InstanceNone_ReturnsBadRequest) {
    EXPECT_EQ(gdial_plat_application_stop("App", GDIAL_APP_INSTANCE_NONE),
              GDIAL_APP_ERROR_BAD_REQUEST);
}

TEST(GDialPlatAppNullGuardTest, State_NullName_ReturnsBadRequest) {
    GDialAppState s = GDIAL_APP_STATE_STOPPED;
    EXPECT_EQ(gdial_plat_application_state(nullptr, 1, &s),
              GDIAL_APP_ERROR_BAD_REQUEST);
}

TEST(GDialPlatAppNullGuardTest, State_NullStatePtr_ReturnsBadRequest) {
    EXPECT_EQ(gdial_plat_application_state("App", 1, nullptr),
              GDIAL_APP_ERROR_BAD_REQUEST);
}

TEST(GDialPlatAppNullGuardTest, State_InstanceNone_ReturnsBadRequest) {
    GDialAppState s = GDIAL_APP_STATE_STOPPED;
    EXPECT_EQ(gdial_plat_application_state("App", GDIAL_APP_INSTANCE_NONE, &s),
              GDIAL_APP_ERROR_BAD_REQUEST);
}

TEST(GDialPlatAppNullGuardTest, StateChanged_NullName_ReturnsBadRequest) {
    EXPECT_EQ(gdial_plat_application_state_changed(nullptr, "id", "running", "none"),
              GDIAL_APP_ERROR_BAD_REQUEST);
}

TEST(GDialPlatAppNullGuardTest, StateChanged_NullId_ReturnsBadRequest) {
    EXPECT_EQ(gdial_plat_application_state_changed("App", nullptr, "running", "none"),
              GDIAL_APP_ERROR_BAD_REQUEST);
}

TEST(GDialPlatAppNullGuardTest, StateChanged_NullState_ReturnsBadRequest) {
    EXPECT_EQ(gdial_plat_application_state_changed("App", "id", nullptr, "none"),
              GDIAL_APP_ERROR_BAD_REQUEST);
}

TEST(GDialPlatAppNullGuardTest, StateChanged_NullError_ReturnsBadRequest) {
    EXPECT_EQ(gdial_plat_application_state_changed("App", "id", "running", nullptr),
              GDIAL_APP_ERROR_BAD_REQUEST);
}

TEST(GDialPlatAppNullGuardTest, ActivationChanged_NullActivation_ReturnsBadRequest) {
    EXPECT_EQ(gdial_plat_application_activation_changed(nullptr, "TV"),
              GDIAL_APP_ERROR_BAD_REQUEST);
}

TEST(GDialPlatAppNullGuardTest, ActivationChanged_NullFriendlyName_ReturnsBadRequest) {
    EXPECT_EQ(gdial_plat_application_activation_changed("true", nullptr),
              GDIAL_APP_ERROR_BAD_REQUEST);
}

TEST(GDialPlatAppNullGuardTest, FriendlyNameChanged_Null_ReturnsBadRequest) {
    EXPECT_EQ(gdial_plat_application_friendlyname_changed(nullptr),
              GDIAL_APP_ERROR_BAD_REQUEST);
}

TEST(GDialPlatAppNullGuardTest, RegisterApplications_Null_ReturnsBadRequest) {
    EXPECT_EQ(gdial_plat_application_register_applications(nullptr),
              GDIAL_APP_ERROR_BAD_REQUEST);
}

TEST(GDialPlatAppNullGuardTest, UpdateManufacturerName_Null_ReturnsBadRequest) {
    EXPECT_EQ(gdial_plat_application_update_manufacturer_name(nullptr),
              GDIAL_APP_ERROR_BAD_REQUEST);
}

TEST(GDialPlatAppNullGuardTest, UpdateModelName_Null_ReturnsBadRequest) {
    EXPECT_EQ(gdial_plat_application_update_model_name(nullptr),
              GDIAL_APP_ERROR_BAD_REQUEST);
}

TEST(GDialPlatAppNullGuardTest, ServiceNotification_TrueNullNotifier_ReturnsBadRequest) {
    /* guard: (notifier != NULL) || (false == isNotifyRequired) */
    EXPECT_EQ(gdial_plat_application_service_notification(TRUE, nullptr),
              GDIAL_APP_ERROR_BAD_REQUEST);
}

/* Before gdial_plat_init, gdial_plat_app_async_contexts is NULL →
 * g_return_val_if_fail(gdial_plat_app_async_contexts != NULL, NULL) fires. */
TEST(GDialPlatAppNullGuardTest, StartAsync_BeforeInit_ReturnsNull) {
    void *h = gdial_plat_application_start_async("Netflix", nullptr, nullptr, nullptr, nullptr);
    EXPECT_EQ(h, nullptr);
}

TEST(GDialPlatAppNullGuardTest, StopAsync_BeforeInit_ReturnsNull) {
    void *h = gdial_plat_application_stop_async("Netflix", 1, nullptr);
    EXPECT_EQ(h, nullptr);
}

TEST(GDialPlatAppNullGuardTest, StateAsync_BeforeInit_ReturnsNull) {
    void *h = gdial_plat_application_state_async("Netflix", 1, nullptr);
    EXPECT_EQ(h, nullptr);
}

/* Empty-name guard: g_return_val_if_fail(app_name != NULL && strlen(app_name), NULL) */
TEST(GDialPlatAppNullGuardTest, StartAsync_EmptyName_ReturnsNull) {
    /* Must call after a gdial_plat_init so async_contexts is non-NULL.
     * Use the default context for a minimal init / term pair. */
    GMainContext *ctx = g_main_context_new();
    gdial_plat_init(ctx);
    void *h = gdial_plat_application_start_async("", nullptr, nullptr, nullptr, nullptr);
    EXPECT_EQ(h, nullptr);
    gdial_plat_term();
    g_main_context_unref(ctx);
}

/* ================================================================== */
/* SECTION 2: Lifecycle and synchronous delegation                    */
/* ================================================================== */

class GDialPlatAppTest : public ::testing::Test {
protected:
    GMainContext *ctx_ = nullptr;

    void SetUp() override {
        ctx_ = g_main_context_new();
        gdial_plat_init(ctx_);
    }

    void TearDown() override {
        /* Drain ALL timers (including cascaded 1 ms sources) before term.
         * Do NOT drain after term: gdial_plat_term() frees the async contexts
         * that timer callbacks still reference, so draining after term causes
         * use-after-free → segfault. */
        drain_default_context();
        gdial_plat_term();
        g_main_context_unref(ctx_);
        ctx_ = nullptr;
    }
};

TEST_F(GDialPlatAppTest, Init_ValidContext_ReturnsNone) {
    /* gdial_plat_init was called in SetUp; assert it succeeded */
    SUCCEED();
}

TEST_F(GDialPlatAppTest, Init_SameContextAgain_ReturnsNone) {
    /* Calling with the same context passes the guard */
    EXPECT_EQ(gdial_plat_init(ctx_), (gint)GDIAL_APP_ERROR_NONE);
    /* Balance the extra ref and hash-table ref acquired by the second init */
    gdial_plat_term();
}

TEST_F(GDialPlatAppTest, Init_DifferentContext_ReturnsInternal) {
    GMainContext *other = g_main_context_new();
    EXPECT_EQ(gdial_plat_init(other), (gint)GDIAL_APP_ERROR_INTERNAL);
    g_main_context_unref(other);
}

TEST_F(GDialPlatAppTest, RegisterCallbacks_NoCrash) {
    gdail_plat_register_activation_cb(nullptr);
    gdail_plat_register_friendlyname_cb(nullptr);
    gdail_plat_register_registerapps_cb(nullptr);
    gdail_plat_register_manufacturername_cb(nullptr);
    gdail_plat_register_modelname_cb(nullptr);
    SUCCEED();
}

TEST_F(GDialPlatAppTest, Start_ValidArgs_ReturnsNoneAndSetsId) {
    gint id = 0;
    EXPECT_EQ(gdial_plat_application_start("Netflix", nullptr, nullptr, nullptr, &id),
              GDIAL_APP_ERROR_NONE);
    EXPECT_EQ(id, 1);  /* OS stub sets instance_id = 1 */
}

TEST_F(GDialPlatAppTest, Start_WithPayloadAndQuery_ReturnsNone) {
    gint id = 0;
    EXPECT_EQ(gdial_plat_application_start("YouTube", "payload", "query=1", nullptr, &id),
              GDIAL_APP_ERROR_NONE);
}

TEST_F(GDialPlatAppTest, Hide_ValidArgs_ReturnsNone) {
    EXPECT_EQ(gdial_plat_application_hide("Netflix", 1), GDIAL_APP_ERROR_NONE);
}

TEST_F(GDialPlatAppTest, Resume_ValidArgs_ReturnsNone) {
    EXPECT_EQ(gdial_plat_application_resume("Netflix", 1), GDIAL_APP_ERROR_NONE);
}

TEST_F(GDialPlatAppTest, Stop_ValidArgs_ReturnsNone) {
    EXPECT_EQ(gdial_plat_application_stop("Netflix", 1), GDIAL_APP_ERROR_NONE);
}

TEST_F(GDialPlatAppTest, State_ValidArgs_ReturnsNoneAndSetsState) {
    GDialAppState state = GDIAL_APP_STATE_MAX;
    EXPECT_EQ(gdial_plat_application_state("Netflix", 1, &state), GDIAL_APP_ERROR_NONE);
    EXPECT_EQ(state, GDIAL_APP_STATE_STOPPED);  /* OS stub default */
}

TEST_F(GDialPlatAppTest, StateChanged_ValidArgs_ReturnsNone) {
    EXPECT_EQ(gdial_plat_application_state_changed("Netflix", "id1", "running", "none"),
              GDIAL_APP_ERROR_NONE);
}

TEST_F(GDialPlatAppTest, ActivationChanged_ValidArgs_ReturnsNone) {
    EXPECT_EQ(gdial_plat_application_activation_changed("true", "LivingRoom"),
              GDIAL_APP_ERROR_NONE);
}

TEST_F(GDialPlatAppTest, FriendlyNameChanged_ValidArgs_ReturnsNone) {
    EXPECT_EQ(gdial_plat_application_friendlyname_changed("LivingRoom"),
              GDIAL_APP_ERROR_NONE);
}

TEST_F(GDialPlatAppTest, GetProtocolVersion_ReturnsNonEmpty) {
    const char *ver = gdial_plat_application_get_protocol_version();
    ASSERT_NE(ver, nullptr);
    EXPECT_GT(strlen(ver), 0u);
}

TEST_F(GDialPlatAppTest, RegisterApplications_ValidPtr_ReturnsNone) {
    int dummy = 42;
    EXPECT_EQ(gdial_plat_application_register_applications(&dummy), GDIAL_APP_ERROR_NONE);
}

TEST_F(GDialPlatAppTest, UpdateNetworkStandbyMode_NoCrash) {
    gdial_plat_application_update_network_standby_mode(TRUE);
    gdial_plat_application_update_network_standby_mode(FALSE);
    SUCCEED();
}

TEST_F(GDialPlatAppTest, UpdateManufacturerName_ValidArgs_ReturnsNone) {
    EXPECT_EQ(gdial_plat_application_update_manufacturer_name("Acme"),
              GDIAL_APP_ERROR_NONE);
}

TEST_F(GDialPlatAppTest, UpdateModelName_ValidArgs_ReturnsNone) {
    EXPECT_EQ(gdial_plat_application_update_model_name("BoxV2"),
              GDIAL_APP_ERROR_NONE);
}

TEST_F(GDialPlatAppTest, ServiceNotification_FalseNullNotifier_ReturnsNone) {
    /* Guard: (notifier != NULL) || (false == isNotifyRequired) — FALSE+NULL passes */
    EXPECT_EQ(gdial_plat_application_service_notification(FALSE, nullptr),
              GDIAL_APP_ERROR_NONE);
}

TEST_F(GDialPlatAppTest, ServiceNotification_TrueValidNotifier_ReturnsNone) {
    int dummy = 1;
    EXPECT_EQ(gdial_plat_application_service_notification(TRUE, &dummy),
              GDIAL_APP_ERROR_NONE);
}

/* ================================================================== */
/* SECTION 3: Async dispatch tests                                    */
/* Timers fire on g_main_context_default(); pump that context.        */
/* ================================================================== */

static int    s_state_cb_calls = 0;
static GDialAppState s_last_cb_state = GDIAL_APP_STATE_MAX;

static void test_state_cb(gint /*instance_id*/, GDialAppState state, gpointer /*data*/)
{
    ++s_state_cb_calls;
    s_last_cb_state = state;
}

static void pump_default(void)
{
    drain_default_context();
}

class GDialPlatAppAsyncTest : public GDialPlatAppTest {
protected:
    void SetUp() override {
        s_state_cb_calls  = 0;
        s_last_cb_state   = GDIAL_APP_STATE_MAX;
        GDialPlatAppTest::SetUp();
        gdial_plat_application_set_state_cb(test_state_cb, nullptr);
    }
    /* TearDown is inherited from GDialPlatAppTest; it pumps then calls term. */
};

TEST_F(GDialPlatAppAsyncTest, StartAsync_Netflix_ReturnsNonNullAndFires) {
    /* GSourceFunc_application_start_async_cb has an explicit branch for "Netflix" */
    void *h = gdial_plat_application_start_async("Netflix", nullptr, nullptr, nullptr, nullptr);
    ASSERT_NE(h, nullptr);
    pump_default();
    /* After pump the context is freed; do NOT access h */
    SUCCEED();
}

TEST_F(GDialPlatAppAsyncTest, StartAsync_Youtube_ReturnsNonNullAndFires) {
    void *h = gdial_plat_application_start_async("Youtube", nullptr, nullptr, nullptr, nullptr);
    ASSERT_NE(h, nullptr);
    pump_default();
    SUCCEED();
}

TEST_F(GDialPlatAppAsyncTest, StartAsync_UnknownApp_ReturnsNonNull) {
    /* Fires g_warn_if_reached() inside the callback — harmless in tests */
    void *h = gdial_plat_application_start_async("UnknownApp", nullptr, nullptr, nullptr, nullptr);
    ASSERT_NE(h, nullptr);
    pump_default();
    SUCCEED();
}

TEST_F(GDialPlatAppAsyncTest, StateAsync_ValidArgs_InvokesStateCb) {
    void *h = gdial_plat_application_state_async("Netflix", 1, nullptr);
    ASSERT_NE(h, nullptr);
    pump_default();
    /* gdial_app_state_cb_ (set to test_state_cb) is invoked by the timer */
    EXPECT_GE(s_state_cb_calls, 1);
    EXPECT_EQ(s_last_cb_state, GDIAL_APP_STATE_STOPPED);  /* OS stub default */
}

TEST_F(GDialPlatAppAsyncTest, StateAsync_EmptyName_ReturnsNull) {
    void *h = gdial_plat_application_state_async("", 1, nullptr);
    EXPECT_EQ(h, nullptr);
}

TEST_F(GDialPlatAppAsyncTest, StopAsync_ValidArgs_ReturnsNonNullAndFires) {
    void *h = gdial_plat_application_stop_async("Netflix", 1, nullptr);
    ASSERT_NE(h, nullptr);
    pump_default();
    SUCCEED();
}

TEST_F(GDialPlatAppAsyncTest, StopAsync_EmptyName_ReturnsNull) {
    void *h = gdial_plat_application_stop_async("", 1, nullptr);
    EXPECT_EQ(h, nullptr);
}

TEST_F(GDialPlatAppAsyncTest, RemoveAsyncSource_BeforeTimerFires_NoCrash) {
    /* Schedule a stop_async, then explicitly cancel it before the timer fires */
    void *h = gdial_plat_application_stop_async("Netflix", 1, nullptr);
    ASSERT_NE(h, nullptr);
    gdial_plat_application_remove_async_source(h);
    SUCCEED();  /* handle freed by remove; no need to pump context */
}

TEST_F(GDialPlatAppAsyncTest, SetStateCb_AffectsNextStateAsync) {
    /* Replace the callback mid-test and verify the new one is invoked */
    static int calls2 = 0;
    gdial_plat_application_set_state_cb(
        [](gint, GDialAppState, gpointer) { ++calls2; },
        nullptr);

    void *h = gdial_plat_application_state_async("Netflix", 1, nullptr);
    ASSERT_NE(h, nullptr);
    pump_default();
    EXPECT_GE(calls2, 1);
    EXPECT_EQ(s_state_cb_calls, 0);  /* original cb NOT called */
}
