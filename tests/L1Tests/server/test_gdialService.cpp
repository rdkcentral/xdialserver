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
 * @file test_gdialService.cpp
 * @brief Unit tests for gdialservice.cpp public API methods.
 *
 * Strategy: provide a stand-alone stub for gdialServiceImpl so no real server
 * threads start, then exercise all gdialService:: methods directly.
 */

#include <gtest/gtest.h>
#include <string>
#include <vector>

#include "gdialservice.h"
#include "gdialserviceimpl.h"

/* ================================================================== */
/* Stub tracking variables — read by tests                            */
/* ================================================================== */

namespace {

struct StubState {
    int  send_request_calls        = 0;
    int  last_event                = -1;
    std::string last_appname;
    std::string last_appid;
    std::string last_state;
    std::string last_error;
    std::string last_activation;
    std::string last_friendlyname;
    std::string last_manufacturer;
    std::string last_model;
    bool        last_nw_standby    = false;
    void       *last_data_param    = nullptr;

    void reset() { *this = StubState(); }
} g_stub;

}  // namespace

/* ================================================================== */
/* gdialServiceImpl stub — no real server, no threads                 */
/* ================================================================== */

static gdialServiceImpl *s_impl_instance = nullptr;

gdialServiceImpl* gdialServiceImpl::getInstance(void) {
    if (!s_impl_instance) {
        s_impl_instance = new gdialServiceImpl();
    }
    return s_impl_instance;
}

void gdialServiceImpl::destroyInstance() {
    delete s_impl_instance;
    s_impl_instance = nullptr;
}

int gdialServiceImpl::start_GDialServer(int /*argc*/, char ** /*argv*/) {
    return 0;  // success, no actual server started
}

bool gdialServiceImpl::stop_GDialServer() {
    return true;
}

void gdialServiceImpl::sendRequest(const RequestHandlerPayload& payload) {
    ++g_stub.send_request_calls;
    g_stub.last_event = static_cast<int>(payload.event);
    g_stub.last_appname      = payload.appNameOrfriendlyname;
    g_stub.last_appid        = payload.appIdOractivation;
    g_stub.last_state        = payload.state;
    g_stub.last_error        = payload.error;
    g_stub.last_manufacturer = payload.manufacturer;
    g_stub.last_model        = payload.model;
    g_stub.last_nw_standby   = payload.user_param1;
    g_stub.last_data_param   = payload.data_param;
}

void gdialServiceImpl::notifyResponse(const ResponseHandlerPayload& /*payload*/) {}

// Virtual overrides required by GDialNotifier interface
void gdialServiceImpl::onApplicationLaunchRequest(std::string, std::string) {}
void gdialServiceImpl::onApplicationLaunchRequestWithLaunchParam(std::string, std::string, std::string, std::string) {}
void gdialServiceImpl::onApplicationStopRequest(std::string, std::string) {}
void gdialServiceImpl::onApplicationHideRequest(std::string, std::string) {}
void gdialServiceImpl::onApplicationResumeRequest(std::string, std::string) {}
void gdialServiceImpl::onApplicationStateRequest(std::string, std::string) {}
void gdialServiceImpl::updatePowerState(std::string) {}

/* ================================================================== */
/* Minimal GDialNotifier implementation for tests                     */
/* ================================================================== */

class TestNotifier : public GDialNotifier {
public:
    std::string last_launch_app;
    std::string last_stop_app;
    std::string last_hide_app;
    std::string last_resume_app;
    std::string last_state_app;
    std::string last_power_state;

    void onApplicationLaunchRequest(std::string appName, std::string /*param*/) override {
        last_launch_app = appName;
    }
    void onApplicationLaunchRequestWithLaunchParam(std::string appName, std::string, std::string, std::string) override {
        last_launch_app = appName;
    }
    void onApplicationStopRequest(std::string appName, std::string) override { last_stop_app = appName; }
    void onApplicationHideRequest(std::string appName, std::string) override { last_hide_app = appName; }
    void onApplicationResumeRequest(std::string appName, std::string) override { last_resume_app = appName; }
    void onApplicationStateRequest(std::string appName, std::string) override { last_state_app = appName; }
    void updatePowerState(std::string ps) override { last_power_state = ps; }
};

/* ================================================================== */
/* Fixture — creates / destroys a gdialService instance per test      */
/* ================================================================== */

class GDialServiceTest : public ::testing::Test {
protected:
    TestNotifier notifier;
    gdialService *svc = nullptr;

    void SetUp() override {
        g_stub.reset();
        std::vector<std::string> args;
        svc = gdialService::getInstance(&notifier, args, "UnitTest");
        ASSERT_NE(svc, nullptr);
    }

    void TearDown() override {
        gdialService::destroyInstance();
        svc = nullptr;
        g_stub.reset();
    }
};

/* ================================================================== */
/* Singleton lifecycle                                                 */
/* ================================================================== */

TEST_F(GDialServiceTest, GetInstance_ReturnsSamePointer) {
    std::vector<std::string> args;
    gdialService *svc2 = gdialService::getInstance(&notifier, args, "UnitTest");
    EXPECT_EQ(svc2, svc);
}

/* ================================================================== */
/* ApplicationStateChanged                                            */
/* ================================================================== */

TEST_F(GDialServiceTest, ApplicationStateChanged_SendsCorrectPayload) {
    auto rc = svc->ApplicationStateChanged("Netflix", "running", "id1", "none");
    EXPECT_EQ(rc, GDIAL_SERVICE_ERROR_NONE);
    EXPECT_EQ(g_stub.send_request_calls, 1);
    EXPECT_EQ(g_stub.last_event, static_cast<int>(APP_STATE_CHANGED));
    EXPECT_EQ(g_stub.last_appname, "Netflix");
    EXPECT_EQ(g_stub.last_appid,   "id1");
    EXPECT_EQ(g_stub.last_state,   "running");
    EXPECT_EQ(g_stub.last_error,   "none");
}

/* ================================================================== */
/* ActivationChanged                                                   */
/* ================================================================== */

TEST_F(GDialServiceTest, ActivationChanged_TruePayload) {
    auto rc = svc->ActivationChanged("true", "FriendlyName");
    EXPECT_EQ(rc, GDIAL_SERVICE_ERROR_NONE);
    EXPECT_EQ(g_stub.send_request_calls, 1);
    EXPECT_EQ(g_stub.last_event,        static_cast<int>(ACTIVATION_CHANGED));
    EXPECT_EQ(g_stub.last_appid,        "true");
    EXPECT_EQ(g_stub.last_appname,      "FriendlyName");
}

TEST_F(GDialServiceTest, ActivationChanged_FalsePayload) {
    auto rc = svc->ActivationChanged("false", "");
    EXPECT_EQ(rc, GDIAL_SERVICE_ERROR_NONE);
    EXPECT_EQ(g_stub.last_appid, "false");
}

/* ================================================================== */
/* FriendlyNameChanged                                                 */
/* ================================================================== */

TEST_F(GDialServiceTest, FriendlyNameChanged_SendsCorrectPayload) {
    auto rc = svc->FriendlyNameChanged("LivingRoom");
    EXPECT_EQ(rc, GDIAL_SERVICE_ERROR_NONE);
    EXPECT_EQ(g_stub.send_request_calls, 1);
    EXPECT_EQ(g_stub.last_event,   static_cast<int>(FRIENDLYNAME_CHANGED));
    EXPECT_EQ(g_stub.last_appname, "LivingRoom");
}

/* ================================================================== */
/* RegisterApplications                                               */
/* ================================================================== */

TEST_F(GDialServiceTest, RegisterApplications_SendsPayloadWithList) {
    RegisterAppEntryList *list = new RegisterAppEntryList;
    RegisterAppEntry *entry = new RegisterAppEntry;
    entry->Names = "Netflix";
    entry->cors  = ".netflix.com";
    list->pushBack(entry);

    auto rc = svc->RegisterApplications(list);
    EXPECT_EQ(rc, GDIAL_SERVICE_ERROR_NONE);
    EXPECT_EQ(g_stub.send_request_calls, 1);
    EXPECT_EQ(g_stub.last_event,      static_cast<int>(REGISTER_APPLICATIONS));
    EXPECT_EQ(g_stub.last_data_param, list);
}

TEST_F(GDialServiceTest, RegisterApplications_NullListNoCrash) {
    auto rc = svc->RegisterApplications(nullptr);
    EXPECT_EQ(rc, GDIAL_SERVICE_ERROR_NONE);
    EXPECT_EQ(g_stub.send_request_calls, 1);
    EXPECT_EQ(g_stub.last_data_param, nullptr);
}

/* ================================================================== */
/* setNetworkStandbyMode                                              */
/* ================================================================== */

TEST_F(GDialServiceTest, SetNetworkStandbyMode_TrueSent) {
    svc->setNetworkStandbyMode(true);
    EXPECT_EQ(g_stub.send_request_calls, 1);
    EXPECT_EQ(g_stub.last_event,       static_cast<int>(UPDATE_NW_STANDBY));
    EXPECT_TRUE(g_stub.last_nw_standby);
}

TEST_F(GDialServiceTest, SetNetworkStandbyMode_FalseSent) {
    svc->setNetworkStandbyMode(false);
    EXPECT_EQ(g_stub.last_event,        static_cast<int>(UPDATE_NW_STANDBY));
    EXPECT_FALSE(g_stub.last_nw_standby);
}

/* ================================================================== */
/* setManufacturerName                                                */
/* ================================================================== */

TEST_F(GDialServiceTest, SetManufacturerName_SendsCorrectPayload) {
    auto rc = svc->setManufacturerName("Acme");
    EXPECT_EQ(rc, GDIAL_SERVICE_ERROR_NONE);
    EXPECT_EQ(g_stub.send_request_calls,  1);
    EXPECT_EQ(g_stub.last_event,           static_cast<int>(UPDATE_MANUFACTURER_NAME));
    EXPECT_EQ(g_stub.last_manufacturer,   "Acme");
}

/* ================================================================== */
/* setModelName                                                       */
/* ================================================================== */

TEST_F(GDialServiceTest, SetModelName_SendsCorrectPayload) {
    auto rc = svc->setModelName("ModelX");
    EXPECT_EQ(rc, GDIAL_SERVICE_ERROR_NONE);
    EXPECT_EQ(g_stub.send_request_calls, 1);
    EXPECT_EQ(g_stub.last_event,          static_cast<int>(UPDATE_MODEL_NAME));
    EXPECT_EQ(g_stub.last_model,          "ModelX");
}

/* ================================================================== */
/* Multiple calls accumulate correctly                                */
/* ================================================================== */

TEST_F(GDialServiceTest, MultipleAPICalls_EachSendsOneRequest) {
    svc->ActivationChanged("true", "TV");
    svc->FriendlyNameChanged("Bedroom");
    svc->setManufacturerName("Corp");
    svc->setModelName("Box1");
    EXPECT_EQ(g_stub.send_request_calls, 4);
}

/* ================================================================== */
/* RegisterAppEntryList helper API                                    */
/* ================================================================== */

TEST(RegisterAppEntryListTest, PushBackAndGetValuesPreservesOrder) {
    RegisterAppEntryList list;
    for (int i = 0; i < 3; ++i) {
        RegisterAppEntry *e = new RegisterAppEntry;
        e->Names = "App" + std::to_string(i);
        list.pushBack(e);
    }
    const auto &vals = list.getValues();
    ASSERT_EQ(vals.size(), 3u);
    EXPECT_EQ(vals[0]->Names, "App0");
    EXPECT_EQ(vals[1]->Names, "App1");
    EXPECT_EQ(vals[2]->Names, "App2");
}

TEST(RegisterAppEntryListTest, DestructorFreesEntries) {
    // No crash when list and entries go out of scope
    {
        RegisterAppEntryList list;
        RegisterAppEntry *e = new RegisterAppEntry;
        e->Names = "YouTube";
        list.pushBack(e);
    }
    SUCCEED();
}

/* ================================================================== */
/* RequestHandlerPayload / ResponseHandlerPayload enum coverage        */
/* ================================================================== */

TEST(AppRequestEventsEnumTest, InvalidRequestIsLast) {
    EXPECT_GT(INVALID_REQUEST, REGISTER_APPLICATIONS);
}

TEST(AppResponseEventsEnumTest, InvalidStateIsLast) {
    EXPECT_GT(APP_INVALID_STATE, APP_RESUME_REQUEST);
}
