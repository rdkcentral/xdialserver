/*
 * If not stated otherwise in this file or this component's LICENSE file the
 * following copyright and licenses apply:
 *
 * Copyright 2023 RDK Management
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
#include <uuid/uuid.h>
#include <sys/prctl.h>
#include "gdialservice.h"
#include "gdialserviceimpl.h"
#include "gdialservicelogging.h"

gdialService *m_gdialService{nullptr};
gdialServiceImpl *m_gdialServiceImpl{nullptr};

static const char *dial_specification_copyright = "Copyright (c) 2017 Netflix, Inc. All rights reserved.";

#define MAX_UUID_SIZE 64
#define UUID_FILE_PATH "/opt/.dial_uuid.txt"

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

static const gchar *iface_ipv4_address_ = NULL;

static void signal_handler_rest_server_invalid_uri(GDialRestServer *dial_rest_server, const gchar *signal_message, gpointer user_data) {
  g_return_if_fail(dial_rest_server && signal_message);
  GDIAL_LOGERROR("signal invalid-uri: [%s]", signal_message);
}

static void signal_handler_rest_server_gmainloop_quit(GDialRestServer *dial_rest_server, const gchar *signal_message, gpointer user_data) {
  g_return_if_fail(dial_rest_server && signal_message);
  GDIAL_LOGERROR("signal gmainloop-quit: [%s]", signal_message);
  GDIAL_LOGINFO("Exiting DIAL Protocol | %s ", dial_specification_copyright);
  //g_main_loop_quit(m_gdialServiceImpl->m_main_loop);
}
static GDialRestServer *dial_rest_server = NULL;

static void server_activation_handler(gboolean status, const gchar *friendlyname)
{
    GDIAL_LOGINFO("server_activation_handler status :%d ",status);
    gdial_ssdp_set_available(status,friendlyname);
    if(dial_rest_server)
    {
        g_object_set(dial_rest_server,"enable" ,status, NULL);
    }
}

static void server_register_application(gpointer data)
{
    GList* g_app_list = (GList*) data;
    GDIAL_LOGTRACE("Entering ...");
    GDIAL_LOGINFO("server_register_application callback ");
    if(g_app_list) {
        GDIAL_LOGINFO("server_register_application appList :%d", g_list_length (g_app_list));
    }

    /*Remove all existing registered Apps*/
    gdial_rest_server_unregister_all_apps(dial_rest_server);
    while(g_app_list) {
        gdial_rest_server_register_app_registry (dial_rest_server, (GDialAppRegistry *)g_app_list->data);
        g_app_list = g_app_list->next;
    }

    if (!options_.app_list) {
        GDIAL_LOGINFO(" No application is enabled from cmdline so ignore system app ");
    }
    else{
       size_t app_list_len = strlen(options_.app_list);
       gchar *app_list_low = g_ascii_strdown(options_.app_list, app_list_len);
       if (g_strstr_len(app_list_low, app_list_len , "system")) {
         GDIAL_LOGINFO("Register system app -  enabled from cmdline");
         gdial_rest_server_register_app(dial_rest_server, "system", NULL, NULL, TRUE, TRUE, NULL);
       }
       else {
         GDIAL_LOGINFO("Dont register system app - not enabled from cmdline");
       }
    }
    GDIAL_LOGTRACE("Exiting ...");
}

static void server_friendlyname_handler(const gchar * friendlyname)
{
    gdial_ssdp_set_friendlyname(friendlyname);
}

static void server_powerstate_handler(const gchar * powerState)
{
    gdialServiceImpl::getInstance()->updatePowerState(std::string(powerState));
}

static void signal_handler_rest_server_rest_enable(GDialRestServer *dial_rest_server, const gchar *signal_message, gpointer user_data) {
  GDIAL_LOGINFO(" signal_handler_rest_server_rest_enable received signal :%s  ",signal_message );
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
  GDIAL_LOGINFO("gdial_http_server_throttle_callback ");
  soup_message_headers_replace(msg->response_headers, "Connection", "close");
  soup_message_set_status(msg, SOUP_STATUS_NOT_FOUND);
}

static void gdial_quit_thread(int signum)
{
  GDIAL_LOGINFO("Exiting DIAL Server thread %d ",signum);
  server_activation_handler(0, "");
  usleep(50000);               //Sleeping 50 ms to allow existing request to finish processing.
  //GDIAL_LOGINFO(" calling g_main_loop_quit loop_: %p ",m_gdialServiceImpl->m_main_loop);
  //if(m_gdialServiceImpl->m_main_loop)g_main_loop_quit(m_gdialServiceImpl->m_main_loop);
}

static SoupServer * m_servers[3] = {NULL,NULL,NULL};
static GOptionContext *m_option_context = NULL;

int gdialServiceImpl::start_GDialServer(int argc, char *argv[])
{
    GError *error = NULL;
    int returnValue = EXIT_FAILURE;
    GDIAL_LOGTRACE("Entering ...");
    GOptionContext *m_option_context = g_option_context_new(NULL);
    if (!m_option_context)
    {
        GDIAL_LOGERROR("Failed to create option context");
        GDIAL_LOGTRACE("Exiting ...");
        return returnValue;
    }

    g_option_context_add_main_entries(m_option_context, option_entries_, NULL);

    if (!g_option_context_parse (m_option_context, &argc, &argv, &error))
    {
        GDIAL_LOGERROR ("%s", error->message);
        g_error_free(error);
        GDIAL_LOGTRACE("Exiting ...");
        return returnValue;
    }

    if (!options_.iface_name)
    {
        options_.iface_name =  g_strdup(GDIAL_IFACE_NAME_DEFAULT);
        GDIAL_LOGWARNING("Interface Name not given, so [%s] used",GDIAL_IFACE_NAME_DEFAULT);
    }

    #define MAX_RETRY 3
    for(int i=1;i<=MAX_RETRY;i++)
    {
        iface_ipv4_address_ = gdial_plat_util_get_iface_ipv4_addr(options_.iface_name);
        if (!iface_ipv4_address_)
        {
            GDIAL_LOGWARNING("Warning: interface %s does not have IP", options_.iface_name);
            if(i >= MAX_RETRY )
            {
                GDIAL_LOGTRACE("Exiting ...");
                g_option_context_free(m_option_context);
                m_option_context = nullptr;
                return returnValue;
            }
            sleep(2);
        }
        else
        {
            break;
        }
    }

    //m_main_loop_context = g_main_context_default();
    m_main_loop_context = g_main_context_new();
    g_main_context_push_thread_default(m_main_loop_context);
    m_main_loop = g_main_loop_new(m_main_loop_context, FALSE);
    gdial_plat_init(m_main_loop_context);

    gdail_plat_register_activation_cb(server_activation_handler);
    gdail_plat_register_friendlyname_cb(server_friendlyname_handler);
    gdail_plat_register_registerapps_cb (server_register_application);
    gdail_plat_dev_register_powerstate_cb(server_powerstate_handler);

    SoupServer * rest_http_server = soup_server_new(NULL);
    SoupServer * ssdp_http_server = soup_server_new(NULL);
    SoupServer * local_rest_http_server = soup_server_new(NULL);
    soup_server_add_handler(rest_http_server, "/", gdial_http_server_throttle_callback, NULL, NULL);
    soup_server_add_handler(ssdp_http_server, "/", gdial_http_server_throttle_callback, NULL, NULL);

    GSocketAddress *listen_address = g_inet_socket_address_new_from_string(iface_ipv4_address_, GDIAL_REST_HTTP_PORT);
    SoupServerListenOptions option = 0;
    gboolean success = soup_server_listen(rest_http_server, listen_address, option, &error);
    g_object_unref (listen_address);
    if (!success)
    {
        GDIAL_LOGERROR("%s", error->message);
        g_error_free(error);
        GDIAL_LOGTRACE("Exiting ...");
        g_option_context_free(m_option_context);
        m_option_context = nullptr;
        return returnValue;
    }
    else
    {
        listen_address = g_inet_socket_address_new_from_string(iface_ipv4_address_, GDIAL_SSDP_HTTP_PORT);
        success = soup_server_listen(ssdp_http_server, listen_address, option, &error);
        g_object_unref (listen_address);
        if (!success)
        {
            GDIAL_LOGERROR("%s", error->message);
            g_error_free(error);
            GDIAL_LOGTRACE("Exiting ...");
            g_option_context_free(m_option_context);
            m_option_context = nullptr;
            return returnValue;
        }
        else
        {
            success = soup_server_listen_local(local_rest_http_server, GDIAL_REST_HTTP_PORT, SOUP_SERVER_LISTEN_IPV4_ONLY, &error);
            if (!success)
            {
                GDIAL_LOGERROR("%s", error->message);
                g_error_free(error);
                GDIAL_LOGTRACE("Exiting ...");
                g_option_context_free(m_option_context);
                m_option_context = nullptr;
                return returnValue;
            }
        }
    }
    gchar uuid_str[MAX_UUID_SIZE] = {0};
    char * static_apps_location = getenv("XDIAL_STATIC_APPS_LOCATION");
    if (static_apps_location != NULL && strlen(static_apps_location))
    {
        g_snprintf(uuid_str, MAX_UUID_SIZE, "%s", static_apps_location);
        GDIAL_LOGINFO("static uuid_str  :%s", uuid_str);
    }
    else
    {
        FILE *fuuid = fopen(UUID_FILE_PATH, "r");
        if (fuuid == NULL)
        {
            uuid_t random_uuid;
            uuid_generate_random(random_uuid);
            uuid_unparse(random_uuid, uuid_str);
            GDIAL_LOGINFO("generated uuid_str  :%s", uuid_str);
            fuuid = fopen(UUID_FILE_PATH, "w");
            if (fuuid != NULL)
            {
                fprintf(fuuid, "%s", uuid_str);
                fclose(fuuid);
            }
        }
        else
        {
            fgets(uuid_str, sizeof(uuid_str), fuuid);
            printf("Persistent uuid_str: %s", uuid_str);
            fclose(fuuid);
        }
    }

    dial_rest_server = gdial_rest_server_new(rest_http_server,local_rest_http_server,uuid_str);
    if (!options_.app_list)
    {
        GDIAL_LOGINFO("no application is enabled from cmdline ");
    }
    else
    {
        GDIAL_LOGINFO("app_list to be enabled from command line %s", options_.app_list);
        size_t app_list_len = strlen(options_.app_list);
        gchar *app_list_low = g_ascii_strdown(options_.app_list, app_list_len);
        if (g_strstr_len(app_list_low, app_list_len, "netflix"))
        {
            GDIAL_LOGINFO("netflix is enabled from cmdline");
            GList *allowed_origins = g_list_prepend(NULL, ".netflix.com");
            gdial_rest_server_register_app(dial_rest_server, "Netflix", NULL, NULL, TRUE, TRUE, allowed_origins);
            g_list_free(allowed_origins);
        }
        else
        {
            GDIAL_LOGINFO("netflix is not enabled from cmdline");
        }

        if (g_strstr_len(app_list_low, app_list_len, "youtube"))
        {
            GDIAL_LOGINFO("youtube is enabled from cmdline");
            GList *allowed_origins = g_list_prepend(NULL, ".youtube.com");
            gdial_rest_server_register_app(dial_rest_server, "YouTube", NULL, NULL, TRUE, TRUE, allowed_origins);
            g_list_free(allowed_origins);
        }
        else
        {
            GDIAL_LOGINFO("youtube is not enabled from cmdline");
        }

        if (g_strstr_len(app_list_low, app_list_len, "youtubetv"))
        {
            GDIAL_LOGINFO("youtubetv is enabled from cmdline");
            GList *allowed_origins = g_list_prepend(NULL, ".youtube.com");
            gdial_rest_server_register_app(dial_rest_server, "YouTubeTV", NULL, NULL, TRUE, TRUE, allowed_origins);
            g_list_free(allowed_origins);
        }
        else
        {
            GDIAL_LOGINFO("youtubetv is not enabled from cmdline");
        }

        if (g_strstr_len(app_list_low, app_list_len, "youtubekids"))
        {
            GDIAL_LOGINFO("youtubekids is enabled from cmdline");
            GList *allowed_origins = g_list_prepend(NULL, ".youtube.com");
            gdial_rest_server_register_app(dial_rest_server, "YouTubeKids", NULL, NULL, TRUE, TRUE, allowed_origins);
            g_list_free(allowed_origins);
        }
        else
        {
            GDIAL_LOGINFO("youtubekids is not enabled from cmdline");
        }

        if (g_strstr_len(app_list_low, app_list_len, "amazoninstantvideo"))
        {
            GDIAL_LOGINFO("AmazonInstantVideo is enabled from cmdline");
            GList *allowed_origins = g_list_prepend(NULL, ".amazonprime.com");
            gdial_rest_server_register_app(dial_rest_server, "AmazonInstantVideo", NULL, NULL, TRUE, TRUE, allowed_origins);
            g_list_free(allowed_origins);
        }
        else
        {
            GDIAL_LOGINFO("AmazonInstantVideo is not enabled from cmdline");
        }

        if (g_strstr_len(app_list_low, app_list_len, "spotify"))
        {
            GDIAL_LOGINFO("spotify is enabled from cmdline");
            GList *app_prefixes= g_list_prepend(NULL, "com.spotify");
            GList *allowed_origins = g_list_prepend(NULL, ".spotify.com");
            gdial_rest_server_register_app(dial_rest_server, "com.spotify.Spotify.TV", app_prefixes, NULL, TRUE, TRUE, allowed_origins);
            g_list_free(allowed_origins);
            g_list_free(app_prefixes);
        }
        else
        {
            GDIAL_LOGINFO("spotify is not enabled from cmdline");
        }

        if (g_strstr_len(app_list_low, app_list_len, "pairing"))
        {
            GDIAL_LOGINFO("pairing is enabled from cmdline");
            GList *allowed_origins = g_list_prepend(NULL, ".comcast.com");
            gdial_rest_server_register_app(dial_rest_server, "Pairing", NULL, NULL, TRUE, TRUE, allowed_origins);
            g_list_free(allowed_origins);
        }
        else
        {
            GDIAL_LOGINFO("pairing is not enabled from cmdline");
        }

        if (g_strstr_len(app_list_low, app_list_len, "system"))
        {
            GDIAL_LOGINFO("system is enabled from cmdline");
            gdial_rest_server_register_app(dial_rest_server, "system", NULL, NULL, TRUE, TRUE, NULL);
        }
        else
        {
            GDIAL_LOGINFO("system is not enabled from cmdline");
        }
        g_free(app_list_low);
        app_list_low = NULL;
    }

    g_signal_connect(dial_rest_server, "invalid-uri", G_CALLBACK(signal_handler_rest_server_invalid_uri), NULL);
    g_signal_connect(dial_rest_server, "gmainloop-quit", G_CALLBACK(signal_handler_rest_server_gmainloop_quit), NULL);
    g_signal_connect(dial_rest_server, "rest-enable", G_CALLBACK(signal_handler_rest_server_rest_enable), NULL);

    gdial_ssdp_new(ssdp_http_server, &options_,uuid_str);
    gdial_shield_init();
    gdial_shield_server(rest_http_server);
    gdial_shield_server(ssdp_http_server);

    m_servers[0] = local_rest_http_server;
    m_servers[1] = rest_http_server;
    m_servers[2] = ssdp_http_server;

    for (int i = 0; i < sizeof(m_servers)/sizeof(m_servers[0]); i++)
    {
        GSList *uris = soup_server_get_uris(m_servers[i]);
        for (GSList *uri =  uris; uri != NULL; uri = uri->next)
        {
            char *uri_string = soup_uri_to_string(uri->data, FALSE);
            GDIAL_LOGINFO("Listening on %s", uri_string);
            g_free(uri_string);
            soup_uri_free(uri->data);
        }
        g_slist_free(uris);
    }

    g_main_context_pop_thread_default(m_main_loop_context);
    pthread_create(&m_gdialserver_main_thread, nullptr, gdialServiceImpl::mainThread, this);
    pthread_create(&m_gdialserver_request_handler_thread, nullptr, gdialServiceImpl::requestHandlerThread, this);
    pthread_create(&m_gdialserver_response_handler_thread, nullptr, gdialServiceImpl::responseHandlerThread, this);

    if (( 0 == m_gdialserver_main_thread ) || ( 0 == m_gdialserver_request_handler_thread ) || ( 0 == m_gdialserver_response_handler_thread ))
    {
        GDIAL_LOGERROR("Failed to create gdialserver thread");
        stop_GDialServer();
    }
    else
    {
        returnValue = 0;
        GDIAL_LOGINFO("Success ...");
        gdial_plat_application_service_notification(true,this);
        GDIAL_LOGINFO("gdial_plat_application_service_notification done ...");
    }
    GDIAL_LOGTRACE("Exiting ...");
    return returnValue;
}

bool gdialServiceImpl::stop_GDialServer()
{
    GDIAL_LOGTRACE("Entering ...");
    server_activation_handler(0, "");
    //Sleeping 50 ms to allow existing request to finish processing.
    usleep(50000);

    GDIAL_LOGINFO("calling RequestHandlerThread to exit");
    {
        m_RequestHandlerThreadExit = true;
        std::unique_lock<std::mutex> lk(m_RequestHandlerEventMutex);
        m_RequestHandlerThreadRun = true;
        m_RequestHandlerCV.notify_one();
    }

    GDIAL_LOGINFO("calling ResponseHandlerThread to exit");
    {
        m_ResponseHandlerThreadExit = true;
        std::unique_lock<std::mutex> lk(m_ResponseHandlerEventMutex);
        m_ResponseHandlerThreadRun = true;
        m_ResponseHandlerCV.notify_one();
    }

    GDIAL_LOGINFO(" calling g_main_loop_quit loop_: %p ",m_main_loop);
    if (m_main_loop)
    {
        g_main_loop_quit(m_main_loop);
    }
    if (m_gdialserver_main_thread)
    {
        pthread_join(m_gdialserver_main_thread,nullptr);
        m_gdialserver_main_thread = 0;
    }
    if (m_gdialserver_request_handler_thread)
    {
        pthread_join(m_gdialserver_request_handler_thread,nullptr);
        m_gdialserver_request_handler_thread = 0;
    }
    if (m_gdialserver_response_handler_thread)
    {
        pthread_join(m_gdialserver_response_handler_thread,nullptr);
        m_gdialserver_response_handler_thread = 0;
    }

    for (int i = 0; i < sizeof(m_servers)/sizeof(m_servers[0]); i++)
    {
        if (m_servers[i])
        {
            soup_server_disconnect(m_servers[i]);
            g_object_unref(m_servers[i]);
            m_servers[i] = NULL;
        }
    }

    gdial_shield_term();
    gdial_ssdp_destroy();
    if (dial_rest_server)
    {
        g_object_unref(dial_rest_server);
        dial_rest_server = NULL;
    }
    gdial_plat_application_service_notification(false,NULL);
    gdial_plat_term();

    if (m_main_loop)
    {
        g_main_loop_unref(m_main_loop);
        m_main_loop = NULL;
    }

    if (m_main_loop_context)
    {
        g_main_context_unref(m_main_loop_context);
        m_main_loop_context = NULL;
    }
    if (m_option_context)
    {
        g_option_context_free(m_option_context);
        m_option_context = nullptr;
    }
    GDIAL_LOGTRACE("Exiting ...");
    return true;
}

void *gdialServiceImpl::mainThread(void *ctx)
{
    GDIAL_LOGTRACE("Entering ...");
    gdialServiceImpl *_instance = (gdialServiceImpl *)ctx;
    g_main_context_push_thread_default(_instance->m_main_loop_context);
    g_main_loop_run(_instance->m_main_loop);
    _instance->m_gdialserver_main_thread = 0;
    g_main_context_pop_thread_default(_instance->m_main_loop_context);
    GDIAL_LOGTRACE("Exiting ...");
    pthread_exit(nullptr);
}

void *gdialServiceImpl::requestHandlerThread(void *ctx)
{
    GDIAL_LOGTRACE("Entering ...");
    gdialServiceImpl *_instance = (gdialServiceImpl *)ctx;
    RequestHandlerPayload reqHdlrPayload;
    while(!_instance->m_RequestHandlerThreadExit)
    {
        reqHdlrPayload.appNameOrfriendlyname = "";
        reqHdlrPayload.appIdOractivation = "";
        reqHdlrPayload.state = "";
        reqHdlrPayload.error = "";
        reqHdlrPayload.data_param = nullptr;
        reqHdlrPayload.event = INVALID_REQUEST;
        {
            // Wait for a message to be added to the queue
            std::unique_lock<std::mutex> lk(_instance->m_RequestHandlerEventMutex);
            _instance->m_RequestHandlerCV.wait(lk, [instance = _instance]{return (instance->m_RequestHandlerThreadRun == true);});
        }

        if (_instance->m_RequestHandlerThreadExit == true)
        {
            GDIAL_LOGINFO(" threadSendRequest Exiting");
            _instance->m_RequestHandlerThreadRun = false;
            break;
        }

        if (_instance->m_RequestHandlerQueue.empty())
        {
            _instance->m_RequestHandlerThreadRun = false;
            continue;
        }

        reqHdlrPayload = _instance->m_RequestHandlerQueue.front();
        _instance->m_RequestHandlerQueue.pop();

        if ( nullptr != _instance->m_observer )
        {
            GDIAL_LOGINFO("Request : Event:0x%x",reqHdlrPayload.event);
            switch(reqHdlrPayload.event)
            {
                case APP_STATE_CHANGED:
                {
                    std::string appName = reqHdlrPayload.appNameOrfriendlyname,
                                appId = reqHdlrPayload.appIdOractivation,
                                state = reqHdlrPayload.state,
                                error = reqHdlrPayload.error;
                    GDIAL_LOGINFO("APP_STATE_CHANGED : appName:%s appId:%s state:%s error:%s",appName.c_str(),appId.c_str(),state.c_str(),error.c_str());
                    gdial_plat_application_state_changed(appName.c_str(),appId.c_str(),state.c_str(),error.c_str());
                }
                break;
                case ACTIVATION_CHANGED:
                {
                    std::string activation = reqHdlrPayload.appIdOractivation,
                                friendlyName = reqHdlrPayload.appNameOrfriendlyname;
                    GDIAL_LOGINFO("ACTIVATION_CHANGED : actication:%s friendlyName:%s",activation.c_str(),friendlyName.c_str());
                    gdial_plat_application_activation_changed(activation.c_str(),friendlyName.c_str());
                }
                break;
                case FRIENDLYNAME_CHANGED:
                {
                    std::string friendlyName = reqHdlrPayload.appNameOrfriendlyname;
                    GDIAL_LOGINFO("FRIENDLYNAME_CHANGED : friendlyName:%s",friendlyName.c_str());
                    gdial_plat_application_friendlyname_changed(friendlyName.c_str());
                }
                break;
                case REGISTER_APPLICATIONS:
                {
                    GDIAL_LOGINFO("REGISTER_APPLICATIONS : data:%p",reqHdlrPayload.data_param);
                    gdial_plat_application_register_applications(reqHdlrPayload.data_param);
                }
                break;
                case UPDATE_NW_STANDBY:
                {
                    GDIAL_LOGINFO("UPDATE_NW_STANDBY : data:%u",reqHdlrPayload.user_param1);
                    gdial_plat_application_update_network_standby_mode((gboolean)reqHdlrPayload.user_param1);
                }
                break;
                default:
                {

                }
                break;
            }
        }
    }
    _instance->m_gdialserver_request_handler_thread = 0;
    GDIAL_LOGTRACE("Exiting ...");
    pthread_exit(nullptr);
}

void *gdialServiceImpl::responseHandlerThread(void *ctx)
{
    gdialServiceImpl *_instance = (gdialServiceImpl *)ctx;
    ResponseHandlerPayload response_data;
    while(!_instance->m_ResponseHandlerThreadExit)
    {
        response_data.appName = "";
        response_data.parameterOrPayload = "";
        response_data.appIdOrQuery = "";
        response_data.AddDataUrl = "";
        response_data.event = APP_INVALID_STATE;

        {
            // Wait for a message to be added to the queue
            std::unique_lock<std::mutex> lk(_instance->m_ResponseHandlerEventMutex);
            _instance->m_ResponseHandlerCV.wait(lk, [instance = _instance]{return (instance->m_ResponseHandlerThreadRun == true);});
        }

        if (_instance->m_ResponseHandlerThreadExit == true)
        {
            GDIAL_LOGINFO(" threadSendResponse Exiting");
            _instance->m_ResponseHandlerThreadRun = false;
            break;
        }

        if (_instance->m_ResponseHandlerQueue.empty())
        {
            _instance->m_ResponseHandlerThreadRun = false;
            continue;
        }

        response_data = _instance->m_ResponseHandlerQueue.front();
        _instance->m_ResponseHandlerQueue.pop();

        if ( nullptr != _instance->m_observer )
        {
            GDIAL_LOGINFO("Response : Event:0x%x",response_data.event);
            switch(response_data.event)
            {
                case APP_LAUNCH_REQUEST_WITH_PARAMS:
                {
                    std::string appName = response_data.appName,
                                payload = response_data.parameterOrPayload,
                                query = response_data.appIdOrQuery,
                                AddDataUrl = response_data.AddDataUrl;
                    GDIAL_LOGINFO("AppLaunchWithParam : appName:%s PayLoad:%s  Query:%s AddUrl:%s",
                            appName.c_str(),
                            payload.c_str(),
                            query.c_str(),
                            AddDataUrl.c_str());
                    _instance->m_observer->onApplicationLaunchRequestWithLaunchParam(appName,payload,query,AddDataUrl);
                }
                break;
                case APP_LAUNCH_REQUEST:
                {
                    std::string appName = response_data.appName,
                                parameter = response_data.parameterOrPayload;
                    GDIAL_LOGINFO("AppLaunch : appName:%s parameter:%s",appName.c_str(),parameter.c_str());
                    _instance->m_observer->onApplicationLaunchRequest(appName,parameter);
                }
                break;
                case APP_STOP_REQUEST:
                {
                    std::string appName = response_data.appName,
                                appId = response_data.appIdOrQuery;
                    GDIAL_LOGINFO("AppStop : appName:%s appId:%s",appName.c_str(),appId.c_str());
                    _instance->m_observer->onApplicationStopRequest(appName,appId);
                }
                break;
                case APP_HIDE_REQUEST:
                {
                    std::string appName = response_data.appName,
                                appId = response_data.appIdOrQuery;
                    GDIAL_LOGINFO("AppHide : appName:%s appId:%s",appName.c_str(),appId.c_str());
                    _instance->m_observer->onApplicationHideRequest(appName,appId);
                }
                break;
                case APP_STATE_REQUEST:
                {
                    std::string appName = response_data.appName,
                                appId = response_data.appIdOrQuery;
                    GDIAL_LOGINFO("AppState : appName:%s appId:%s",appName.c_str(),appId.c_str());
                    _instance->m_observer->onApplicationStateRequest(appName,appId);
                }
                break;
                case APP_RESUME_REQUEST:
                {
                    std::string appName = response_data.appName,
                                appId = response_data.appIdOrQuery;
                    GDIAL_LOGINFO("AppResume : appName:%s appId:%s",appName.c_str(),appId.c_str());
                    _instance->m_observer->onApplicationResumeRequest(appName,appId);
                }
                break;
                default:
                {

                }
                break;
            }
        }
    }
    _instance->m_gdialserver_response_handler_thread = 0;
    _instance->m_observer->onStopped();
    pthread_exit(nullptr);
}

gdialServiceImpl* gdialServiceImpl::getInstance(void)
{
    if (nullptr == m_gdialServiceImpl)
    {
        m_gdialServiceImpl = new gdialServiceImpl();
    }
    return m_gdialServiceImpl;
}

void gdialServiceImpl::destroyInstance()
{
    if (m_gdialServiceImpl)
    {
        m_gdialServiceImpl->stop_GDialServer();
        delete m_gdialServiceImpl;
        m_gdialServiceImpl = nullptr;
    }
}

gdialService* gdialService::getInstance(GDialNotifier* observer, const std::vector<std::string>& gdial_args,const std::string& actualprocessName )
{
    GDIAL_LOGTRACE("Entering ...");

    gdial_plat_util_logger_init();

    if (nullptr == m_gdialService)
    {
        m_gdialService = new gdialService();
    }

    gdialServiceImpl* gdialImplInstance = gdialServiceImpl::getInstance();

    if (nullptr != gdialImplInstance)
    {
        char    name[256] = {0};
        std::string process_name = actualprocessName;
        int input_argc = gdial_args.size(),
            overall_argc = input_argc + 1;// store process name
        char    **argv = nullptr,
                **backup_argv = nullptr;
        bool    isAllocated = true;

        argv = new char*[overall_argc];
        backup_argv = new char*[overall_argc];

        if (( nullptr == argv ) || ( nullptr == backup_argv ))
        {
            GDIAL_LOGERROR("Failed to allocate memory");
            isAllocated = false;
        }
        else
        {
            memset(argv,0,(overall_argc*(sizeof(char*))));
            memset(backup_argv,0,(overall_argc*(sizeof(char*))));

            if (process_name.empty())
            {
                prctl(PR_GET_NAME, name, 0, 0, 0);
                process_name = std::string(name);

                if (process_name.empty())
                {
                    process_name = "Unknown";
                }
            }

            argv[0] = new char[process_name.size() + 1];
            if (nullptr == argv[0])
            {
                GDIAL_LOGERROR("Failed to allocate memory");
                isAllocated = false;
            }
            else
            {
                strncpy(argv[0],process_name.c_str(),process_name.size());
                argv[0][process_name.size()] = '\0';
                backup_argv[0] = argv[0];

                GDIAL_LOGINFO("Process Name:[%s]",process_name.c_str());

                for (int i = 0; i < input_argc; ++i)
                {
                    argv[i+1] = new char[gdial_args[i].size() + 1];
                    backup_argv[i+1] = argv[i+1];
                    if (nullptr == argv[i+1])
                    {
                        isAllocated = false;
                        GDIAL_LOGERROR("Failed to allocate memory at %d",i+1);
                        break;
                    }
                    strncpy(argv[i+1], gdial_args[i].c_str(),(gdial_args[i].size()));
                    argv[i+1][gdial_args[i].size()] = '\0';
                    GDIAL_LOGINFO("Args:%d [%s]",i,argv[i+1]);
                }
                GDIAL_LOGINFO("start_GDialServer with argc[%d]",overall_argc);
            }
        }

        if (( false == isAllocated ) || ( 0 != gdialImplInstance->start_GDialServer(overall_argc,argv)))
        {
            GDIAL_LOGERROR("Failed to start GDial server");
            gdialServiceImpl::destroyInstance();
            delete m_gdialService;
            m_gdialService = nullptr;
        }
        else
        {
            GDIAL_LOGINFO("start_GDialServer done ...");
            gdialImplInstance->setService(observer);
        }

        if (nullptr != backup_argv)
        {
            // Free allocated memory after the thread starts
            for (int i = 0; i < overall_argc; ++i)
            {
                if (nullptr != backup_argv[i])
                {
                    delete[] backup_argv[i];
                }
            }
            delete[] backup_argv;
        }

        if (nullptr != argv)
        {
            delete[] argv;
        }
    }
    return m_gdialService;
}

void gdialService::destroyInstance()
{
    if (nullptr != m_gdialService )
    {
        gdialServiceImpl::destroyInstance();
        delete m_gdialService;
        m_gdialService = nullptr;
    }
}

GDIAL_SERVICE_ERROR_CODES gdialService::ApplicationStateChanged(string applicationName, string appState, string applicationId, string error)
{
    gdialServiceImpl* gdialImplInstance = gdialServiceImpl::getInstance();
    GDIAL_LOGTRACE("Entering ...");
    GDIAL_LOGINFO("appName[%s] appId[%s] state[%s] error[%s]",
                    applicationName.c_str(),
                    applicationId.c_str(),
                    appState.c_str(),
                    error.c_str());
    if ((nullptr != m_gdialService ) && (nullptr != gdialImplInstance))
    {
        RequestHandlerPayload payload;
        payload.event = APP_STATE_CHANGED;

        payload.appNameOrfriendlyname = applicationName;
        payload.appIdOractivation = applicationId;
        payload.state = appState;
        payload.error = error;
        gdialImplInstance->sendRequest(payload);
    }
    GDIAL_LOGTRACE("Exiting ...");
    return GDIAL_SERVICE_ERROR_NONE;
}

GDIAL_SERVICE_ERROR_CODES gdialService::ActivationChanged(string activation, string friendlyname)
{
    gdialServiceImpl* gdialImplInstance = gdialServiceImpl::getInstance();
    GDIAL_LOGTRACE("Entering ...");
    GDIAL_LOGINFO("activation[%s] friendlyname[%s]",activation.c_str(),friendlyname.c_str());
    if ((nullptr != m_gdialService ) && (nullptr != gdialImplInstance))
    {
        RequestHandlerPayload payload;
        payload.event = ACTIVATION_CHANGED;

        payload.appNameOrfriendlyname = friendlyname;
        payload.appIdOractivation = activation;
        GDIAL_LOGINFO("ACTIVATION_CHANGED request sent");
        gdialImplInstance->sendRequest(payload);
    }
    GDIAL_LOGTRACE("Exiting ...");
    return GDIAL_SERVICE_ERROR_NONE;
}

GDIAL_SERVICE_ERROR_CODES gdialService::FriendlyNameChanged(string friendlyname)
{
    gdialServiceImpl* gdialImplInstance = gdialServiceImpl::getInstance();
    GDIAL_LOGTRACE("Entering ...");
    GDIAL_LOGINFO("friendlyname[%s]",friendlyname.c_str());
    if ((nullptr != m_gdialService ) && (nullptr != gdialImplInstance))
    {
        RequestHandlerPayload payload;
        payload.event = FRIENDLYNAME_CHANGED;

        payload.appNameOrfriendlyname = friendlyname;
        gdialImplInstance->sendRequest(payload);
    }
    GDIAL_LOGTRACE("Exiting ...");
    return GDIAL_SERVICE_ERROR_NONE;
}

std::string gdialService::getProtocolVersion()
{
    gdialServiceImpl* gdialImplInstance = gdialServiceImpl::getInstance();
    GDIAL_LOGTRACE("Entering ...");
    std::string protocolVersion;
    if ((nullptr != m_gdialService ) && (nullptr != gdialImplInstance))
    {
        protocolVersion = gdial_plat_application_get_protocol_version();
    }
    GDIAL_LOGTRACE("Exiting ...");
    return protocolVersion;
}

GDIAL_SERVICE_ERROR_CODES gdialService::RegisterApplications(RegisterAppEntryList* appConfigList)
{
    gdialServiceImpl* gdialImplInstance = gdialServiceImpl::getInstance();
    GDIAL_LOGTRACE("Entering ...");
    GDIAL_LOGINFO("appConfigList[%p]",appConfigList);
    if ((nullptr != m_gdialService ) && (nullptr != gdialImplInstance))
    {
        RequestHandlerPayload payload;
        payload.event = REGISTER_APPLICATIONS;
        payload.data_param = appConfigList;
        gdialImplInstance->sendRequest(payload);
    }
    GDIAL_LOGTRACE("Exiting ...");
    return GDIAL_SERVICE_ERROR_NONE;
}

void gdialService::setNetworkStandbyMode(bool nwStandbymode)
{
    gdialServiceImpl* gdialImplInstance = gdialServiceImpl::getInstance();
    GDIAL_LOGTRACE("Entering ...");
    GDIAL_LOGINFO("nwStandbymode[%u]",nwStandbymode);
    if ((nullptr != m_gdialService ) && (nullptr != gdialImplInstance))
    {
        RequestHandlerPayload payload;
        payload.event = UPDATE_NW_STANDBY;
        payload.user_param1 = (bool)nwStandbymode;
        gdialImplInstance->sendRequest(payload);
    }
    GDIAL_LOGTRACE("Exiting ...");
}

void gdialServiceImpl::sendRequest( const RequestHandlerPayload& payload )
{
    GDIAL_LOGTRACE("Entering ...");
    std::unique_lock<std::mutex> lk(m_RequestHandlerEventMutex);
    m_RequestHandlerQueue.push(payload);
    m_RequestHandlerThreadRun = true;
    m_RequestHandlerCV.notify_one();
    GDIAL_LOGTRACE("Exiting ...");
}

void gdialServiceImpl::notifyResponse( const ResponseHandlerPayload& payload )
{
    GDIAL_LOGTRACE("Entering ...");
    std::unique_lock<std::mutex> lk(m_ResponseHandlerEventMutex);
    m_ResponseHandlerQueue.push(payload);
    m_ResponseHandlerThreadRun = true;
    m_ResponseHandlerCV.notify_one();
    GDIAL_LOGTRACE("Exiting ...");
}

void gdialServiceImpl::onApplicationLaunchRequestWithLaunchParam(string appName,string strPayLoad, string strQuery, string strAddDataUrl)
{
    ResponseHandlerPayload payload;
    GDIAL_LOGTRACE("Entering ...");
    payload.event = APP_LAUNCH_REQUEST_WITH_PARAMS;

    payload.appName = appName;
    payload.parameterOrPayload = strPayLoad;
    payload.appIdOrQuery = strQuery;
    payload.AddDataUrl = strAddDataUrl;
    notifyResponse(payload);
    GDIAL_LOGTRACE("Exiting ...");
}

void gdialServiceImpl::onApplicationLaunchRequest(string appName, string parameter)
{
    ResponseHandlerPayload payload;
    GDIAL_LOGTRACE("Entering ...");
    payload.event = APP_LAUNCH_REQUEST;

    payload.appName = appName;
    payload.parameterOrPayload = parameter;
    notifyResponse(payload);
    GDIAL_LOGTRACE("Exiting ...");
}

void gdialServiceImpl::onApplicationStopRequest(string appName, string appID)
{
    ResponseHandlerPayload payload;
    GDIAL_LOGTRACE("Entering ...");
    payload.event = APP_STOP_REQUEST;

    payload.appName = appName;
    payload.appIdOrQuery = appID;
    notifyResponse(payload);
    GDIAL_LOGTRACE("Exiting ...");
}

void gdialServiceImpl::onApplicationHideRequest(string appName, string appID)
{
    ResponseHandlerPayload payload;
    GDIAL_LOGTRACE("Entering ...");
    payload.event = APP_HIDE_REQUEST;

    payload.appName = appName;
    payload.appIdOrQuery = appID;
    notifyResponse(payload);
    GDIAL_LOGTRACE("Exiting ...");
}

void gdialServiceImpl::onApplicationResumeRequest(string appName, string appID)
{
    ResponseHandlerPayload payload;
    GDIAL_LOGTRACE("Entering ...");
    payload.event = APP_RESUME_REQUEST;

    payload.appName = appName;
    payload.appIdOrQuery = appID;
    notifyResponse(payload);
    GDIAL_LOGTRACE("Exiting ...");
}

void gdialServiceImpl::onApplicationStateRequest(string appName, string appID)
{
    ResponseHandlerPayload payload;
    GDIAL_LOGTRACE("Entering ...");
    payload.event = APP_STATE_REQUEST;

    payload.appName = appName;
    payload.appIdOrQuery = appID;
    notifyResponse(payload);
    GDIAL_LOGTRACE("Exiting ...");
}

void gdialServiceImpl::onStopped()
{
    //
}

void gdialServiceImpl::updatePowerState(string powerState)
{
    GDIAL_LOGTRACE("Entering ...");
    GDIAL_LOGINFO("powerState : %s",powerState.c_str());
    if (m_observer)
    {
        m_observer->updatePowerState(powerState);
    }
    GDIAL_LOGTRACE("Exiting ...");
}