/*
 * If not stated otherwise in this file or this component's Licenses.txt file the
 * following copyright and licenses apply:
 *
 * Copyright 2019 RDK Management
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

#include <stdlib.h>
#include <signal.h>
#include <string.h>
#include <stdio.h>
#include <glib.h>
#include <libsoup/soup.h>

#include "gdial-config.h"
#include "gdial-debug.h"
#include "gdial-options.h"
#include "gdial-shield.h"
#include "gdial-ssdp.h"
#include "gdial-rest.h"
#include "gdial-plat-util.h"
#include "gdial-plat-dev.h"
#include "gdial-plat-app.h"
#include "gdial_app_registry.h"

static const char *dial_specification_copyright = "Copyright (c) 2017 Netflix, Inc. All rights reserved.";

static GDialOptions options_;

static GOptionEntry option_entries_[] = {
    {
        FRIENDLY_NAME_OPTION_LONG,
        FRIENDLY_NAME_OPTION,
        0, G_OPTION_ARG_STRING, &options_.friendly_name,
        FRIENDLY_NAME_DESCRIPTION, NULL
    },
    {
        MANUFACTURER_OPTION_LONG,
        MANUFACTURER_OPTION,
        0, G_OPTION_ARG_STRING, &options_.manufacturer,
        MANUFACTURER_DESCRIPTION, NULL
    },
    {
        MODELNAME_OPTION_LONG,
        MODELNAME_OPTION,
        0, G_OPTION_ARG_STRING, &options_.model_name,
        MODELNAME_DESCRIPTION, NULL
    },
    {
        UUID_OPTION_LONG,
        UUID_OPTION,
        0, G_OPTION_ARG_STRING, &options_.uuid,
        UUID_DESCRIPTION, NULL
    },
    {
        WAKE_OPTION_LONG,
        WAKE_OPTION,
        0, G_OPTION_ARG_STRING, &options_.wake,
        WAKE_DESCRIPTION, NULL
    },
    {
        IFNAME_OPTION_LONG,
        IFNAME_OPTION,
        0, G_OPTION_ARG_STRING, &options_.iface_name,
        IFNAME_DESCRIPTION, NULL
    },
    {
        APP_LIST_OPTION_LONG,
        APP_LIST_OPTION,
        0, G_OPTION_ARG_STRING, &options_.app_list,
        APP_LIST_DESCRIPTION, NULL
    },
    {
        FEATURE_FRIENDLYNAME_OPTION_LONG,
        0,
        0, G_OPTION_ARG_NONE, &options_.feature_friendlyname,
        FEATURE_FRIENDLYNAME_DESCRIPTION, NULL
    },
    {
        FEATURE_WOLWAKE_OPTION_LONG,
        0,
        0, G_OPTION_ARG_NONE, &options_.feature_wolwake,
        FEATURE_WOLWAKE_DESCRIPTION, NULL
    },
    { NULL }
};
static GMainLoop *loop_ = NULL;
static const gchar *iface_ipv4_address_ = NULL;

static void signal_handler_rest_server_invalid_uri(GDialRestServer *dial_rest_server, const gchar *signal_message, gpointer user_data) {
  g_return_if_fail(dial_rest_server && signal_message);
  g_printerr("signal invalid-uri: [%s]\r\n", signal_message);
}

static void signal_handler_rest_server_gmainloop_quit(GDialRestServer *dial_rest_server, const gchar *signal_message, gpointer user_data) {
  g_return_if_fail(dial_rest_server && signal_message);
  g_printerr("signal gmainloop-quit: [%s]\r\n", signal_message);
  g_print("Exiting DIAL Protocol | %s \r\n", dial_specification_copyright);
  g_main_loop_quit(loop_);
}
static GDialRestServer *dial_rest_server = NULL;

static void server_activation_handler(gboolean status, const gchar *friendlyname)
{
    g_print("server_activation_handler status :%d \n",status);
    gdial_ssdp_set_available(status,friendlyname);
    if(dial_rest_server)
    {
        g_object_set(dial_rest_server,"enable" ,status, NULL);
    }
}

static void server_register_application(gpointer data)
{
    GList* g_app_list = (GList*) data;
    g_print("server_register_application callback \n");
    if(g_app_list) {
        g_print("server_register_application appList :%d\n", g_list_length (g_app_list));
    }

    /*Remove all existing registered Apps*/
    gdial_rest_server_unregister_all_apps(dial_rest_server);
    while(g_app_list) {
        gdial_rest_server_register_app_registry (dial_rest_server, (GDialAppRegistry *)g_app_list->data);
        g_app_list = g_app_list->next;
    }

    size_t app_list_len = strlen(options_.app_list);
    gchar *app_list_low = g_ascii_strdown(options_.app_list, app_list_len);
    if (g_strstr_len(app_list_low, app_list_len , "system")) {
      g_print("Register system app -  enabled from cmdline\r\n");
      gdial_rest_server_register_app(dial_rest_server, "system", NULL, NULL, TRUE, TRUE, NULL);
    }
    else {
      g_print("Dont register system app - not enabled from cmdline\r\n");
    }

}

static void server_friendlyname_handler(const gchar * friendlyname)
{
    gdial_ssdp_set_friendlyname(friendlyname);
}

static void signal_handler_rest_server_rest_enable(GDialRestServer *dial_rest_server, const gchar *signal_message, gpointer user_data) {
  g_print(" signal_handler_rest_server_rest_enable received signal :%s \n ",signal_message );
  if(!strcmp(signal_message,"true"))
  {
      server_activation_handler(1, "");
  }
  else
  {
      server_activation_handler(0, "");
  }
}

static void gdial_http_server_throttle_callback(SoupServer *server,
            SoupMessage *msg, const gchar *path, GHashTable *query,
            SoupClientContext  *client, gpointer user_data)
{
  g_print("gdial_http_server_throttle_callback \r\n");
  soup_message_headers_replace(msg->response_headers, "Connection", "close");
  soup_message_set_status(msg, SOUP_STATUS_NOT_FOUND);
}

static void gdial_quit_thread(int signum)
{
  g_print("Exiting DIAL Server thread %d \r\n",signum);
  server_activation_handler(0, "");
  usleep(50000);               //Sleeping 50 ms to allow existing request to finish processing.
  g_print(" calling g_main_loop_quit loop_: %p \r\n",loop_);
  if(loop_)g_main_loop_quit(loop_);
  
}

int main(int argc, char *argv[]) {

  GError *error = NULL;
  GOptionContext *option_context = g_option_context_new(NULL);
  g_option_context_add_main_entries(option_context, option_entries_, NULL);

  if (!g_option_context_parse (option_context, &argc, &argv, &error)) {
    g_print ("%s\r\n", error->message);
    g_error_free(error);
    return EXIT_FAILURE;
  }

  if (!options_.iface_name) options_.iface_name =  g_strdup(GDIAL_IFACE_NAME_DEFAULT);

  #define MAX_RETRY 3
  for(int i=1;i<=MAX_RETRY;i++) {
    iface_ipv4_address_ = gdial_plat_util_get_iface_ipv4_addr(options_.iface_name);
    if (!iface_ipv4_address_) {
        g_warn_msg_if_fail(FALSE, "interface %s does not have IP\r\n", options_.iface_name);
        if(i >= MAX_RETRY )
            return EXIT_FAILURE;
        sleep(2);
    }
    else {
      break;
    }
  }
  gdial_plat_init(g_main_context_default());

  gdail_plat_register_activation_cb(server_activation_handler);
  gdail_plat_register_friendlyname_cb(server_friendlyname_handler);
  gdail_plat_register_registerapps_cb (server_register_application);

  SoupServer * rest_http_server = soup_server_new(NULL);
  SoupServer * ssdp_http_server = soup_server_new(NULL);
  SoupServer * local_rest_http_server = soup_server_new(NULL);
  soup_server_add_handler(rest_http_server, "/", gdial_http_server_throttle_callback, NULL, NULL);
  soup_server_add_handler(ssdp_http_server, "/", gdial_http_server_throttle_callback, NULL, NULL);

  GSocketAddress *listen_address = g_inet_socket_address_new_from_string(iface_ipv4_address_, GDIAL_REST_HTTP_PORT);
  gboolean success = soup_server_listen(rest_http_server, listen_address, 0, &error);
  g_object_unref (listen_address);
  if (!success) {
    g_printerr("%s\r\n", error->message);
    g_error_free(error);
    return EXIT_FAILURE;
  }
  else {
    listen_address = g_inet_socket_address_new_from_string(iface_ipv4_address_, GDIAL_SSDP_HTTP_PORT);
    success = soup_server_listen(ssdp_http_server, listen_address, 0, &error);
    g_object_unref (listen_address);
    if (!success) {
      g_printerr("%s\r\n", error->message);
      g_error_free(error);
      return EXIT_FAILURE;
    }
    else {
      success = soup_server_listen_local(local_rest_http_server, GDIAL_REST_HTTP_PORT, SOUP_SERVER_LISTEN_IPV4_ONLY, &error);
      if (!success) {
        g_printerr("%s\r\n", error->message);
        g_error_free(error);
        return EXIT_FAILURE;
      }
    }
  }

  dial_rest_server = gdial_rest_server_new(rest_http_server,local_rest_http_server);
  if (!options_.app_list) {
    g_print("no application is enabled from cmdline \r\n");
  }
  else {
    g_print("app_list to be enabled from command line %s\r\n", options_.app_list);
    size_t app_list_len = strlen(options_.app_list);
    gchar *app_list_low = g_ascii_strdown(options_.app_list, app_list_len);
    if (g_strstr_len(app_list_low, app_list_len, "netflix")) {
      g_print("netflix is enabled from cmdline\r\n");
      GList *allowed_origins = g_list_prepend(NULL, ".netflix.com");
      gdial_rest_server_register_app(dial_rest_server, "Netflix", NULL, NULL, TRUE, TRUE, allowed_origins);
      g_list_free(allowed_origins);
    }
    else {
      g_print("netflix is not enabled from cmdline\r\n");
    }

    if (g_strstr_len(app_list_low, app_list_len, "youtube")) {
      g_print("youtube is enabled from cmdline\r\n");
      GList *allowed_origins = g_list_prepend(NULL, ".youtube.com");
      gdial_rest_server_register_app(dial_rest_server, "YouTube", NULL, NULL, TRUE, TRUE, allowed_origins);
      g_list_free(allowed_origins);
    }
    else {
      g_print("youtube is not enabled from cmdline\r\n");
    }

    if (g_strstr_len(app_list_low, app_list_len, "youtubetv")) {
      g_print("youtubetv is enabled from cmdline\r\n");
      GList *allowed_origins = g_list_prepend(NULL, ".youtube.com");
      gdial_rest_server_register_app(dial_rest_server, "YouTubeTV", NULL, NULL, TRUE, TRUE, allowed_origins);
      g_list_free(allowed_origins);
    }
    else {
      g_print("youtubetv is not enabled from cmdline\r\n");
    }

    if (g_strstr_len(app_list_low, app_list_len, "amazoninstantvideo")) {
      g_print("AmazonInstantVideo is enabled from cmdline\r\n");
      GList *allowed_origins = g_list_prepend(NULL, ".amazonprime.com");
      gdial_rest_server_register_app(dial_rest_server, "AmazonInstantVideo", NULL, NULL, TRUE, TRUE, allowed_origins);
      g_list_free(allowed_origins);
    }
    else {
      g_print("AmazonInstantVideo is not enabled from cmdline\r\n");
    }

    if (g_strstr_len(app_list_low, app_list_len, "spotify")) {
      g_print("spotify is enabled from cmdline\r\n");
      GList *app_prefixes= g_list_prepend(NULL, "com.spotify");
      GList *allowed_origins = g_list_prepend(NULL, ".spotify.com");
      gdial_rest_server_register_app(dial_rest_server, "com.spotify.Spotify.TV", app_prefixes, NULL, TRUE, TRUE, allowed_origins);
      g_list_free(allowed_origins);
      g_list_free(app_prefixes);
    }
    else {
      g_print("spotify is not enabled from cmdline\r\n");
    }

    if (g_strstr_len(app_list_low, app_list_len, "pairing")) {
      g_print("pairing is enabled from cmdline\r\n");
      GList *allowed_origins = g_list_prepend(NULL, ".comcast.com");
      gdial_rest_server_register_app(dial_rest_server, "Pairing", NULL, NULL, TRUE, TRUE, allowed_origins);
      g_list_free(allowed_origins);
    }
    else {
      g_print("pairing is not enabled from cmdline\r\n");
    }

    if (g_strstr_len(app_list_low, app_list_len, "system")) {
      g_print("system is enabled from cmdline\r\n");
      gdial_rest_server_register_app(dial_rest_server, "system", NULL, NULL, TRUE, TRUE, NULL);
    }
    else {
      g_print("system is not enabled from cmdline\r\n");
    }

    g_free(app_list_low);
  }

  g_signal_connect(dial_rest_server, "invalid-uri", G_CALLBACK(signal_handler_rest_server_invalid_uri), NULL);
  g_signal_connect(dial_rest_server, "gmainloop-quit", G_CALLBACK(signal_handler_rest_server_gmainloop_quit), NULL);
  g_signal_connect(dial_rest_server, "rest-enable", G_CALLBACK(signal_handler_rest_server_rest_enable), NULL);

  gdial_ssdp_new(ssdp_http_server, &options_);
  gdial_shield_init();
  gdial_shield_server(rest_http_server);
  gdial_shield_server(ssdp_http_server);

  SoupServer * servers[] = {local_rest_http_server,rest_http_server, ssdp_http_server};
  for (int i = 0; i < sizeof(servers)/sizeof(servers[0]); i++) {
    GSList *uris = soup_server_get_uris(servers[i]);
    for (GSList *uri =  uris; uri != NULL; uri = uri->next) {
      char *uri_string = soup_uri_to_string(uri->data, FALSE);
      g_print("Listening on %s\n", uri_string);
      g_free(uri_string);
      soup_uri_free(uri->data);
    }
    g_slist_free(uris);
  }

  /*
   * Use global context
   */
  loop_ = g_main_loop_new (NULL, TRUE);
  signal(SIGTERM,gdial_quit_thread);
  g_main_loop_run (loop_);

  for (int i = 0; i < sizeof(servers)/sizeof(servers[0]); i++) {
    soup_server_disconnect(servers[i]);
    g_object_unref(servers[i]);
  }

  gdial_shield_term();
  gdial_ssdp_destroy();
  g_object_unref(dial_rest_server);
  gdial_plat_term();

  g_main_loop_unref(loop_);
  g_option_context_free(option_context);
  return 0;
}
