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

extern "C" {
#include <glib.h>
#include <glib-object.h>
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

