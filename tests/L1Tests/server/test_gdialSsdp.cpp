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
#include "gdial-ssdp.h"
#include "gdial-options.h"
void gdial_ssdp_networkstandbymode_handler(const bool nwstandby);
}

namespace {

class GDialSsdpTest : public ::testing::Test {
protected:
    SoupServer *server = nullptr;

    static std::string BuildServerBaseUrl(SoupServer *srv) {
        GSList *uris = soup_server_get_uris(srv);
        if (!uris) return "";
        SoupURI *uri = static_cast<SoupURI *>(uris->data);
        if (!uri) {
            g_slist_free(uris);
            return "";
        }

        char *uri_str = soup_uri_to_string(uri, FALSE);
        std::string base = uri_str ? uri_str : "";
        g_free(uri_str);
        soup_uri_free(uri);
        g_slist_free(uris);
        return base;
    }

    void SetUp() override {
        server = soup_server_new(nullptr, nullptr);
        ASSERT_NE(server, nullptr);
    }

    void TearDown() override {
        if (server) {
            g_object_unref(server);
            server = nullptr;
        }
    }
};

TEST_F(GDialSsdpTest, New_NullServerRejected) {
    GDialOptions opt = {};
    opt.iface_name = g_strdup("lo");
    EXPECT_EQ(gdial_ssdp_new(nullptr, &opt, "uuid1"), -1);
    g_free(opt.iface_name);
}

TEST_F(GDialSsdpTest, New_NullOptionsRejected) {
    EXPECT_EQ(gdial_ssdp_new(server, nullptr, "uuid1"), -1);
}

TEST_F(GDialSsdpTest, New_NullIfaceRejected) {
    GDialOptions opt = {};
    opt.iface_name = nullptr;
    EXPECT_EQ(gdial_ssdp_new(server, &opt, "uuid1"), -1);
}

TEST_F(GDialSsdpTest, Setters_WithValues_NoCrash) {
    EXPECT_EQ(gdial_ssdp_set_friendlyname("Living Room"), 0);
    EXPECT_EQ(gdial_ssdp_set_manufacturername("Acme"), 0);
    EXPECT_EQ(gdial_ssdp_set_modelname("ModelX"), 0);
}

TEST_F(GDialSsdpTest, Setters_WithNull_NoCrash) {
    EXPECT_EQ(gdial_ssdp_set_friendlyname(nullptr), 0);
    EXPECT_EQ(gdial_ssdp_set_manufacturername(nullptr), 0);
    EXPECT_EQ(gdial_ssdp_set_modelname(nullptr), 0);
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

TEST_F(GDialSsdpTest, SsdpHttpCallback_GetDdXmlReturnsOkAndHeaders) {
    GError *error = nullptr;
    ASSERT_TRUE(soup_server_listen_local(server, 0, SOUP_SERVER_LISTEN_IPV4_ONLY, &error));
    ASSERT_EQ(error, nullptr);

    GDialOptions opt = {};
    opt.iface_name = g_strdup("lo");
    opt.friendly_name = g_strdup("L1Friendly");
    opt.manufacturer = g_strdup("L1Maker");
    opt.model_name = g_strdup("L1Model");
    opt.uuid = g_strdup("12345678-abcd-abcd-1234-123456789abc");

    const char *uuid = "uuid_ut";
    ASSERT_EQ(gdial_ssdp_new(server, &opt, uuid), 0);

    std::string base = BuildServerBaseUrl(server);
    ASSERT_FALSE(base.empty());
    std::string url = base + uuid + "/dd.xml";

    /*
     * Use async queue_message + g_main_loop_run so that both the outbound
     * client I/O and the SoupServer's inbound dispatch are handled by the
     * same g_main_context_default() iteration.  Blocking send_message
     * deadlocks because the server needs the context iterated while the
     * test thread is blocked on the socket waiting for a response.
     */
    struct Ctx {
        GMainLoop *loop;
        guint      status = 0;
        std::string app_url;
        std::string body;
    } ctx;
    ctx.loop = g_main_loop_new(nullptr, FALSE);

    /* Safety watchdog — quits the loop if no response arrives in 5 s. */
    GSource *watchdog = g_timeout_source_new_seconds(5);
    g_source_set_callback(watchdog,
        [](gpointer d) -> gboolean {
            g_main_loop_quit(static_cast<GMainLoop *>(d));
            return G_SOURCE_REMOVE;
        }, ctx.loop, nullptr);
    g_source_attach(watchdog, nullptr);
    g_source_unref(watchdog);

    SoupSession *session = soup_session_new_with_options(
        SOUP_SESSION_TIMEOUT, (guint)5, nullptr);
    ASSERT_NE(session, nullptr);
    SoupMessage *msg = soup_message_new("GET", url.c_str());
    ASSERT_NE(msg, nullptr);

    /* queue_message transfers ownership of msg to the session. */
    soup_session_queue_message(session, msg,
        [](SoupSession *, SoupMessage *m, gpointer d) {
            auto *c = static_cast<Ctx *>(d);
            c->status = m->status_code;
            const char *au = soup_message_headers_get_one(
                m->response_headers, "Application-URL");
            c->app_url = au ? au : "";
            if (m->response_body && m->response_body->data)
                c->body.assign(m->response_body->data,
                               (std::string::size_type)m->response_body->length);
            g_main_loop_quit(c->loop);
        }, &ctx);

    g_main_loop_run(ctx.loop);
    g_main_loop_unref(ctx.loop);
    g_object_unref(session);

    EXPECT_EQ(ctx.status, (guint)SOUP_STATUS_OK);
    EXPECT_FALSE(ctx.app_url.empty());
    EXPECT_NE(ctx.body.find("<friendlyName>L1Friendly</friendlyName>"), std::string::npos);
    EXPECT_NE(ctx.body.find("<manufacturer>L1Maker</manufacturer>"), std::string::npos);
    EXPECT_NE(ctx.body.find("<modelName>L1Model</modelName>"), std::string::npos);

    gdial_ssdp_destroy();
}

TEST_F(GDialSsdpTest, SsdpHttpCallback_NonGetReturnsBadRequest) {
    GError *error = nullptr;
    ASSERT_TRUE(soup_server_listen_local(server, 0, SOUP_SERVER_LISTEN_IPV4_ONLY, &error));
    ASSERT_EQ(error, nullptr);

    GDialOptions opt = {};
    opt.iface_name = g_strdup("lo");
    opt.friendly_name = g_strdup("L1Friendly");
    opt.manufacturer = g_strdup("L1Maker");
    opt.model_name = g_strdup("L1Model");
    opt.uuid = g_strdup("12345678-abcd-abcd-1234-123456789abc");

    const char *uuid = "uuid_ut";
    ASSERT_EQ(gdial_ssdp_new(server, &opt, uuid), 0);

    std::string base = BuildServerBaseUrl(server);
    ASSERT_FALSE(base.empty());
    std::string url = base + uuid + "/dd.xml";

    struct Ctx {
        GMainLoop *loop;
        guint      status = 0;
    } ctx;
    ctx.loop = g_main_loop_new(nullptr, FALSE);

    GSource *watchdog = g_timeout_source_new_seconds(5);
    g_source_set_callback(watchdog,
        [](gpointer d) -> gboolean {
            g_main_loop_quit(static_cast<GMainLoop *>(d));
            return G_SOURCE_REMOVE;
        }, ctx.loop, nullptr);
    g_source_attach(watchdog, nullptr);
    g_source_unref(watchdog);

    SoupSession *session = soup_session_new_with_options(
        SOUP_SESSION_TIMEOUT, (guint)5, nullptr);
    ASSERT_NE(session, nullptr);
    SoupMessage *msg = soup_message_new("POST", url.c_str());
    ASSERT_NE(msg, nullptr);

    soup_session_queue_message(session, msg,
        [](SoupSession *, SoupMessage *m, gpointer d) {
            auto *c = static_cast<Ctx *>(d);
            c->status = m->status_code;
            g_main_loop_quit(c->loop);
        }, &ctx);

    g_main_loop_run(ctx.loop);
    g_main_loop_unref(ctx.loop);
    g_object_unref(session);

    EXPECT_EQ(ctx.status, (guint)SOUP_STATUS_BAD_REQUEST);

    gdial_ssdp_destroy();
}

}  // namespace
