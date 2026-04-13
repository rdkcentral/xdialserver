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
#include <cstring>

extern "C" {
#include "gdial-plat-util.h"
#include "gdialservicelogging.h"
}

TEST(GDialPlatUtilTest, GetIfaceIpv4Addr_InvalidIfaceReturnsNull) {
    const char *ip = gdial_plat_util_get_iface_ipv4_addr("iface_does_not_exist_123");
    EXPECT_EQ(ip, nullptr);
}

TEST(GDialPlatUtilTest, GetIfaceIpv4Addr_LoopbackLooksValid) {
    const char *ip = gdial_plat_util_get_iface_ipv4_addr("lo");
    ASSERT_NE(ip, nullptr);
    EXPECT_STREQ(ip, "127.0.0.1");
}

TEST(GDialPlatUtilTest, GetIfaceMacAddr_InvalidIfaceReturnsNull) {
    const char *mac = gdial_plat_util_get_iface_mac_addr("iface_does_not_exist_123");
    EXPECT_EQ(mac, nullptr);
}

TEST(GDialPlatUtilTest, GetIfaceMacAddr_LoopbackLooksLikeMac) {
    const char *mac = gdial_plat_util_get_iface_mac_addr("lo");
    ASSERT_NE(mac, nullptr);
    EXPECT_EQ(std::strlen(mac), 17u);
    for (size_t i = 0; i < 17; ++i) {
        if ((i + 1) % 3 == 0) {
            EXPECT_EQ(mac[i], ':');
        }
    }
}

TEST(GDialPlatUtilLogTest, LoggerInitAndSetLevelDoNotCrash) {
    gdial_plat_util_logger_init();
    gdial_plat_util_set_loglevel(INFO_LEVEL);
    SUCCEED();
}

TEST(GDialPlatUtilLogTest, LogAtFatalDoesNotCrash) {
    gdial_plat_util_log(FATAL_LEVEL, "fn", "file.c", 1, 123, "fatal %d", 1);
    SUCCEED();
}

TEST(GDialPlatUtilLogTest, LogAtVerboseDoesNotCrash) {
    gdial_plat_util_set_loglevel(TRACE_LEVEL);
    gdial_plat_util_log(VERBOSE_LEVEL, "fn", "file.c", 2, 124, "verbose %s", "ok");
    SUCCEED();
}
