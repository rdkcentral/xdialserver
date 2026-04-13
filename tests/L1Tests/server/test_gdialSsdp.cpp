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

extern "C" {
#include "gdial-ssdp.h"
void gdial_ssdp_networkstandbymode_handler(const bool nwstandby);
}

namespace {

class GDialSsdpTest : public ::testing::Test {
protected:
    void TearDown() override {
        // Keep global static state tidy for subsequent tests.
        gdial_ssdp_set_friendlyname(NULL);
        gdial_ssdp_set_manufacturername(NULL);
        gdial_ssdp_set_modelname(NULL);
    }
};

TEST_F(GDialSsdpTest, Setters_NoCrash) {
    EXPECT_EQ(gdial_ssdp_set_friendlyname("Living Room"), 0);
    EXPECT_EQ(gdial_ssdp_set_manufacturername("Acme"), 0);
    EXPECT_EQ(gdial_ssdp_set_modelname("ModelX"), 0);
}

TEST_F(GDialSsdpTest, Setters_NullInput_NoCrash) {
    EXPECT_EQ(gdial_ssdp_set_friendlyname(NULL), 0);
    EXPECT_EQ(gdial_ssdp_set_manufacturername(NULL), 0);
    EXPECT_EQ(gdial_ssdp_set_modelname(NULL), 0);
}

TEST_F(GDialSsdpTest, SetAvailable_NoInit_NoCrash) {
    EXPECT_EQ(gdial_ssdp_set_available(true, "Kitchen"), 0);
    EXPECT_EQ(gdial_ssdp_set_available(false, "Kitchen"), 0);
}

TEST_F(GDialSsdpTest, NetworkStandbyHandler_NoInit_NoCrash) {
    gdial_ssdp_networkstandbymode_handler(true);
    gdial_ssdp_networkstandbymode_handler(false);
    SUCCEED();
}

}  // namespace
