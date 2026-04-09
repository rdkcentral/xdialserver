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
 * @file test_gdialServer.cpp
 * @brief Unit tests for the GDialApp object (server/gdial-app.h)
 *
 * Functions under test:
 *   gdial_app_state_to_string  - pure state→string mapping
 *   gdial_app_new              - GObject construction
 *   gdial_app_state            - query state of an unstarted app
 *   GDialAppState / GDialAppError enum contracts
 */

#include <gtest/gtest.h>
#include <cstring>

extern "C" {
#include <glib.h>
#include <glib-object.h>
#include <libxml/tree.h>
#include "gdial-app.h"
}

/* ================================================================== */
/* gdial_app_state_to_string                                           */
/* ================================================================== */

TEST(GDialAppStateToStringTest, Stopped) {
    EXPECT_STREQ(gdial_app_state_to_string(GDIAL_APP_STATE_STOPPED), "stopped");
}

TEST(GDialAppStateToStringTest, Running) {
    EXPECT_STREQ(gdial_app_state_to_string(GDIAL_APP_STATE_RUNNING), "running");
}

TEST(GDialAppStateToStringTest, Hide) {
    EXPECT_STREQ(gdial_app_state_to_string(GDIAL_APP_STATE_HIDE), "hidden");
}

TEST(GDialAppStateToStringTest, MaxReturnsNull) {
    EXPECT_EQ(gdial_app_state_to_string(GDIAL_APP_STATE_MAX), nullptr);
}

/* ================================================================== */
/* GDialAppState enum contracts                                        */
/* ================================================================== */

TEST(GDialAppStateEnumTest, StoppedIsZero) {
    EXPECT_EQ(GDIAL_APP_STATE_STOPPED, 0);
}

TEST(GDialAppStateEnumTest, MaxIsGreaterThanAllRunningStates) {
    EXPECT_GT(GDIAL_APP_STATE_MAX, GDIAL_APP_STATE_STOPPED);
    EXPECT_GT(GDIAL_APP_STATE_MAX, GDIAL_APP_STATE_HIDE);
    EXPECT_GT(GDIAL_APP_STATE_MAX, GDIAL_APP_STATE_RUNNING);
}

/* ================================================================== */
/* GDialAppError enum contracts                                        */
/* ================================================================== */

TEST(GDialAppErrorEnumTest, ErrorNoneIsZero) {
    EXPECT_EQ(GDIAL_APP_ERROR_NONE, 0);
}

TEST(GDialAppErrorEnumTest, ImplErrorsExceedPublicErrors) {
    /* GDIAL_APP_ERROR_IMPL_ (0x1000) must be above the state-derived errors */
    EXPECT_GT((int)GDIAL_APP_ERROR_IMPL_, (int)GDIAL_APP_ERROR_UNAUTH);
}

/* ================================================================== */
/* GDialApp object lifecycle                                           */
/* ================================================================== */

class GDialAppLifecycleTest : public ::testing::Test {
protected:
    GDialApp *app = nullptr;

    void TearDown() override {
        if (app) {
            g_object_unref(app);
            app = nullptr;
        }
    }
};

TEST_F(GDialAppLifecycleTest, New_ValidNameReturnsNonNull) {
    app = gdial_app_new("Netflix");
    ASSERT_NE(app, nullptr);
}

TEST_F(GDialAppLifecycleTest, New_AppNameStoredCorrectly) {
    app = gdial_app_new("YouTube");
    ASSERT_NE(app, nullptr);
    EXPECT_STREQ(app->name, "YouTube");
}

TEST_F(GDialAppLifecycleTest, New_InitialStateIsStopped) {
    app = gdial_app_new("Netflix");
    ASSERT_NE(app, nullptr);
    EXPECT_EQ(GDIAL_APP_GET_STATE(app), GDIAL_APP_STATE_STOPPED);
}

TEST_F(GDialAppLifecycleTest, New_InitialInstanceIdIsNone) {
    app = gdial_app_new("Netflix");
    ASSERT_NE(app, nullptr);
    EXPECT_EQ(app->instance_id, GDIAL_APP_INSTANCE_NONE);
}

TEST_F(GDialAppLifecycleTest, State_UnstartedAppReturnsStopped) {
    /* instance_id == NONE → gdial_app_state fast-path returns STOPPED */
    app = gdial_app_new("Netflix");
    ASSERT_NE(app, nullptr);
    GDialAppError err = gdial_app_state(app);
    EXPECT_EQ(err, GDIAL_APP_ERROR_NONE);
    EXPECT_EQ(GDIAL_APP_GET_STATE(app), GDIAL_APP_STATE_STOPPED);
}

TEST_F(GDialAppLifecycleTest, GetStateMacro_MatchesStructField) {
    app = gdial_app_new("App");
    ASSERT_NE(app, nullptr);
    /* The macro must read the same field the struct exposes */
    EXPECT_EQ(GDIAL_APP_GET_STATE(app), app->state);
}

/* ================================================================== */
/* Additional coverage for gdial-app.c helpers                         */
/* ================================================================== */

class GDialAppExtendedTest : public ::testing::Test {
protected:
    GDialApp *app = nullptr;
    const gchar *kAppName = "CoverageApp";

    void SetUp() override {
        app = gdial_app_new(kAppName);
        ASSERT_NE(app, nullptr);
    }

    void TearDown() override {
        /* Ensure persistent additional-data test file is removed. */
        gdial_app_remove_additional_dial_data_file(kAppName);
        if (app) {
            g_object_unref(app);
            app = nullptr;
        }
    }
};

TEST_F(GDialAppExtendedTest, StartHideResumeStop_ReturnsNone) {
    EXPECT_EQ(gdial_app_start(app, "payload", "query", "url", nullptr), GDIAL_APP_ERROR_NONE);
    EXPECT_EQ(gdial_app_hide(app), GDIAL_APP_ERROR_NONE);
    EXPECT_EQ(gdial_app_resume(app), GDIAL_APP_ERROR_NONE);
    EXPECT_EQ(gdial_app_stop(app), GDIAL_APP_ERROR_NONE);
}

TEST_F(GDialAppExtendedTest, Start_AssignsInstanceId) {
    EXPECT_EQ(app->instance_id, GDIAL_APP_INSTANCE_NONE);
    EXPECT_EQ(gdial_app_start(app, nullptr, nullptr, nullptr, nullptr), GDIAL_APP_ERROR_NONE);
    EXPECT_NE(app->instance_id, GDIAL_APP_INSTANCE_NONE);
}

TEST_F(GDialAppExtendedTest, FindInstanceByNameAndId) {
    EXPECT_EQ(gdial_app_start(app, nullptr, nullptr, nullptr, nullptr), GDIAL_APP_ERROR_NONE);

    GDialApp *by_name = gdial_app_find_instance_by_name(kAppName);
    ASSERT_NE(by_name, nullptr);
    EXPECT_EQ(by_name, app);

    GDialApp *by_id = gdial_app_find_instance_by_instance_id(app->instance_id);
    ASSERT_NE(by_id, nullptr);
    EXPECT_EQ(by_id, app);
}

TEST_F(GDialAppExtendedTest, SetAndGetLaunchPayload) {
    EXPECT_EQ(gdial_app_get_launch_payload(app), nullptr);

    gdial_app_set_launch_payload(app, "hello");
    ASSERT_NE(gdial_app_get_launch_payload(app), nullptr);
    EXPECT_STREQ(gdial_app_get_launch_payload(app), "hello");

    gdial_app_set_launch_payload(app, nullptr);
    EXPECT_EQ(gdial_app_get_launch_payload(app), nullptr);
}

TEST_F(GDialAppExtendedTest, SetGetAdditionalDialDataByKey) {
    GHashTable *ht = g_hash_table_new_full(g_str_hash, g_str_equal, g_free, g_free);
    g_hash_table_insert(ht, g_strdup("k1"), g_strdup("v1"));
    g_hash_table_insert(ht, g_strdup("k2"), g_strdup("v2"));

    gdial_app_set_additional_dial_data(app, ht);
    EXPECT_STREQ(gdial_app_get_additional_dial_data_by_key(app, "k1"), "v1");
    EXPECT_STREQ(gdial_app_get_additional_dial_data_by_key(app, "k2"), "v2");

    GHashTable *dup = gdial_app_get_additional_dial_data(app);
    ASSERT_NE(dup, nullptr);
    EXPECT_EQ(g_hash_table_size(dup), (guint)2);
    g_hash_table_unref(dup);
    g_hash_table_destroy(ht);
}

TEST_F(GDialAppExtendedTest, ClearAdditionalDialDataEmptiesTable) {
    GHashTable *ht = g_hash_table_new_full(g_str_hash, g_str_equal, g_free, g_free);
    g_hash_table_insert(ht, g_strdup("k"), g_strdup("v"));
    gdial_app_set_additional_dial_data(app, ht);
    g_hash_table_destroy(ht);

    gdial_app_clear_additional_dial_data(app);
    EXPECT_EQ(gdial_app_get_additional_dial_data_by_key(app, "k"), nullptr);
}

TEST_F(GDialAppExtendedTest, RefreshAdditionalDialDataReadsFromFile) {
    const gchar *raw = "alpha one\r\nbeta two\r\n";
    ASSERT_TRUE(gdial_app_write_additional_dial_data(kAppName, raw, strlen(raw)));

    gdial_app_refresh_additional_dial_data(app);
    EXPECT_STREQ(gdial_app_get_additional_dial_data_by_key(app, "alpha"), "one");
    EXPECT_STREQ(gdial_app_get_additional_dial_data_by_key(app, "beta"), "two");
}

TEST_F(GDialAppExtendedTest, FileHelpers_WriteReadRemoveRoundTrip) {
    const gchar *payload = "x y\r\n";
    gchar *out = nullptr;
    size_t out_len = 0;

    ASSERT_TRUE(gdial_app_write_additional_dial_data(kAppName, payload, strlen(payload)));
    ASSERT_TRUE(gdial_app_read_additional_dial_data(kAppName, &out, &out_len));
    ASSERT_NE(out, nullptr);
    EXPECT_EQ(out_len, strlen(payload));
    EXPECT_STREQ(out, payload);
    g_free(out);

    EXPECT_TRUE(gdial_app_remove_additional_dial_data_file(kAppName));
}

TEST_F(GDialAppExtendedTest, StateResponseNew_GeneratesXml) {
    int len = 0;
    app->state = GDIAL_APP_STATE_RUNNING;

    gchar *xml = gdial_app_state_response_new(
        app,
        "2.2.1",
        "2.2",
        "urn:dial-multiscreen-org:schemas:dial",
        &len);

    ASSERT_NE(xml, nullptr);
    EXPECT_GT(len, 0);
    EXPECT_NE(strstr(xml, "<service"), nullptr);
    EXPECT_NE(strstr(xml, "<state>running</state>"), nullptr);
    xmlFree(xml);
}

TEST_F(GDialAppExtendedTest, StateResponseNew_ClientBelow21MapsHiddenToStopped) {
    int len = 0;
    app->state = GDIAL_APP_STATE_HIDE;

    gchar *xml = gdial_app_state_response_new(
        app,
        "2.2.1",
        "2.0",
        "urn:dial-multiscreen-org:schemas:dial",
        &len);

    ASSERT_NE(xml, nullptr);
    EXPECT_NE(strstr(xml, "<state>stopped</state>"), nullptr);
    xmlFree(xml);
}

