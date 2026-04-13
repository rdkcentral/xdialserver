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
#include <cstring>

extern "C" {
#include <glib.h>
#include <libsoup/soup.h>
#include "gdial-ssdp.h"
#include "gdial-options.h"
}

namespace {

class GDialSsdpTest : public ::testing::Test {
protected:
    SoupServer *server = nullptr;
    SoupSession *session = nullptr;
    GMainLoop *main_loop = nullptr;
    GThread *main_loop_thread = nullptr;
    std::string base_url;
    bool ssdp_started = false;

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

        main_loop = g_main_loop_new(nullptr, FALSE);
        ASSERT_NE(main_loop, nullptr);
        main_loop_thread = g_thread_new(
            "gdial-ssdp-test-loop",
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
        if (ssdp_started) {
            gdial_ssdp_destroy();
            ssdp_started = false;
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

    static GDialOptions make_options(bool feature_friendlyname) {
        GDialOptions opt = {};
        opt.iface_name = g_strdup("lo");
        opt.feature_friendlyname = feature_friendlyname;
        opt.feature_wolwake = FALSE;
        return opt;
    }
};

TEST_F(GDialSsdpTest, NewAndDestroy_DefaultDdXmlResponse) {
    const char *random_uuid = "apps123";
    GDialOptions opt = make_options(FALSE);

    ASSERT_EQ(gdial_ssdp_new(server, &opt, random_uuid), 0);
    ssdp_started = true;

    SoupMessage *msg = send_get(std::string("/") + random_uuid + "/dd.xml");
    ASSERT_NE(msg, nullptr);
    EXPECT_EQ(msg->status_code, SOUP_STATUS_OK);

    const char *app_url = soup_message_headers_get_one(msg->response_headers, "Application-URL");
    ASSERT_NE(app_url, nullptr);
    EXPECT_NE(strstr(app_url, "/apps123/"), nullptr);

    ASSERT_NE(msg->response_body, nullptr);
    ASSERT_NE(msg->response_body->data, nullptr);
    EXPECT_NE(strstr(msg->response_body->data, "<friendlyName>DialClient</friendlyName>"), nullptr);
    EXPECT_NE(strstr(msg->response_body->data, "<manufacturer>OEM</manufacturer>"), nullptr);
    EXPECT_NE(strstr(msg->response_body->data, "<modelName>Device</modelName>"), nullptr);

    g_object_unref(msg);
}

TEST_F(GDialSsdpTest, Setters_UpdateDdXmlWhenFeatureFriendlyNameEnabled) {
    const char *random_uuid = "appsxyz";
    GDialOptions opt = make_options(TRUE);

    ASSERT_EQ(gdial_ssdp_new(server, &opt, random_uuid), 0);
    ssdp_started = true;

    EXPECT_EQ(gdial_ssdp_set_friendlyname("Living Room"), 0);
    EXPECT_EQ(gdial_ssdp_set_manufacturername("Acme"), 0);
    EXPECT_EQ(gdial_ssdp_set_modelname("ModelX"), 0);

    SoupMessage *msg = send_get(std::string("/") + random_uuid + "/dd.xml");
    ASSERT_NE(msg, nullptr);
    EXPECT_EQ(msg->status_code, SOUP_STATUS_OK);
    ASSERT_NE(msg->response_body, nullptr);
    ASSERT_NE(msg->response_body->data, nullptr);

    EXPECT_NE(strstr(msg->response_body->data, "<friendlyName>Living Room</friendlyName>"), nullptr);
    EXPECT_NE(strstr(msg->response_body->data, "<manufacturer>Acme</manufacturer>"), nullptr);
    EXPECT_NE(strstr(msg->response_body->data, "<modelName>ModelX</modelName>"), nullptr);

    g_object_unref(msg);
}

TEST_F(GDialSsdpTest, SetAvailableAndNetworkStandbyPath_NoCrash) {
    const char *random_uuid = "appsavail";
    GDialOptions opt = make_options(FALSE);

    ASSERT_EQ(gdial_ssdp_new(server, &opt, random_uuid), 0);
    ssdp_started = true;

    EXPECT_EQ(gdial_ssdp_set_available(true, "Kitchen"), 0);
    EXPECT_EQ(gdial_ssdp_set_available(false, "Kitchen"), 0);
}

}  // namespace
