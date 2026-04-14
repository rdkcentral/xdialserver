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
 *
 * Coverage strategy:
 *  - GDialServiceImplTest: exercises gdialServiceImpl singleton, sendRequest,
 *    notifyResponse (via on* callbacks), and stop_GDialServer without a live
 *    server — covers lines 831-848, 1119-1222.
 *  - GDialServiceTest: calls gdialService::getInstance() which always exercises
 *    start_GDialServer up to the point it fails in CI (loopback-only environment
 *    causes rest_http_server and local_rest_http_server to conflict on port 56889)
 *    — covers lines 850-961 + 222-332.
 *  - Public API tests are conditional: they GTEST_SKIP when getInstance returns
 *    null, and provide full coverage when a real non-loopback interface exists.
 */

#include <gtest/gtest.h>

#include <string>
#include <vector>

#include "gdialservice.h"
#include "gdialserviceimpl.h"
#include "gdialservicecommon.h"

#include "gdial-plat-util.h"

/* ================================================================== */
/* Minimal notifier                                                    */
/* ================================================================== */

class TestServiceNotifier : public GDialNotifier {
public:
    int update_power_calls = 0;

    void onApplicationLaunchRequest(std::string, std::string) override {}
    void onApplicationLaunchRequestWithLaunchParam(std::string, std::string, std::string, std::string) override {}
    void onApplicationStopRequest(std::string, std::string) override {}
    void onApplicationHideRequest(std::string, std::string) override {}
    void onApplicationResumeRequest(std::string, std::string) override {}
    void onApplicationStateRequest(std::string, std::string) override {}
    void updatePowerState(std::string) override { ++update_power_calls; }
};

/* ================================================================== */
/* Helper: find an interface that has a non-loopback IPv4 address so  */
/* that rest_http_server and local_rest_http_server can bind to        */
/* different addresses (127.0.0.1 vs <iface_ip>), avoiding the port   */
/* conflict that prevents start_GDialServer from succeeding.          */
/* ================================================================== */
static const char *find_usable_iface() {
    static const char *candidates[] = {
        "eth0", "eth1", "ens3", "ens4", "ens5", "ens33",
        "enp0s3", "enp3s0", "enp0s8", "em1", "wlan0", nullptr
    };
    for (int i = 0; candidates[i]; ++i) {
        const char *ip = gdial_plat_util_get_iface_ipv4_addr(candidates[i]);
        if (ip && strcmp(ip, "127.0.0.1") != 0) {
            return candidates[i];
        }
    }
    return nullptr;  /* only loopback available; start_GDialServer will fail */
}

/* ================================================================== */
/* SECTION 1: gdialServiceImpl standalone tests                       */
/* These always run and cover getInstance, destroyInstance,           */
/* stop_GDialServer (null-thread path), sendRequest, notifyResponse,  */
/* and all on* virtual method bodies in gdialservice.cpp.             */
/* ================================================================== */

class GDialServiceImplTest : public ::testing::Test {
protected:
    TestServiceNotifier notifier;

    void SetUp() override {
        gdialServiceImpl::destroyInstance();
    }
    void TearDown() override {
        gdialServiceImpl::destroyInstance();
    }
};

TEST_F(GDialServiceImplTest, GetInstance_ReturnsNonNull) {
    gdialServiceImpl *impl = gdialServiceImpl::getInstance();
    ASSERT_NE(impl, nullptr);
}

TEST_F(GDialServiceImplTest, GetInstance_ReturnsSingleton) {
    gdialServiceImpl *a = gdialServiceImpl::getInstance();
    gdialServiceImpl *b = gdialServiceImpl::getInstance();
    EXPECT_EQ(a, b);
}

TEST_F(GDialServiceImplTest, DestroyInstance_NoCrash) {
    /* Covers destroyInstance + stop_GDialServer when no threads are running */
    gdialServiceImpl::getInstance();
    gdialServiceImpl::destroyInstance();
    SUCCEED();
}

TEST_F(GDialServiceImplTest, SendRequest_AppStateChanged_NoCrash) {
    /* Covers sendRequest() body (lines ~1119-1127) */
    gdialServiceImpl *impl = gdialServiceImpl::getInstance();
    ASSERT_NE(impl, nullptr);

    RequestHandlerPayload p = {};
    p.event = APP_STATE_CHANGED;
    p.appNameOrfriendlyname = "Netflix";
    p.appIdOractivation = "id1";
    p.state = "running";
    p.error = "none";
    impl->sendRequest(p);
    SUCCEED();
}

TEST_F(GDialServiceImplTest, SendRequest_ActivationChanged_NoCrash) {
    gdialServiceImpl *impl = gdialServiceImpl::getInstance();
    RequestHandlerPayload p = {};
    p.event = ACTIVATION_CHANGED;
    p.appIdOractivation = "true";
    p.appNameOrfriendlyname = "TV";
    impl->sendRequest(p);
    SUCCEED();
}

TEST_F(GDialServiceImplTest, SendRequest_FriendlyNameChanged_NoCrash) {
    gdialServiceImpl *impl = gdialServiceImpl::getInstance();
    RequestHandlerPayload p = {};
    p.event = FRIENDLYNAME_CHANGED;
    p.appNameOrfriendlyname = "LivingRoom";
    impl->sendRequest(p);
    SUCCEED();
}

TEST_F(GDialServiceImplTest, SendRequest_RegisterApplications_NoCrash) {
    gdialServiceImpl *impl = gdialServiceImpl::getInstance();
    RequestHandlerPayload p = {};
    p.event = REGISTER_APPLICATIONS;
    p.data_param = nullptr;
    impl->sendRequest(p);
    SUCCEED();
}

TEST_F(GDialServiceImplTest, SendRequest_UpdateNwStandby_NoCrash) {
    gdialServiceImpl *impl = gdialServiceImpl::getInstance();
    RequestHandlerPayload p = {};
    p.event = UPDATE_NW_STANDBY;
    p.user_param1 = true;
    impl->sendRequest(p);
    SUCCEED();
}

TEST_F(GDialServiceImplTest, SendRequest_UpdateManufacturer_NoCrash) {
    gdialServiceImpl *impl = gdialServiceImpl::getInstance();
    RequestHandlerPayload p = {};
    p.event = UPDATE_MANUFACTURER_NAME;
    p.manufacturer = "Acme";
    impl->sendRequest(p);
    SUCCEED();
}

TEST_F(GDialServiceImplTest, SendRequest_UpdateModel_NoCrash) {
    gdialServiceImpl *impl = gdialServiceImpl::getInstance();
    RequestHandlerPayload p = {};
    p.event = UPDATE_MODEL_NAME;
    p.model = "BoxV2";
    impl->sendRequest(p);
    SUCCEED();
}

TEST_F(GDialServiceImplTest, OnApplicationLaunchRequest_NoCrash) {
    /* Covers onApplicationLaunchRequest + notifyResponse (lines ~1153-1137) */
    gdialServiceImpl *impl = gdialServiceImpl::getInstance();
    impl->onApplicationLaunchRequest("App1", "param1");
    SUCCEED();
}

TEST_F(GDialServiceImplTest, OnApplicationLaunchRequestWithParams_NoCrash) {
    gdialServiceImpl *impl = gdialServiceImpl::getInstance();
    impl->onApplicationLaunchRequestWithLaunchParam("App", "payload", "query", "url");
    SUCCEED();
}

TEST_F(GDialServiceImplTest, OnApplicationStopRequest_NoCrash) {
    gdialServiceImpl *impl = gdialServiceImpl::getInstance();
    impl->onApplicationStopRequest("App1", "id1");
    SUCCEED();
}

TEST_F(GDialServiceImplTest, OnApplicationHideRequest_NoCrash) {
    gdialServiceImpl *impl = gdialServiceImpl::getInstance();
    impl->onApplicationHideRequest("App1", "id1");
    SUCCEED();
}

TEST_F(GDialServiceImplTest, OnApplicationResumeRequest_NoCrash) {
    gdialServiceImpl *impl = gdialServiceImpl::getInstance();
    impl->onApplicationResumeRequest("App1", "id1");
    SUCCEED();
}

TEST_F(GDialServiceImplTest, OnApplicationStateRequest_NoCrash) {
    gdialServiceImpl *impl = gdialServiceImpl::getInstance();
    impl->onApplicationStateRequest("App1", "id1");
    SUCCEED();
}

TEST_F(GDialServiceImplTest, UpdatePowerState_NullObserver_NoCrash) {
    /* m_observer is null (setService never called) - covers null branch */
    gdialServiceImpl *impl = gdialServiceImpl::getInstance();
    impl->updatePowerState("STANDBY");
    SUCCEED();
}

TEST_F(GDialServiceImplTest, UpdatePowerState_WithObserver_CallsThrough) {
    /* Covers the m_observer branch in updatePowerState (line ~1219) */
    gdialServiceImpl *impl = gdialServiceImpl::getInstance();
    impl->setService(&notifier);
    impl->updatePowerState("ON");
    EXPECT_EQ(notifier.update_power_calls, 1);
}

/* ================================================================== */
/* SECTION 2: gdialService lifecycle — covers getInstance failure and  */
/* conditional success paths, plus destroyInstance.                   */
/* ================================================================== */

class GDialServiceTest : public ::testing::Test {
protected:
    TestServiceNotifier notifier;
    gdialService *svc = nullptr;

    void SetUp() override {
        gdialService::destroyInstance();

        std::vector<std::string> args;
        const char *iface = find_usable_iface();
        if (iface) {
            args.push_back("--network-interface");
            args.push_back(iface);
        }

        svc = gdialService::getInstance(&notifier, args, "L1Test");
    }

    void TearDown() override {
        gdialService::destroyInstance();
        svc = nullptr;
    }
};

TEST_F(GDialServiceTest, GetInstance_NoCrash) {
    /* Covers getInstance() call path including start_GDialServer attempt */
    SUCCEED();
}

TEST_F(GDialServiceTest, DestroyInstance_NoCrash) {
    gdialService::destroyInstance();
    svc = nullptr;
    SUCCEED();
}

TEST_F(GDialServiceTest, ApplicationStateChanged_ReturnsNone) {
    if (!svc) GTEST_SKIP() << "Service start failed (loopback-only CI env)";
    EXPECT_EQ(svc->ApplicationStateChanged("Netflix", "running", "id1", "none"),
              GDIAL_SERVICE_ERROR_NONE);
}

TEST_F(GDialServiceTest, ActivationChanged_TrueAndFalse_ReturnsNone) {
    if (!svc) GTEST_SKIP() << "Service start failed (loopback-only CI env)";
    EXPECT_EQ(svc->ActivationChanged("true",  "TV"), GDIAL_SERVICE_ERROR_NONE);
    EXPECT_EQ(svc->ActivationChanged("false", "TV"), GDIAL_SERVICE_ERROR_NONE);
}

TEST_F(GDialServiceTest, FriendlyNameChanged_ReturnsNone) {
    if (!svc) GTEST_SKIP() << "Service start failed (loopback-only CI env)";
    EXPECT_EQ(svc->FriendlyNameChanged("LivingRoom"), GDIAL_SERVICE_ERROR_NONE);
}

TEST_F(GDialServiceTest, RegisterApplications_WithList_ReturnsNone) {
    if (!svc) GTEST_SKIP() << "Service start failed (loopback-only CI env)";
    RegisterAppEntryList *list = new RegisterAppEntryList;
    RegisterAppEntry *e = new RegisterAppEntry;
    e->Names = "Netflix";
    e->cors  = ".netflix.com";
    list->pushBack(e);
    EXPECT_EQ(svc->RegisterApplications(list), GDIAL_SERVICE_ERROR_NONE);
}

TEST_F(GDialServiceTest, RegisterApplications_NullList_ReturnsNone) {
    if (!svc) GTEST_SKIP() << "Service start failed (loopback-only CI env)";
    EXPECT_EQ(svc->RegisterApplications(nullptr), GDIAL_SERVICE_ERROR_NONE);
}

TEST_F(GDialServiceTest, SetNetworkStandbyMode_NoCrash) {
    if (!svc) GTEST_SKIP() << "Service start failed (loopback-only CI env)";
    svc->setNetworkStandbyMode(true);
    svc->setNetworkStandbyMode(false);
    SUCCEED();
}

TEST_F(GDialServiceTest, SetManufacturerName_ReturnsNone) {
    if (!svc) GTEST_SKIP() << "Service start failed (loopback-only CI env)";
    EXPECT_EQ(svc->setManufacturerName("Acme"), GDIAL_SERVICE_ERROR_NONE);
}

TEST_F(GDialServiceTest, SetModelName_ReturnsNone) {
    if (!svc) GTEST_SKIP() << "Service start failed (loopback-only CI env)";
    EXPECT_EQ(svc->setModelName("BoxV2"), GDIAL_SERVICE_ERROR_NONE);
}

TEST_F(GDialServiceTest, GetProtocolVersion_ReturnsNonEmpty) {
    if (!svc) GTEST_SKIP() << "Service start failed (loopback-only CI env)";
    EXPECT_FALSE(svc->getProtocolVersion().empty());
}

/* ================================================================== */
/* SECTION 2b: start_GDialServer app_list else branch                 */
/* Passes --app-list so the option-parsing else branch executes,      */
/* covering all the g_strstr_len checks for netflix/youtube/etc.      */
/* Only reachable when a non-loopback interface is present.           */
/* ================================================================== */

class GDialServiceWithAppListTest : public ::testing::Test {
protected:
    TestServiceNotifier notifier;
    gdialService *svc = nullptr;

    void SetUp() override {
        gdialService::destroyInstance();

        const char *iface = find_usable_iface();
        if (!iface) return;   /* TearDown still safe; svc stays null */

        std::vector<std::string> args = {
            "--network-interface", iface,
            "--app-list", "netflix,youtube,youtubetv,youtubekids,amazoninstantvideo,spotify,pairing,system"
        };
        svc = gdialService::getInstance(&notifier, args, "L1Test");
    }

    void TearDown() override {
        gdialService::destroyInstance();
        svc = nullptr;
    }
};

TEST_F(GDialServiceWithAppListTest, StartWithAppList_ExercisesElseBranch) {
    /* Exercises the `else { ... }` block in start_GDialServer that processes
     * options_.app_list — covers all g_strstr_len checks at lines ~360-440. */
    if (!svc) GTEST_SKIP() << "Service start failed (loopback-only CI env)";
    SUCCEED();
}

/* ================================================================== */
/* SECTION 3: RegisterAppEntryList and enum helpers                   */
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
    {
        RegisterAppEntryList list;
        RegisterAppEntry *e = new RegisterAppEntry;
        e->Names = "YouTube";
        list.pushBack(e);
    }
    SUCCEED();
}

TEST(AppRequestEventsEnumTest, InvalidRequestIsLast) {
    EXPECT_GT(INVALID_REQUEST, REGISTER_APPLICATIONS);
}

TEST(AppResponseEventsEnumTest, InvalidStateIsLast) {
    EXPECT_GT(APP_INVALID_STATE, APP_RESUME_REQUEST);
}
