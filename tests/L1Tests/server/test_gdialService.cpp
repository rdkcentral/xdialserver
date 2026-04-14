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

#include <string>

#include "gdialservicecommon.h"
#include "gdialserviceimpl.h"

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
