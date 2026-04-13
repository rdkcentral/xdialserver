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
#include <cstdio>
#include <cstring>

extern "C" {
#include <glib.h>
#include <libsoup/soup.h>
#include "gdial-rest.h"
}

class GDialRestServerTest : public ::testing::Test {
protected:
    SoupServer *rest_server = nullptr;
    SoupServer *local_rest_server = nullptr;
    GDialRestServer *server = nullptr;

    void SetUp() override {
        rest_server = soup_server_new(nullptr, nullptr);
        local_rest_server = soup_server_new(nullptr, nullptr);
        ASSERT_NE(rest_server, nullptr);
        ASSERT_NE(local_rest_server, nullptr);

        server = gdial_rest_server_new(rest_server, local_rest_server, (gchar *)"apps");
        ASSERT_NE(server, nullptr);

        /* gdial_rest_server_new takes refs on both servers. */
        g_object_unref(rest_server);
        g_object_unref(local_rest_server);
        rest_server = nullptr;
        local_rest_server = nullptr;
    }

    void TearDown() override {
        if (server) {
            g_object_unref(server);
            server = nullptr;
        }

        std::remove("/tmp/.dial_Netflix_uuid.txt");
        std::remove("/tmp/.dial_YouTube_uuid.txt");
    }

    static GList *make_list1(const gchar *v1) {
        GList *list = nullptr;
        list = g_list_prepend(list, (gpointer)v1);
        return list;
    }
};

TEST(GDialRestHelperTest, NewAdditionalDataUrl_UnencodedLooksCorrect) {
    gchar *url = gdial_rest_server_new_additional_data_url(56890, "Netflix", FALSE, "/abcd");
    ASSERT_NE(url, nullptr);
    EXPECT_STREQ(url, "http://localhost:56890/abcd/dial_data");
    g_free(url);
}

TEST(GDialRestHelperTest, NewAdditionalDataUrl_EncodedLooksUrlEncoded) {
    gchar *url = gdial_rest_server_new_additional_data_url(1234, "Netflix", TRUE, "/abcd");
    ASSERT_NE(url, nullptr);
    EXPECT_NE(strstr(url, "http%3A%2F%2Flocalhost%3A1234%2Fabcd%2Fdial_data"), nullptr);
    g_free(url);
}

TEST_F(GDialRestServerTest, RegisterFindAndUnregisterAppByName) {
    GList *allowed_origins = make_list1(".example.com");

    ASSERT_TRUE(gdial_rest_server_register_app(
        server, "Netflix", nullptr, nullptr, TRUE, FALSE, allowed_origins));

    EXPECT_TRUE(gdial_rest_server_is_app_registered(server, "Netflix"));

    GDialAppRegistry *registry = gdial_rest_server_find_app_registry(server, "Netflix");
    ASSERT_NE(registry, nullptr);
    EXPECT_STREQ(registry->name, "Netflix");

    EXPECT_TRUE(gdial_rest_server_unregister_app(server, "Netflix"));
    EXPECT_FALSE(gdial_rest_server_is_app_registered(server, "Netflix"));

    g_list_free(allowed_origins);
}

TEST_F(GDialRestServerTest, RegisterDuplicateAppFails) {
    ASSERT_TRUE(gdial_rest_server_register_app(
        server, "Netflix", nullptr, nullptr, TRUE, FALSE, nullptr));
    EXPECT_FALSE(gdial_rest_server_register_app(
        server, "Netflix", nullptr, nullptr, TRUE, FALSE, nullptr));
}

TEST_F(GDialRestServerTest, FindRegistryByUuidMatchesRegisteredAppUri) {
    ASSERT_TRUE(gdial_rest_server_register_app(
        server, "Netflix", nullptr, nullptr, TRUE, FALSE, nullptr));

    GDialAppRegistry *by_name = gdial_rest_server_find_app_registry(server, "Netflix");
    ASSERT_NE(by_name, nullptr);
    ASSERT_TRUE(by_name->app_uri[0] == '/');

    GDialAppRegistry *by_uuid =
        gdial_rest_server_find_app_registry_by_uuid(server, &by_name->app_uri[1]);
    ASSERT_NE(by_uuid, nullptr);
    EXPECT_EQ(by_uuid, by_name);
}

TEST_F(GDialRestServerTest, AllowedOrigin_NonYouTubeBehavior) {
    GList *allowed_origins = make_list1(".example.com");
    ASSERT_TRUE(gdial_rest_server_register_app(
        server, "Netflix", nullptr, nullptr, TRUE, FALSE, allowed_origins));

    EXPECT_TRUE(gdial_rest_server_is_allowed_origin(server, nullptr, "Netflix"));
    EXPECT_TRUE(gdial_rest_server_is_allowed_origin(server, "", "Netflix"));
    EXPECT_TRUE(gdial_rest_server_is_allowed_origin(server, "https://www.example.com", "Netflix"));
    EXPECT_FALSE(gdial_rest_server_is_allowed_origin(server, "https://evil.org", "Netflix"));
    EXPECT_TRUE(gdial_rest_server_is_allowed_origin(server, "http://evil.org", "Netflix"));

    g_list_free(allowed_origins);
}

TEST_F(GDialRestServerTest, AllowedOrigin_YouTubeRequiresSpecificOriginRules) {
    GList *allowed_origins = make_list1(".youtube.com");
    ASSERT_TRUE(gdial_rest_server_register_app(
        server, "YouTube", nullptr, nullptr, TRUE, FALSE, allowed_origins));

    EXPECT_FALSE(gdial_rest_server_is_allowed_origin(server, nullptr, "YouTube"));
    EXPECT_TRUE(gdial_rest_server_is_allowed_origin(server, "https://www.youtube.com", "YouTube"));
    EXPECT_FALSE(gdial_rest_server_is_allowed_origin(server, "https://www.example.com", "YouTube"));
    EXPECT_TRUE(gdial_rest_server_is_allowed_origin(server, "package:youtube", "YouTube"));

    g_list_free(allowed_origins);
}
