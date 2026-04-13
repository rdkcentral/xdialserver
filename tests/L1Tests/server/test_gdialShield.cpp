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

extern "C" {
#include <glib.h>
#include <libsoup/soup.h>
#include "gdial-shield.h"
}

namespace {

static void ping_handler(
    SoupServer *server,
    SoupMessage *msg,
    const char *path,
    GHashTable *query,
    SoupClientContext *client,
    gpointer user_data)
{
    (void)server;
    (void)path;
    (void)query;
    (void)client;
    (void)user_data;
    soup_message_set_status(msg, SOUP_STATUS_OK);
    const char *payload = "pong";
    soup_message_set_response(msg, "text/plain", SOUP_MEMORY_COPY, payload, 4);
}

class GDialShieldTest : public ::testing::Test {
protected:
    SoupServer *server = nullptr;
    SoupSession *session = nullptr;
    GMainLoop *main_loop = nullptr;
    GThread *main_loop_thread = nullptr;
    bool shield_inited = false;
    std::string base_url;

    void SetUp() override {
        server = soup_server_new(nullptr, nullptr);
        ASSERT_NE(server, nullptr);

        GError *error = nullptr;
        ASSERT_TRUE(soup_server_listen_local(server, 0, SOUP_SERVER_LISTEN_IPV4_ONLY, &error));
        ASSERT_EQ(error, nullptr);

        GSList *uris = soup_server_get_uris(server);
        ASSERT_NE(uris, nullptr);
        SoupURI *uri = (SoupURI *)uris->data;
        guint port = soup_uri_get_port(uri);
        base_url = std::string("http://127.0.0.1:") + std::to_string(port);
        g_slist_free_full(uris, (GDestroyNotify)soup_uri_free);

        soup_server_add_handler(server, "/ping", ping_handler, nullptr, nullptr);

        main_loop = g_main_loop_new(nullptr, FALSE);
        ASSERT_NE(main_loop, nullptr);
        main_loop_thread = g_thread_new(
            "gdial-shield-test-loop",
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
    }

    void TearDown() override {
        if (shield_inited) {
            gdial_shield_term();
            shield_inited = false;
        }

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
    }

    SoupMessage *send_get(const std::string &suffix) {
        std::string url = base_url + suffix;
        SoupMessage *msg = soup_message_new("GET", url.c_str());
        EXPECT_NE(msg, nullptr);
        if (!msg) {
            return nullptr;
        }
        soup_session_send_message(session, msg);
        return msg;
    }
};

TEST_F(GDialShieldTest, InitServerTerm_NoCrash) {
    gdial_shield_init();
    shield_inited = true;

    gdial_shield_server(server);

    gdial_shield_term();
    shield_inited = false;

    SUCCEED();
}

TEST_F(GDialShieldTest, ShieldedServer_ProcessesBasicRequest) {
    gdial_shield_init();
    shield_inited = true;

    gdial_shield_server(server);

    SoupMessage *msg = send_get("/ping");
    ASSERT_NE(msg, nullptr);
    EXPECT_EQ(msg->status_code, SOUP_STATUS_OK);
    ASSERT_NE(msg->response_body, nullptr);
    ASSERT_NE(msg->response_body->data, nullptr);
    EXPECT_NE(strstr(msg->response_body->data, "pong"), nullptr);
    g_object_unref(msg);
}

}  // namespace
