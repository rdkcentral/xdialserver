/*
 * If not stated otherwise in this file or this component's LICENSE file the
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
#include <pthread.h>
#include <libsoup/soup.h>
#include <libgssdp/gssdp.h>

#include "gdial-config.h"
#include "gdial-plat-util.h"
#include "gdial-plat-dev.h"
#include "gdial-ssdp.h"
#include "gdialservicelogging.h"

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
#define DIAL_SSDP_USN_FMT "uuid:%s::urn:dial-multiscreen-org:service:dial:1"
#define DIAL_SSDP_LOCATION_FMT "http://%s:%d/%s/dd.xml"
#define DIAL_SSDP_WAKEUP_FMT "MAC=%s;Timeout=%d"


/*
 * server cmdline options
 */
#define GDIAL_SSDP_DEVICE_UUID_DEFAULT "12345678-abcd-abcd-1234-123456789abc"
#define GDIAL_SSDP_FRIENDLY_DEFAULT  "DialClient"
#define GDIAL_SSDP_MANUFACTURER_DEFAULT "OEM"
#define GDIAL_SSDP_MODELNAME_DEFAULT "Device"

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
static gchar *app_random_uuid = NULL;
static pthread_mutex_t ssdpServerEventSync = PTHREAD_MUTEX_INITIALIZER;

static void ssdp_http_server_callback(SoupServer *server, SoupMessage *msg, const char *path, GHashTable *query, SoupClientContext  *client, gpointer user_data) {
  /*
   * /dd.xml only supports GET
   */
  if (!msg || !msg->method || msg->method != SOUP_METHOD_GET) {
    soup_message_set_status(msg, SOUP_STATUS_BAD_REQUEST);
    GDIAL_CHECK("GET_method_only");
    GDIAL_DEBUG("warning: SSDP HTTP Method is not GET");
    return;
  }

  pthread_mutex_lock(&ssdpServerEventSync);
  /*
   * there is no variant here, so we can cache the response.
   */
  static size_t dd_xml_response_str_len = 0;

  if (!dd_xml_response_str_) {
    const gchar *manufacturer= gdial_plat_dev_get_manufacturer();
    const gchar *model = gdial_plat_dev_get_model();

    if (manufacturer == NULL) {
        manufacturer = gdial_options_->manufacturer;
    }

    if (model == NULL) {
        model = gdial_options_->model_name;
    }

    if(gdial_options_->feature_friendlyname && app_friendly_name && strlen(app_friendly_name))
    {
        dd_xml_response_str_ = g_strdup_printf(ssdp_device_xml_template, app_friendly_name, manufacturer, model, gdial_options_->uuid);
    }
    else
    {
        dd_xml_response_str_ = g_strdup_printf(ssdp_device_xml_template, gdial_options_->friendly_name, manufacturer, model, gdial_options_->uuid);
    }

    if ( dd_xml_response_str_ ) {
        dd_xml_response_str_len = strlen(dd_xml_response_str_);
    }
    else {
        GDIAL_LOGERROR("Failed to allocate memory for dd.xml response");
        soup_message_set_status(msg, SOUP_STATUS_INTERNAL_SERVER_ERROR);
    }
  }

  if ( dd_xml_response_str_ ) {
    gchar *application_url_str = g_strdup_printf("http://%s:%d/%s/", iface_ipv4_address, GDIAL_REST_HTTP_PORT,app_random_uuid);

    if ( application_url_str )
    {
        soup_message_headers_replace (msg->response_headers, "Application-URL", application_url_str);
        g_free(application_url_str);
        application_url_str = NULL;

        soup_message_set_response(msg, "text/xml; charset=utf-8", SOUP_MEMORY_STATIC, dd_xml_response_str_, dd_xml_response_str_len);
        soup_message_set_status(msg, SOUP_STATUS_OK);

        GDIAL_CHECK("Content-Type:text/xml");
        GDIAL_CHECK("Application-URL: exist");
    }
    else
    {
        GDIAL_LOGERROR("Failed to allocate memory for response_headers");
        soup_message_set_status(msg, SOUP_STATUS_INTERNAL_SERVER_ERROR);
    }
  }
  pthread_mutex_unlock(&ssdpServerEventSync);
}

void gdial_ssdp_networkstandbymode_handler(const bool nwstandby)
{
  if(ssdp_client_){
     if(nwstandby){
        GDIAL_LOGINFO("gdial_ssdp_networkstandbymode_handler add WAKEUP header ");
        gchar *dial_ssdp_WAKEUP = g_strdup_printf(DIAL_SSDP_WAKEUP_FMT,gdial_plat_util_get_iface_mac_addr(gdial_options_->iface_name),MAX_POWERON_TIME);
        gssdp_client_append_header(ssdp_client_, "WAKEUP", dial_ssdp_WAKEUP);
        g_free(dial_ssdp_WAKEUP);
        dial_ssdp_WAKEUP = NULL;
     }
     else{
        GDIAL_LOGINFO("gdial_ssdp_networkstandbymode_handler remove WAKEUP header  ");
        gssdp_client_remove_header(ssdp_client_, "WAKEUP");
     }
  }
  return;
}

int gdial_ssdp_new(SoupServer *ssdp_http_server, GDialOptions *options, const gchar *random_uuid) {

  g_return_val_if_fail(ssdp_http_server != NULL, -1);
  g_return_val_if_fail(options != NULL, -1);
  g_return_val_if_fail(options->iface_name != NULL, -1);

  if (0 != pthread_mutex_init(&ssdpServerEventSync, NULL))
  {
    GDIAL_LOGERROR("Failed to initializing mutex");
    return EXIT_FAILURE;
  }

  gdial_options_ = options;
  GDIAL_LOGINFO("gdial_options_->friendly_name[%p]",gdial_options_->friendly_name);

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

  GSSDPClient *ssdp_client = gssdp_client_new(
#ifndef HAVE_GSSDP_VERSION_1_2_OR_NEWER
    NULL,
#endif
    gdial_options_->iface_name, &error);

  if (!ssdp_client || error) {
      GDIAL_LOGERROR("%s", error->message);
      g_error_free(error);
      return EXIT_FAILURE;
  }

  gdail_plat_dev_register_nwstandbymode_cb(gdial_ssdp_networkstandbymode_handler);
  /*
   * setup configurable headers.
   * header "SERVER" is populated by gssdp.
   * header "EXT" is mandatory, set by gssdp
   * header "CACHE-CONTROL" is mandatory, set by gssdp, default 1800
   */
  gssdp_client_append_header(ssdp_client, "BOOTID.UPNP.ORG", "1");

  /* feature_wolwake should be handled gdialservice users*/
  if(gdial_options_->feature_wolwake) {
    GDIAL_LOGINFO("WOL Wake feature is enabled");
    gchar *dial_ssdp_WAKEUP = g_strdup_printf(DIAL_SSDP_WAKEUP_FMT,gdial_plat_util_get_iface_mac_addr(gdial_options_->iface_name),MAX_POWERON_TIME);
    gssdp_client_append_header(ssdp_client, "WAKEUP", dial_ssdp_WAKEUP);
    g_free(dial_ssdp_WAKEUP);
    dial_ssdp_WAKEUP = NULL;
  }
  else {
    GDIAL_LOGINFO("WOL Wake feature is disabled");
  }
  GDIAL_CHECK("EXT");
  GDIAL_CHECK("CACHE-CONTROL");
  GDIAL_CHECK("BOOTID.UPNP.ORG");

  GSSDPResourceGroup *ssdp_resource_group = gssdp_resource_group_new(ssdp_client);
  gchar *dial_ssdp_USN = g_strdup_printf(DIAL_SSDP_USN_FMT, gdial_options_->uuid);
  gchar *dial_ssdp_LOCATION = g_strdup_printf(DIAL_SSDP_LOCATION_FMT, iface_ipv4_address, GDIAL_SSDP_HTTP_PORT,random_uuid);
  ssdp_resource_id_ =
    gssdp_resource_group_add_resource_simple (ssdp_resource_group, dial_ssdp_ST_target, dial_ssdp_USN, dial_ssdp_LOCATION);
  gssdp_resource_group_set_available (ssdp_resource_group, FALSE);
  g_free(dial_ssdp_USN);
  dial_ssdp_USN = NULL;
  g_free(dial_ssdp_LOCATION);
  dial_ssdp_LOCATION = NULL;

  ssdp_resource_group_ = ssdp_resource_group;

  g_object_ref(ssdp_http_server);
  ssdp_http_server_ = ssdp_http_server;
  app_random_uuid = g_strdup(random_uuid);
  gchar *dail_ssdp_handler = g_strdup_printf("/%s/%s", random_uuid,"dd.xml");
  soup_server_add_handler(ssdp_http_server_, dail_ssdp_handler, ssdp_http_server_callback, NULL, NULL);
  ssdp_client_ = ssdp_client;

  return 0;
}

int gdial_ssdp_destroy() {
  GDIAL_LOGTRACE("Entering ...");
  if (ssdp_http_server_)
  {
    soup_server_remove_handler(ssdp_http_server_, "/dd.xml");
  }

  if (ssdp_resource_group_)
  {
    gssdp_resource_group_remove_resource(ssdp_resource_group_, ssdp_resource_id_);
    ssdp_resource_id_ = 0;
  }

  if (dd_xml_response_str_)
  {
    g_free(dd_xml_response_str_);
    dd_xml_response_str_ = NULL;
  }
  if (gdial_options_)
  {
    if (gdial_options_->friendly_name != NULL)
    {
        g_free(gdial_options_->friendly_name);
        gdial_options_->friendly_name = NULL;
    }
    if (gdial_options_->uuid != NULL)
    {
        g_free(gdial_options_->uuid);
        gdial_options_->uuid = NULL;
    }
    if (gdial_options_->iface_name != NULL)
    {
        g_free(gdial_options_->iface_name);
        gdial_options_->iface_name = NULL;
    }
    gdial_options_ = NULL;
  }
  if (app_random_uuid != NULL) {
    g_free(app_random_uuid);
    app_random_uuid = NULL;
  }
  if (ssdp_client_)
  {
    gssdp_client_clear_headers(ssdp_client_);
  }
  if (ssdp_http_server_)
  {
    g_object_unref(ssdp_http_server_);
    ssdp_http_server_ = NULL;
  }
  if (ssdp_resource_group_)
  {
    g_object_unref(ssdp_resource_group_);
    ssdp_resource_group_ = NULL;
  }
  if (ssdp_client_)
  {
    g_object_unref(ssdp_client_);
    ssdp_client_ = NULL;
  }
  pthread_mutex_destroy(&ssdpServerEventSync);
  GDIAL_LOGTRACE("Exiting ...");
  return 0;
}

int gdial_ssdp_set_available(bool activation_status, const gchar *friendlyname)
{
  GDIAL_LOGINFO("gdial_ssdp_set_available activation_status :%d  ",activation_status);
  gdial_ssdp_set_friendlyname(friendlyname);
  if(ssdp_resource_group_) gssdp_resource_group_set_available (ssdp_resource_group_, activation_status);
  return 0;
}

int gdial_ssdp_set_friendlyname(const gchar *friendlyname)
{
    pthread_mutex_lock(&ssdpServerEventSync);
    if(gdial_options_ && gdial_options_->feature_friendlyname && friendlyname)
    {
        if (app_friendly_name != NULL) {
            g_free(app_friendly_name);
        }
        app_friendly_name = g_strdup(friendlyname);

        GDIAL_LOGINFO("gdial_ssdp_set_friendlyname app_friendly_name :%s  ",app_friendly_name);
        if (dd_xml_response_str_!= NULL){
            g_free(dd_xml_response_str_);
            dd_xml_response_str_ = NULL;
        }
    }
    pthread_mutex_unlock(&ssdpServerEventSync);
    return 0;
}