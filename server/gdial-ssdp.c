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
#include <libgssdp/gssdp.h>

#include "gdial-config.h"
#include "gdial-plat-util.h"
#include "gdial-plat-dev.h"
#include "gdial-ssdp.h"

#define MAX_POWERON_TIME 10
static SoupServer *ssdp_http_server_ = NULL;
static GDialOptions *gdial_options_ = NULL;
static GSSDPClient *ssdp_client_ = NULL;
static GSSDPResourceGroup *ssdp_resource_group_ = NULL;
static int ssdp_resource_id_ = 0;
/*
 * ssdp settings
 */
static const char *dial_ssdp_ST_target = "urn:dial-multiscreen-org:service:dial:1";
static const char *dial_ssdp_USN_fmt = "uuid:%s::urn:dial-multiscreen-org:service:dial:1";
static const char *dial_ssdp_LOCATION_fmt = "http://%s:%d/dd.xml";
static const char *dial_ssdp_WAKEUP_fmt = "MAC=%s;Timeout=%d";


/*
 * server cmdline options
 */
#define GDIAL_SSDP_DEVICE_UUID_DEFAULT "12345678-abcd-abcd-1234-123456789abc"
#define GDIAL_SSDP_FRIENDLY_DEFAULT  "FriendXi6"
#define GDIAL_SSDP_MANUFACTURER_DEFAULT "OEM"
#define GDIAL_SSDP_MODELNAME_DEFAULT "Xi6"

static const char *iface_ipv4_address = NULL;
/*
 * Copyright (c) 2014 Netflix, Inc.
 * Licensed under the BSD-2 license
 */
static const char ssdp_device_xml_template[] = ""
  "<?xml version=\"1.0\"?>"
  "<root xmlns=\"urn:schemas-upnp-org:device-1-0\" xmlns:r=\"urn:restful-tv-org:schemas:upnp-dd\">"
  "  <specVersion> <major>1</major> <minor>0</minor> </specVersion>"
  "    <device>"
  "      <deviceType>urn:schemas-upnp-org:device:tvdevice:1</deviceType>"
  "      <friendlyName>%s</friendlyName>"
  "      <manufacturer>%s</manufacturer>"
  "      <modelName>%s</modelName>"
  "      <UDN>uuid:%s</UDN>"
  "     </device>"
  "</root>";

static gchar *dd_xml_response_str_ = NULL;
static gchar *app_friendly_name = NULL;

static void ssdp_http_server_callback(SoupServer *server, SoupMessage *msg, const char *path, GHashTable *query, SoupClientContext  *client, gpointer user_data) {
  /*
   * /dd.xml only supports GET
   */
  if (!msg || !msg->method || msg->method != SOUP_METHOD_GET) {
    soup_message_set_status(msg, SOUP_STATUS_BAD_REQUEST);
    GDIAL_CHECK("GET_method_only");
    GDIAL_DEBUG("warning: SSDP HTTP Method is not GET\r\n");
    return;
  }

  /*
   * there is no variant here, so we can cache the response.
   */
  static size_t dd_xml_response_str_len = 0;

  if (!dd_xml_response_str_) {
    const gchar *manufacturer= gdial_plat_dev_get_manufacturer();
    const gchar *model = gdial_plat_dev_get_model();

    if (manufacturer == NULL) {manufacturer = gdial_options_->manufacturer;}
    if (model == NULL) {model = gdial_options_->model_name;}
    if(gdial_options_->feature_friendlyname && strlen(app_friendly_name))
        dd_xml_response_str_ = g_strdup_printf(ssdp_device_xml_template, app_friendly_name, manufacturer, model, gdial_options_->uuid);
    else
        dd_xml_response_str_ = g_strdup_printf(ssdp_device_xml_template, gdial_options_->friendly_name, manufacturer, model, gdial_options_->uuid);

    dd_xml_response_str_len = strlen(dd_xml_response_str_);
  }

  gchar *application_url_str = g_strdup_printf("http://%s:%d%s/", iface_ipv4_address, GDIAL_REST_HTTP_PORT, GDIAL_REST_HTTP_APPS_URI);
  soup_message_headers_replace (msg->response_headers, "Application-URL", application_url_str);
  g_free(application_url_str);
  soup_message_set_response(msg, "text/xml; charset=utf-8", SOUP_MEMORY_STATIC, dd_xml_response_str_, dd_xml_response_str_len);
  soup_message_set_status(msg, SOUP_STATUS_OK);
  GDIAL_CHECK("Content-Type:text/xml");
  GDIAL_CHECK("Application-URL: exist");
}

int gdial_ssdp_new(SoupServer *ssdp_http_server, GDialOptions *options) {

  g_return_val_if_fail(ssdp_http_server != NULL, -1);
  g_return_val_if_fail(options != NULL, -1);
  g_return_val_if_fail(options->iface_name != NULL, -1);

  gdial_options_ = options;
  if (gdial_options_->friendly_name == NULL) gdial_options_->friendly_name = g_strdup(GDIAL_SSDP_FRIENDLY_DEFAULT);
  if (gdial_options_->manufacturer== NULL) gdial_options_->manufacturer = g_strdup(GDIAL_SSDP_MANUFACTURER_DEFAULT);
  if (gdial_options_->model_name== NULL) gdial_options_->model_name = g_strdup(GDIAL_SSDP_MODELNAME_DEFAULT);
  if (gdial_options_->uuid == NULL) gdial_options_->uuid = g_strdup(GDIAL_SSDP_DEVICE_UUID_DEFAULT);
  if (gdial_options_->iface_name == NULL) gdial_options_->iface_name = g_strdup(GDIAL_IFACE_NAME_DEFAULT);

  GError *error = NULL;

  iface_ipv4_address = gdial_plat_util_get_iface_ipv4_addr(gdial_options_->iface_name);
  if (!iface_ipv4_address) {
    return EXIT_FAILURE;
  }

  GSSDPClient *ssdp_client = gssdp_client_new(NULL, gdial_options_->iface_name, &error);
  if (!ssdp_client || error) {
      g_printerr("%s\r\n", error->message);
      g_error_free(error);
      return EXIT_FAILURE;
  }

  /*
   * setup configurable headers.
   * header "SERVER" is populated by gssdp.
   * header "EXT" is mandatory, set by gssdp
   * header "CACHE-CONTROL" is mandatory, set by gssdp, default 1800
   */
  gssdp_client_append_header(ssdp_client, "BOOTID.UPNP.ORG", "1");
  if(gdial_options_->feature_wolwake) {
    g_print("WOL Wake feature is enabled");
    gchar *dial_ssdp_WAKEUP = g_strdup_printf(dial_ssdp_WAKEUP_fmt,gdial_plat_util_get_iface_mac_addr(gdial_options_->iface_name),MAX_POWERON_TIME);
    gssdp_client_append_header(ssdp_client, "WAKEUP", dial_ssdp_WAKEUP);
    g_free(dial_ssdp_WAKEUP);
  }
  else {
    g_print("WOL Wake feature is disabled");
  }
  GDIAL_CHECK("EXT");
  GDIAL_CHECK("CACHE-CONTROL");
  GDIAL_CHECK("BOOTID.UPNP.ORG");

  GSSDPResourceGroup *ssdp_resource_group = gssdp_resource_group_new(ssdp_client);
  gchar *dial_ssdp_USN = g_strdup_printf(dial_ssdp_USN_fmt, gdial_options_->uuid);
  gchar *dial_ssdp_LOCATION = g_strdup_printf(dial_ssdp_LOCATION_fmt, iface_ipv4_address, GDIAL_SSDP_HTTP_PORT);
  ssdp_resource_id_ =
    gssdp_resource_group_add_resource_simple (ssdp_resource_group, dial_ssdp_ST_target, dial_ssdp_USN, dial_ssdp_LOCATION);
  gssdp_resource_group_set_available (ssdp_resource_group, FALSE);
  g_free(dial_ssdp_USN);
  g_free(dial_ssdp_LOCATION);

  ssdp_resource_group_ = ssdp_resource_group;

  g_object_ref(ssdp_http_server);
  ssdp_http_server_ = ssdp_http_server;

  soup_server_add_handler(ssdp_http_server_, "/dd.xml", ssdp_http_server_callback, NULL, NULL);
  ssdp_client_ = ssdp_client;

  return 0;
}

int gdial_ssdp_destroy() {
  soup_server_remove_handler(ssdp_http_server_, "/dd.xml");
  gssdp_resource_group_remove_resource(ssdp_resource_group_, ssdp_resource_id_);

  if (dd_xml_response_str_) {
    g_free(dd_xml_response_str_);
  }
  if (gdial_options_->friendly_name != NULL) g_free(gdial_options_->friendly_name);
  if (gdial_options_->uuid != NULL) g_free(gdial_options_->uuid);
  if (gdial_options_->iface_name != NULL) g_free(gdial_options_->iface_name);

  gssdp_client_clear_headers(ssdp_client_);

  g_object_unref(ssdp_http_server_);
  g_object_unref(ssdp_resource_group_);
  g_object_unref(ssdp_client_);

  return 0;
}

int gdial_ssdp_set_available(bool activation_status, const gchar *friendlyname)
{
  g_print("gdial_ssdp_set_available activation_status :%d \n ",activation_status);
  gdial_ssdp_set_friendlyname(friendlyname);
  if(ssdp_resource_group_) gssdp_resource_group_set_available (ssdp_resource_group_, activation_status);
  return 0;
}

int gdial_ssdp_set_friendlyname(const gchar *friendlyname)
{
  if(gdial_options_ && gdial_options_->feature_friendlyname && friendlyname)
  {
     if (app_friendly_name != NULL) g_free(app_friendly_name);
     app_friendly_name = g_strdup(friendlyname);
     g_print("gdial_ssdp_set_friendlyname app_friendly_name :%s \n ",app_friendly_name);
     if (dd_xml_response_str_!= NULL){
      g_free(dd_xml_response_str_);
      dd_xml_response_str_ = NULL;
     }
  }
  return 0;
}
