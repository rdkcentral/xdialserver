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
#include <string.h>
#include <stdio.h>
#include <glib.h>
#include <libsoup/soup.h>
#include <json-c/json.h>
#include <json-c/json_object.h>
#include <json-c/json_object_iterator.h>

#include "gdial-config.h"
#include "gdial-debug.h"
#include "gdial-options.h"
#include "gdial-shield.h"
#include "gdial-ssdp.h"
#include "gdial-rest.h"
#include "gdial-plat-util.h"
#include "gdial-plat-dev.h"
#include "gdial-plat-app.h"

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

static void server_activation_handler(gboolean status)
{
    g_print("server_activation_handler status :%d\n",status);
    gdial_ssdp_set_available(status);
    if(dial_rest_server)
    {
        g_object_set(dial_rest_server,"enable" , status, NULL);
    }
}

static void signal_handler_rest_server_rest_enable(GDialRestServer *dial_rest_server, const gchar *signal_message, gpointer user_data) {
  g_print(" signal_handler_rest_server_rest_enable received signal :%s \n ",signal_message );
  if(!strcmp(signal_message,"true"))
  {
      server_activation_handler(1);
  }
  else
  {
      server_activation_handler(0);
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

static char* get_app_name(const char *config_name)
{
    static int prefix_len = strlen("/apps/");
    static int suffix_len = strlen("/dial_data");

    int size = strlen(config_name);
    int app_name_size = size - (prefix_len + suffix_len);
    char *app_name = malloc(app_name_size + 1);
    strncpy(app_name, config_name + prefix_len, app_name_size);
    app_name[app_name_size] = '\0';

    return app_name;
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

    struct json_object *root = json_tokener_parse(options_.app_list);
    struct json_object_iterator it = json_object_iter_begin(root);
    struct json_object_iterator it_end = json_object_iter_end(root);

    while (!json_object_iter_equal(&it, &it_end)) {
        const char *config_name = json_object_iter_peek_name(&it);
        const char *app_name = get_app_name(config_name);
        g_print("%s is enabled from cmdline\r\n", app_name);

        struct json_object *origins = json_object_iter_peek_value(&it);
        int arraylen = json_object_array_length(origins);

        GList *allowed_origins = NULL;
        for (int i = 0; i < arraylen; i++) {
          struct json_object *origin = json_object_array_get_idx(origins, i);
          char *origin_value = g_strdup(json_object_get_string(origin));
          g_print("\t origin %s\r\n", origin_value);

          allowed_origins = g_list_prepend(allowed_origins, origin_value);
       }

       gdial_rest_server_register_app(dial_rest_server, app_name, NULL, TRUE, TRUE, allowed_origins);
       g_list_free_full(allowed_origins, g_free);
       free(app_name);

       json_object_iter_next(&it);
    }

    json_object_put(root);
  }

  g_signal_connect(dial_rest_server, "invalid-uri", G_CALLBACK(signal_handler_rest_server_invalid_uri), NULL);
  g_signal_connect(dial_rest_server, "gmainloop-quit", G_CALLBACK(signal_handler_rest_server_gmainloop_quit), NULL);
  g_signal_connect(dial_rest_server, "rest-enable", G_CALLBACK(signal_handler_rest_server_rest_enable), NULL);

  gdial_ssdp_init(ssdp_http_server, &options_);
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
  g_main_loop_run (loop_);
  for (int i = 0; i < sizeof(servers)/sizeof(servers[0]); i++) {
    soup_server_disconnect(servers[i]);
    g_object_unref(servers[i]);
  }

  gdial_shield_term();
  gdial_ssdp_term();
  g_object_unref(dial_rest_server);
  gdial_plat_term();

  g_main_loop_unref(loop_);
  g_option_context_free(option_context);
  return 0;
}
