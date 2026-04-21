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

#include "gdialappcache.hpp"

class GDialAppCacheTest : public ::testing::Test {
protected:
    GDialAppStatusCache cache;
    std::string original_netflix_id;
    std::string original_youtube_id;

    void SetUp() override
    {
        original_netflix_id = cache.getAppCacheId("Netflix");
        original_youtube_id = cache.getAppCacheId("YouTube");
    }

    void TearDown() override
    {
        // Restore static cache IDs so tests remain isolated.
        cache.setAppCacheId("Netflix", original_netflix_id);
        cache.setAppCacheId("YouTube", original_youtube_id);
    }
};

TEST_F(GDialAppCacheTest, SetAppCacheId_UpdatesNetflixId)
{
    cache.setAppCacheId("Netflix", "DialNetflix_L1");
    EXPECT_EQ(cache.getAppCacheId("Netflix"), "DialNetflix_L1");
}

TEST_F(GDialAppCacheTest, SetAppCacheId_UpdatesYoutubeId)
{
    cache.setAppCacheId("YouTube", "DialYouTube_L1");
    EXPECT_EQ(cache.getAppCacheId("YouTube"), "DialYouTube_L1");
}

TEST_F(GDialAppCacheTest, SetAppCacheId_UnknownAppDoesNotChangeKnownIds)
{
    cache.setAppCacheId("UnknownApp", "Unknown_L1");
    EXPECT_EQ(cache.getAppCacheId("Netflix"), original_netflix_id);
    EXPECT_EQ(cache.getAppCacheId("YouTube"), original_youtube_id);
}
