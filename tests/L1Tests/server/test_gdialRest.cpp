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
#include <string>

extern "C" {
#include <glib.h>
#include <libsoup/soup.h>
#include "gdial-rest.h"
#include "gdial-rest-builder.h"
#include "gdial-app.h"

void gdial_plat_stub_reset_behavior(void);
void gdial_plat_stub_set_app_state(GDialAppState state);
void gdial_plat_stub_set_errors(
    GDialAppError start_err,
    GDialAppError hide_err,
    GDialAppError resume_err,
    GDialAppError stop_err,
    GDialAppError state_err);
}

class GDialRestServerTest : public ::testing::Test {
protected:
    const char *rest_route_id = "apps123";
    SoupServer *rest_server = nullptr;
    SoupServer *local_rest_server = nullptr;
    GDialRestServer *server = nullptr;
    SoupSession *session = nullptr;
    GMainLoop *main_loop = nullptr;
    GThread *main_loop_thread = nullptr;
    std::string rest_base;
    std::string local_base;

    void SetUp() override {
        gdial_plat_stub_reset_behavior();

        rest_server = soup_server_new(nullptr, nullptr);
        local_rest_server = soup_server_new(nullptr, nullptr);
        ASSERT_NE(rest_server, nullptr);
        ASSERT_NE(local_rest_server, nullptr);

        GError *error = nullptr;
        ASSERT_TRUE(soup_server_listen_local(rest_server, 0, SOUP_SERVER_LISTEN_IPV4_ONLY, &error));
        ASSERT_EQ(error, nullptr);
        ASSERT_TRUE(soup_server_listen_local(local_rest_server, 0, SOUP_SERVER_LISTEN_IPV4_ONLY, &error));
        ASSERT_EQ(error, nullptr);

        GSList *rest_uris = soup_server_get_uris(rest_server);
        ASSERT_NE(rest_uris, nullptr);
        SoupURI *rest_uri = (SoupURI *)rest_uris->data;
        guint rest_port = soup_uri_get_port(rest_uri);
        rest_base = std::string("http://127.0.0.1:") + std::to_string(rest_port) + "/" + rest_route_id;
        g_slist_free_full(rest_uris, (GDestroyNotify)soup_uri_free);

        GSList *local_uris = soup_server_get_uris(local_rest_server);
        ASSERT_NE(local_uris, nullptr);
        SoupURI *local_uri = (SoupURI *)local_uris->data;
        guint local_port = soup_uri_get_port(local_uri);
        local_base = std::string("http://127.0.0.1:") + std::to_string(local_port);
        g_slist_free_full(local_uris, (GDestroyNotify)soup_uri_free);

        main_loop = g_main_loop_new(nullptr, FALSE);
        ASSERT_NE(main_loop, nullptr);
        main_loop_thread = g_thread_new(
            "gdial-rest-test-loop",
            [](gpointer data) -> gpointer {
                g_main_loop_run((GMainLoop *)data);
                return nullptr;
            },
            main_loop);
        ASSERT_NE(main_loop_thread, nullptr);

        session = soup_session_new_with_options(
            SOUP_SESSION_TIMEOUT, 5,
            SOUP_SESSION_IDLE_TIMEOUT, 5,
            NULL);
        ASSERT_NE(session, nullptr);

        server = gdial_rest_server_new(rest_server, local_rest_server, (gchar *)rest_route_id);
        ASSERT_NE(server, nullptr);
        g_object_set(server, "enable", TRUE, NULL);
    }

    void TearDown() override {
        if (session) {
            g_object_unref(session);
            session = nullptr;
        }

        if (main_loop) {
            g_main_loop_quit(main_loop);
        }
        if (main_loop_thread) {
            g_thread_join(main_loop_thread);
            main_loop_thread = nullptr;
        }
        if (main_loop) {
            g_main_loop_unref(main_loop);
            main_loop = nullptr;
        }

        if (server) {
            g_object_unref(server);
            server = nullptr;
        }

        if (rest_server) {
            g_object_unref(rest_server);
            rest_server = nullptr;
        }
        if (local_rest_server) {
            g_object_unref(local_rest_server);
            local_rest_server = nullptr;
        }

        gdial_plat_stub_reset_behavior();

        std::remove("/tmp/.dial_Netflix_uuid.txt");
        std::remove("/tmp/.dial_YouTube_uuid.txt");
    }

    static GList *make_list1(const gchar *v1) {
        GList *list = nullptr;
        list = g_list_prepend(list, (gpointer)v1);
        return list;
    }

    SoupMessage *send_rest(
        const char *method,
        const std::string &suffix,
        const char *origin = nullptr,
        const char *body = nullptr,
        const char *content_type = "application/x-www-form-urlencoded")
    {
        std::string url = rest_base + suffix;
        SoupMessage *msg = soup_message_new(method, url.c_str());
        EXPECT_NE(msg, nullptr);
        if (!msg) {
            return nullptr;
        }
        if (origin) {
            soup_message_headers_replace(msg->request_headers, "Origin", origin);
        }
        if (body) {
            soup_message_set_request(msg, content_type, SOUP_MEMORY_COPY, body, strlen(body));
        }
        soup_session_send_message(session, msg);
        return msg;
    }

    SoupMessage *send_local(
        const char *method,
        const std::string &suffix,
        const char *body = nullptr,
        const char *content_type = "application/x-www-form-urlencoded")
    {
        std::string url = local_base + suffix;
        SoupMessage *msg = soup_message_new(method, url.c_str());
        EXPECT_NE(msg, nullptr);
        if (!msg) {
            return nullptr;
        }
        if (body) {
            soup_message_set_request(msg, content_type, SOUP_MEMORY_COPY, body, strlen(body));
        }
        soup_session_send_message(session, msg);
        return msg;
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

TEST_F(GDialRestServerTest, RegisterApp_NonSingletonRejected) {
    EXPECT_FALSE(gdial_rest_server_register_app(
        server, "Netflix", nullptr, nullptr, FALSE, FALSE, nullptr));
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

TEST_F(GDialRestServerTest, FindRegistryByUuid_UnknownUuidReturnsNull) {
    ASSERT_TRUE(gdial_rest_server_register_app(
        server, "Netflix", nullptr, nullptr, TRUE, FALSE, nullptr));

    GDialAppRegistry *by_uuid =
        gdial_rest_server_find_app_registry_by_uuid(server, "not-a-real-uuid");
    EXPECT_EQ(by_uuid, nullptr);
}

TEST_F(GDialRestServerTest, FindRegistry_MatchesByPrefix) {
    GList *prefixes = make_list1("YouTube");
    ASSERT_TRUE(gdial_rest_server_register_app(
        server, "YouTube", prefixes, nullptr, TRUE, FALSE, nullptr));

    GDialAppRegistry *registry =
        gdial_rest_server_find_app_registry(server, "YouTubeTV");
    ASSERT_NE(registry, nullptr);
    EXPECT_STREQ(registry->name, "YouTube");

    g_list_free(prefixes);
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

TEST_F(GDialRestServerTest, RegisterAppRegistryAndUnregisterAllApps) {
    GDialAppRegistry *registry =
        gdial_app_registry_new("Netflix", nullptr, nullptr, TRUE, FALSE, nullptr);
    ASSERT_NE(registry, nullptr);

    ASSERT_TRUE(gdial_rest_server_register_app_registry(server, registry));
    EXPECT_TRUE(gdial_rest_server_is_app_registered(server, "Netflix"));

    GDialApp *app = gdial_app_new("Netflix");
    ASSERT_NE(app, nullptr);
    EXPECT_EQ(gdial_app_start(app, nullptr, nullptr, nullptr, nullptr), GDIAL_APP_ERROR_NONE);
    g_object_unref(app);

    EXPECT_TRUE(gdial_rest_server_unregister_all_apps(server));
    EXPECT_FALSE(gdial_rest_server_is_app_registered(server, "Netflix"));
}

TEST_F(GDialRestServerTest, RegisterAppRegistry_DuplicateRejected) {
    GDialAppRegistry *registry1 =
        gdial_app_registry_new("Netflix", nullptr, nullptr, TRUE, FALSE, nullptr);
    ASSERT_NE(registry1, nullptr);
    ASSERT_TRUE(gdial_rest_server_register_app_registry(server, registry1));

    GDialAppRegistry *registry2 =
        gdial_app_registry_new("Netflix", nullptr, nullptr, TRUE, FALSE, nullptr);
    ASSERT_NE(registry2, nullptr);
    EXPECT_FALSE(gdial_rest_server_register_app_registry(server, registry2));

    /* registry2 is not owned by server because registration failed. */
    gdial_app_regstry_dispose(registry2);
}

TEST_F(GDialRestServerTest, UnregisterUnknownAppReturnsFalse) {
    EXPECT_FALSE(gdial_rest_server_unregister_app(server, "MissingApp"));
}

TEST_F(GDialRestServerTest, EnablePropertyCanBeToggled) {
    g_object_set(server, "enable", TRUE, NULL);
    g_object_set(server, "enable", FALSE, NULL);
    SUCCEED();
}

TEST_F(GDialRestServerTest, HttpOptionsOnAppPathReturnsNoContent) {
    ASSERT_TRUE(gdial_rest_server_register_app(
        server, "Netflix", nullptr, nullptr, TRUE, FALSE, nullptr));

    SoupMessage *msg = send_rest("OPTIONS", "/Netflix");
    ASSERT_NE(msg, nullptr);
    EXPECT_EQ(msg->status_code, SOUP_STATUS_NO_CONTENT);
    EXPECT_STREQ(
        soup_message_headers_get_one(msg->response_headers, "Access-Control-Allow-Methods"),
        "GET, POST, OPTIONS");
    g_object_unref(msg);
}

TEST_F(GDialRestServerTest, HttpPostOnAppPathCreatesInstance) {
    ASSERT_TRUE(gdial_rest_server_register_app(
        server, "Netflix", nullptr, nullptr, TRUE, FALSE, nullptr));
    gdial_plat_stub_set_app_state(GDIAL_APP_STATE_RUNNING);

    SoupMessage *msg = send_rest("POST", "/Netflix", nullptr, "k=v");
    ASSERT_NE(msg, nullptr);
    EXPECT_EQ(msg->status_code, SOUP_STATUS_CREATED);
    const char *location = soup_message_headers_get_one(msg->response_headers, "Location");
    ASSERT_NE(location, nullptr);
    EXPECT_NE(strstr(location, "/apps/Netflix/run"), nullptr);
    g_object_unref(msg);
}

TEST_F(GDialRestServerTest, HttpGetOnAppPathReturnsXml) {
    ASSERT_TRUE(gdial_rest_server_register_app(
        server, "Netflix", nullptr, nullptr, TRUE, FALSE, nullptr));

    SoupMessage *msg = send_rest("GET", "/Netflix");
    ASSERT_NE(msg, nullptr);
    EXPECT_EQ(msg->status_code, SOUP_STATUS_OK);
    ASSERT_NE(msg->response_body, nullptr);
    ASSERT_NE(msg->response_body->data, nullptr);
    EXPECT_NE(strstr(msg->response_body->data, "<service"), nullptr);
    g_object_unref(msg);
}

TEST_F(GDialRestServerTest, HttpPostHidePathRunsHandlePostHide) {
    GList *allowed_origins = make_list1(".example.com");
    ASSERT_TRUE(gdial_rest_server_register_app(
        server, "Netflix", nullptr, nullptr, TRUE, FALSE, allowed_origins));

    gdial_plat_stub_set_app_state(GDIAL_APP_STATE_RUNNING);

    GDialApp *app = gdial_app_new("Netflix");
    ASSERT_NE(app, nullptr);
    app->instance_id = 1;
    app->state = GDIAL_APP_STATE_RUNNING;

    SoupMessage *msg = send_rest("POST", "/Netflix/run/hide", "https://www.example.com");
    ASSERT_NE(msg, nullptr);
    EXPECT_EQ(msg->status_code, SOUP_STATUS_OK);
    EXPECT_STREQ(
        soup_message_headers_get_one(msg->response_headers, "Content-Type"),
        "text/plain; charset=utf-8");
    EXPECT_STREQ(
        soup_message_headers_get_one(msg->response_headers, "Access-Control-Allow-Origin"),
        "https://www.example.com");

    g_object_unref(msg);
    g_object_unref(app);
    g_list_free(allowed_origins);
}

TEST_F(GDialRestServerTest, HttpDeleteOnRunPathRunsHandleDelete) {
    ASSERT_TRUE(gdial_rest_server_register_app(
        server, "Netflix", nullptr, nullptr, TRUE, FALSE, nullptr));
    gdial_plat_stub_set_app_state(GDIAL_APP_STATE_RUNNING);

    GDialApp *app = gdial_app_new("Netflix");
    ASSERT_NE(app, nullptr);
    app->instance_id = 1;
    app->state = GDIAL_APP_STATE_RUNNING;

    SoupMessage *msg = send_rest("DELETE", "/Netflix/run");
    ASSERT_NE(msg, nullptr);
    EXPECT_EQ(msg->status_code, SOUP_STATUS_OK);
    g_object_unref(msg);
}

TEST_F(GDialRestServerTest, LocalPostDialDataPathReturnsOk) {
    ASSERT_TRUE(gdial_rest_server_register_app(
        server, "Netflix", nullptr, nullptr, TRUE, FALSE, nullptr));
    GDialAppRegistry *registry = gdial_rest_server_find_app_registry(server, "Netflix");
    ASSERT_NE(registry, nullptr);

    std::string path = std::string(registry->app_uri) + "/dial_data";
    SoupMessage *msg = send_local("POST", path, "a=1&b=2");
    ASSERT_NE(msg, nullptr);
    EXPECT_EQ(msg->status_code, SOUP_STATUS_OK);
    g_object_unref(msg);
}

TEST(GDialRestBuilderTest, BuildRunningResponseIncludesLinkAndOptions) {
    void *builder = GET_APP_response_builder_new("Netflix");
    ASSERT_NE(builder, nullptr);

    GET_APP_response_builder_set_option(builder, "allowStop", "true");
    GET_APP_response_builder_set_state(builder, GDIAL_APP_STATE_RUNNING);
    GET_APP_response_builder_set_link_href(builder, "run?q=1");
    GET_APP_response_builder_set_installable(builder, "http://example/install");
    GET_APP_response_builder_set_additionalData(builder, "k=v");

    gsize len = 0;
    gchar *xml = GET_APP_response_builder_build(builder, &len);
    ASSERT_NE(xml, nullptr);
    EXPECT_GT(len, 0u);
    EXPECT_NE(strstr(xml, "<name>Netflix</name>"), nullptr);
    EXPECT_NE(strstr(xml, "allowStop=\"true\""), nullptr);
    EXPECT_NE(strstr(xml, "<state>running</state>"), nullptr);
    EXPECT_NE(strstr(xml, "<link rel=\"run\""), nullptr);

    g_free(xml);
    GET_APP_response_builder_destroy(builder);
}

TEST(GDialRestBuilderTest, BuildStoppedResponseOmitsLink) {
    void *builder = GET_APP_response_builder_new("Netflix");
    ASSERT_NE(builder, nullptr);

    GET_APP_response_builder_set_state(builder, GDIAL_APP_STATE_STOPPED);
    GET_APP_response_builder_set_link_href(builder, NULL);

    gsize len = 0;
    gchar *xml = GET_APP_response_builder_build(builder, &len);
    ASSERT_NE(xml, nullptr);
    EXPECT_GT(len, 0u);
    EXPECT_NE(strstr(xml, "<state>stopped</state>"), nullptr);
    EXPECT_EQ(strstr(xml, "<link rel=\"run\""), nullptr);

    g_free(xml);
    GET_APP_response_builder_destroy(builder);
}
