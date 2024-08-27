/*
 * If not stated otherwise in this file or this component's Licenses.txt file the
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

#include "gdialservice.h"
#include "gdialserviceimpl.h"

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
  g_printerr("signal invalid-uri: [%s]\r\n", signal_message);
}

static void signal_handler_rest_server_gmainloop_quit(GDialRestServer *dial_rest_server, const gchar *signal_message, gpointer user_data) {
  g_return_if_fail(dial_rest_server && signal_message);
  g_printerr("signal gmainloop-quit: [%s]\r\n", signal_message);
  g_print("Exiting DIAL Protocol | %s \r\n", dial_specification_copyright);
  //g_main_loop_quit(m_gdialServiceImpl->m_main_loop);
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

    if (!options_.app_list) {
        g_print(" No application is enabled from cmdline so ignore system app \r\n");
    }
    else{
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
  //g_print(" calling g_main_loop_quit loop_: %p \r\n",m_gdialServiceImpl->m_main_loop);
  //if(m_gdialServiceImpl->m_main_loop)g_main_loop_quit(m_gdialServiceImpl->m_main_loop);
}

static SoupServer * m_servers[3] = {};

int gdialServiceImpl::start_GDialServer(int argc, char *argv[])
{
    GError *error = NULL;
    int returnValue = EXIT_FAILURE;
    GOptionContext *option_context = g_option_context_new(NULL);
    g_option_context_add_main_entries(option_context, option_entries_, NULL);

    if (!g_option_context_parse (option_context, &argc, &argv, &error))
    {
        g_print ("%s\r\n", error->message);
        g_error_free(error);
        return returnValue;
    }

    if (!options_.iface_name)
    {
        options_.iface_name =  g_strdup(GDIAL_IFACE_NAME_DEFAULT);
    }

    #define MAX_RETRY 3
    for(int i=1;i<=MAX_RETRY;i++)
    {
        iface_ipv4_address_ = gdial_plat_util_get_iface_ipv4_addr(options_.iface_name);
        if (!iface_ipv4_address_)
        {
            g_print("Warning: interface %s does not have IP\r\n", options_.iface_name);
            if(i >= MAX_RETRY )
            {
                return returnValue;
            }
            sleep(2);
        }
        else
        {
            break;
        }
    }

    m_main_loop_context = g_main_context_default();
    g_main_context_push_thread_default(m_main_loop_context);
    m_main_loop = g_main_loop_new(m_main_loop_context, FALSE);
    gdial_plat_init(m_main_loop_context);

    gdail_plat_register_activation_cb(server_activation_handler);
    gdail_plat_register_friendlyname_cb(server_friendlyname_handler);
    gdail_plat_register_registerapps_cb (server_register_application);

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
        g_printerr("%s\r\n", error->message);
        g_error_free(error);
        return returnValue;
    }
    else
    {
        listen_address = g_inet_socket_address_new_from_string(iface_ipv4_address_, GDIAL_SSDP_HTTP_PORT);
        success = soup_server_listen(ssdp_http_server, listen_address, option, &error);
        g_object_unref (listen_address);
        if (!success)
        {
            g_printerr("%s\r\n", error->message);
            g_error_free(error);
            return returnValue;
        }
        else
        {
            success = soup_server_listen_local(local_rest_http_server, GDIAL_REST_HTTP_PORT, SOUP_SERVER_LISTEN_IPV4_ONLY, &error);
            if (!success)
            {
                g_printerr("%s\r\n", error->message);
                g_error_free(error);
                return returnValue;
            }
        }
    }
    gchar uuid_str[MAX_UUID_SIZE] = {0};
    char * static_apps_location = getenv("XDIAL_STATIC_APPS_LOCATION");
    if (static_apps_location != NULL && strlen(static_apps_location))
    {
        g_snprintf(uuid_str, MAX_UUID_SIZE, "%s", static_apps_location);
        g_print("static uuid_str  :%s\r\n", uuid_str);
    }
    else
    {
        FILE *fuuid = fopen(UUID_FILE_PATH, "r");
        if (fuuid == NULL)
        {
            uuid_t random_uuid;
            uuid_generate_random(random_uuid);
            uuid_unparse(random_uuid, uuid_str);
            g_print("generated uuid_str  :%s\r\n", uuid_str);
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
        g_print("no application is enabled from cmdline \r\n");
    }
    else
    {
        g_print("app_list to be enabled from command line %s\r\n", options_.app_list);
        size_t app_list_len = strlen(options_.app_list);
        gchar *app_list_low = g_ascii_strdown(options_.app_list, app_list_len);
        if (g_strstr_len(app_list_low, app_list_len, "netflix"))
        {
            g_print("netflix is enabled from cmdline\r\n");
            GList *allowed_origins = g_list_prepend(NULL, ".netflix.com");
            gdial_rest_server_register_app(dial_rest_server, "Netflix", NULL, NULL, TRUE, TRUE, allowed_origins);
            g_list_free(allowed_origins);
        }
        else
        {
            g_print("netflix is not enabled from cmdline\r\n");
        }

        if (g_strstr_len(app_list_low, app_list_len, "youtube"))
        {
            g_print("youtube is enabled from cmdline\r\n");
            GList *allowed_origins = g_list_prepend(NULL, ".youtube.com");
            gdial_rest_server_register_app(dial_rest_server, "YouTube", NULL, NULL, TRUE, TRUE, allowed_origins);
            g_list_free(allowed_origins);
        }
        else
        {
            g_print("youtube is not enabled from cmdline\r\n");
        }

        if (g_strstr_len(app_list_low, app_list_len, "youtubetv"))
        {
            g_print("youtubetv is enabled from cmdline\r\n");
            GList *allowed_origins = g_list_prepend(NULL, ".youtube.com");
            gdial_rest_server_register_app(dial_rest_server, "YouTubeTV", NULL, NULL, TRUE, TRUE, allowed_origins);
            g_list_free(allowed_origins);
        }
        else
        {
            g_print("youtubetv is not enabled from cmdline\r\n");
        }

        if (g_strstr_len(app_list_low, app_list_len, "youtubekids"))
        {
            g_print("youtubekids is enabled from cmdline\r\n");
            GList *allowed_origins = g_list_prepend(NULL, ".youtube.com");
            gdial_rest_server_register_app(dial_rest_server, "YouTubeKids", NULL, NULL, TRUE, TRUE, allowed_origins);
            g_list_free(allowed_origins);
        }
        else
        {
            g_print("youtubekids is not enabled from cmdline\r\n");
        }

        if (g_strstr_len(app_list_low, app_list_len, "amazoninstantvideo"))
        {
            g_print("AmazonInstantVideo is enabled from cmdline\r\n");
            GList *allowed_origins = g_list_prepend(NULL, ".amazonprime.com");
            gdial_rest_server_register_app(dial_rest_server, "AmazonInstantVideo", NULL, NULL, TRUE, TRUE, allowed_origins);
            g_list_free(allowed_origins);
        }
        else
        {
            g_print("AmazonInstantVideo is not enabled from cmdline\r\n");
        }

        if (g_strstr_len(app_list_low, app_list_len, "spotify"))
        {
            g_print("spotify is enabled from cmdline\r\n");
            GList *app_prefixes= g_list_prepend(NULL, "com.spotify");
            GList *allowed_origins = g_list_prepend(NULL, ".spotify.com");
            gdial_rest_server_register_app(dial_rest_server, "com.spotify.Spotify.TV", app_prefixes, NULL, TRUE, TRUE, allowed_origins);
            g_list_free(allowed_origins);
            g_list_free(app_prefixes);
        }
        else
        {
            g_print("spotify is not enabled from cmdline\r\n");
        }

        if (g_strstr_len(app_list_low, app_list_len, "pairing"))
        {
            g_print("pairing is enabled from cmdline\r\n");
            GList *allowed_origins = g_list_prepend(NULL, ".comcast.com");
            gdial_rest_server_register_app(dial_rest_server, "Pairing", NULL, NULL, TRUE, TRUE, allowed_origins);
            g_list_free(allowed_origins);
        }
        else
        {
            g_print("pairing is not enabled from cmdline\r\n");
        }

        if (g_strstr_len(app_list_low, app_list_len, "system"))
        {
            g_print("system is enabled from cmdline\r\n");
            gdial_rest_server_register_app(dial_rest_server, "system", NULL, NULL, TRUE, TRUE, NULL);
        }
        else
        {
            g_print("system is not enabled from cmdline\r\n");
        }

        g_free(app_list_low);
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
            g_print("Listening on %s\n", uri_string);
            g_free(uri_string);
            soup_uri_free(uri->data);
        }
        g_slist_free(uris);
    }

    /*
    * Use global context
    */
    m_main_loop = g_main_loop_new(m_main_loop_context, FALSE);
    g_main_context_pop_thread_default(m_main_loop_context);
    pthread_create(&m_gdialserver_main_thread, nullptr, gdialServiceImpl::GDialMain, this);
    pthread_create(&m_gdialserver_request_handler_thread, nullptr, gdialServiceImpl::GDialRequestHandler, this);
    pthread_create(&m_gdialserver_response_handler_thread, nullptr, gdialServiceImpl::GDialResponseHandler, this);

    if (( 0 == m_gdialserver_main_thread ) || ( 0 == m_gdialserver_request_handler_thread ) || ( 0 == m_gdialserver_response_handler_thread ))
    {
        g_print("Failed to create gdialserver thread");
        stop_GDialServer();
    }
    else
    {
        returnValue = 0;
        gdial_plat_application_service_notification(true,this);
    }
    return returnValue;
}

bool gdialServiceImpl::stop_GDialServer()
{
    g_print("Exiting DIAL Server thread \r\n");
    server_activation_handler(0, "");
    //Sleeping 50 ms to allow existing request to finish processing.
    usleep(50000);
    g_print(" calling g_main_loop_quit loop_: %p \r\n",m_main_loop);

    if (m_main_loop)
    {
        g_main_loop_quit(m_main_loop);
    }
    if (m_gdialserver_main_thread)
    {
        pthread_join(m_gdialserver_main_thread,nullptr);
        m_gdialserver_main_thread = 0;
    }

    for (int i = 0; i < sizeof(m_servers)/sizeof(m_servers[0]); i++)
    {
        soup_server_disconnect(m_servers[i]);
        g_object_unref(m_servers[i]);
    }

    gdial_shield_term();
    gdial_ssdp_destroy();
    g_object_unref(dial_rest_server);
    gdial_plat_application_service_notification(false,nullptr);
    gdial_plat_term();

    if (m_main_loop)
    {
        g_main_loop_unref(m_main_loop);
        m_main_loop = nullptr;
    }
    if (m_main_loop_context)
    {
        g_main_context_unref(m_main_loop_context);
        m_main_loop_context = nullptr;
    }
    return true;
}

void *gdialServiceImpl::GDialMain(void *ctx)
{
    gdialServiceImpl *_instance = (gdialServiceImpl *)ctx;
    g_main_context_push_thread_default(_instance->m_main_loop_context);
    g_main_loop_run(_instance->m_main_loop);
    _instance->m_gdialserver_main_thread = 0;
    pthread_exit(nullptr);
}

void *gdialServiceImpl::GDialRequestHandler(void *ctx)
{
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
            g_print(" threadSendRequest Exiting");
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
        // reqHdlrPayload need to processed
    }
    _instance->m_gdialserver_request_handler_thread = 0;
    pthread_exit(nullptr);
}

void *gdialServiceImpl::GDialResponseHandler(void *ctx)
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
            g_print(" threadSendResponse Exiting");
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
            g_print("sendAppEvent : Event:0x%x",response_data.event);
            switch(response_data.event)
            {
                case APP_LAUNCH_REQUEST_WITH_PARAMS:
                {
                    g_print("AppLaunchWithParam : appName:%s PayLoad:%s  Query:%s PayLoad:%s",
                            response_data.appName.c_str(),
                            response_data.parameterOrPayload.c_str(),
                            response_data.appIdOrQuery.c_str(),
                            response_data.AddDataUrl.c_str());
                    _instance->m_observer->onApplicationLaunchRequestWithLaunchParam(
                                            response_data.appName,
                                            response_data.parameterOrPayload,
                                            response_data.appIdOrQuery,
                                            response_data.AddDataUrl);
                }
                break;
                case APP_LAUNCH_REQUEST:
                {
                    g_print("AppLaunch : appName:%s parameter:%s",response_data.appName.c_str(),response_data.parameterOrPayload.c_str());
                    _instance->m_observer->onApplicationLaunchRequest(
                                            response_data.appName,
                                            response_data.parameterOrPayload);
                }
                break;
                case APP_STOP_REQUEST:
                {
                    g_print("AppStop : appName:%s appId:%s",response_data.appName.c_str(),response_data.appIdOrQuery.c_str());
                    _instance->m_observer->onApplicationStopRequest(
                                            response_data.appName,
                                            response_data.appIdOrQuery);
                }
                break;
                case APP_HIDE_REQUEST:
                {
                    g_print("AppHide : appName:%s appId:%s",response_data.appName.c_str(),response_data.appIdOrQuery.c_str());
                    _instance->m_observer->onApplicationHideRequest(
                                            response_data.appName,
                                            response_data.appIdOrQuery);
                }
                break;
                case APP_STATE_REQUEST:
                {
                    g_print("AppState : appName:%s appId:%s",response_data.appName.c_str(),response_data.appIdOrQuery.c_str());
                    _instance->m_observer->onApplicationStateRequest(
                                            response_data.appName,
                                            response_data.appIdOrQuery);
                }
                break;
                case APP_RESUME_REQUEST:
                {
                    g_print("AppResume : appName:%s appId:%s",response_data.appName.c_str(),response_data.appIdOrQuery.c_str());
                    _instance->m_observer->onApplicationResumeRequest(
                                            response_data.appName,
                                            response_data.appIdOrQuery);
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
    pthread_exit(nullptr);
}

gdialService* gdialService::getInstance(GDialNotifier* observer, const std::vector<std::string>& gdial_args)
{
    if (nullptr == m_gdialService)
    {
        m_gdialService = new gdialService();
        if (nullptr != m_gdialService)
        {
            if (nullptr == m_gdialServiceImpl)
            {
                m_gdialServiceImpl = new gdialServiceImpl();
            }

            if (nullptr != m_gdialServiceImpl)
            {
                int argc = gdial_args.size();
                char** argv = new char*[argc];

                for (int i = 0; i < argc; ++i)
                {
                    argv[i] = new char[gdial_args[i].size() + 1];
                    strncpy(argv[i], gdial_args[i].c_str(),(gdial_args[i].size() + 1));
                }
                if ( 0 != m_gdialServiceImpl->start_GDialServer(argc,argv))
                {
                    g_print("Failed to start GDial server");
                    delete m_gdialServiceImpl;
                    m_gdialServiceImpl = nullptr;
                    delete m_gdialService;
                    m_gdialService = nullptr;
                }
                else
                {
                    m_gdialServiceImpl->setService(observer);
                }

                // Free allocated memory after the thread starts
                for (int i = 0; i < argc; ++i)
                {
                    delete[] argv[i];
                }
                delete[] argv;
            }
        }
    }
    return m_gdialService;
}

void gdialService::destroyInstance()
{
    if (nullptr != m_gdialService )
    {
        if (nullptr != m_gdialServiceImpl )
        {
            m_gdialServiceImpl->stop_GDialServer();
            delete m_gdialServiceImpl;
            m_gdialServiceImpl = nullptr;
        }
        delete m_gdialService;
        m_gdialService = nullptr;
    }
}

int gdialService::ApplicationStateChanged(string applicationName, string applicationId, string state, string error)
{
    if ((nullptr != m_gdialService ) && (nullptr != m_gdialServiceImpl ))
    {
        RequestHandlerPayload payload;
        payload.event = APP_STATE_CHANGED;

        payload.appNameOrfriendlyname = applicationName;
        payload.appIdOractivation = applicationId;
        payload.state = state;
        payload.error = error;
        m_gdialServiceImpl->sendRequest(payload);
    }
}

int gdialService::ActivationChanged(string activation, string friendlyname)
{
    if ((nullptr != m_gdialService ) && (nullptr != m_gdialServiceImpl ))
    {
        RequestHandlerPayload payload;
        payload.event = ACTIVATION_CHANGED;

        payload.appNameOrfriendlyname = friendlyname;
        payload.appIdOractivation = activation;
        m_gdialServiceImpl->sendRequest(payload);
    }
}

int gdialService::FriendlyNameChanged(string friendlyname)
{
    if ((nullptr != m_gdialService ) && (nullptr != m_gdialServiceImpl ))
    {
        RequestHandlerPayload payload;
        payload.event = FRIENDLYNAME_CHANGED;

        payload.appNameOrfriendlyname = friendlyname;
        m_gdialServiceImpl->sendRequest(payload);
    }
}

std::string gdialService::getProtocolVersion()
{
    std::string protocolVersion;
    if ((nullptr != m_gdialService ) && (nullptr != m_gdialServiceImpl ))
    {
        protocolVersion = gdial_plat_application_get_protocol_version();
    }
    return protocolVersion;
}

int gdialService::RegisterApplications(RegisterAppEntryList* appConfigList)
{
    if ((nullptr != m_gdialService ) && (nullptr != m_gdialServiceImpl ))
    {
        RequestHandlerPayload payload;
        payload.event = REGISTER_APPLICATIONS;
        payload.data_param = appConfigList;
        m_gdialServiceImpl->sendRequest(payload);
    }
}

void gdialServiceImpl::sendRequest( const RequestHandlerPayload& payload )
{
    std::unique_lock<std::mutex> lk(m_RequestHandlerEventMutex);
    m_RequestHandlerQueue.push(payload);
    m_RequestHandlerThreadRun = true;
    m_RequestHandlerCV.notify_one();
}

void gdialServiceImpl::notifyResponse( const ResponseHandlerPayload& payload )
{
    std::unique_lock<std::mutex> lk(m_ResponseHandlerEventMutex);
    m_ResponseHandlerQueue.push(payload);
    m_ResponseHandlerThreadRun = true;
    m_ResponseHandlerCV.notify_one();
}

void gdialServiceImpl::onApplicationLaunchRequestWithLaunchParam(string appName,string strPayLoad, string strQuery, string strAddDataUrl)
{
    ResponseHandlerPayload payload;
    payload.event = APP_LAUNCH_REQUEST_WITH_PARAMS;

    payload.appName = appName;
    payload.parameterOrPayload = strPayLoad;
    payload.appIdOrQuery = strQuery;
    payload.AddDataUrl = strAddDataUrl;
    notifyResponse(payload);
}

void gdialServiceImpl::onApplicationLaunchRequest(string appName, string parameter)
{
    ResponseHandlerPayload payload;
    payload.event = APP_LAUNCH_REQUEST;

    payload.appName = appName;
    payload.parameterOrPayload = parameter;
    notifyResponse(payload);
}

void gdialServiceImpl::onApplicationStopRequest(string appName, string appID)
{
    ResponseHandlerPayload payload;
    payload.event = APP_STOP_REQUEST;

    payload.appName = appName;
    payload.appIdOrQuery = appID;
    notifyResponse(payload);
}

void gdialServiceImpl::onApplicationHideRequest(string appName, string appID)
{
    ResponseHandlerPayload payload;
    payload.event = APP_HIDE_REQUEST;

    payload.appName = appName;
    payload.appIdOrQuery = appID;
    notifyResponse(payload);
}

void gdialServiceImpl::onApplicationResumeRequest(string appName, string appID)
{
    ResponseHandlerPayload payload;
    payload.event = APP_RESUME_REQUEST;

    payload.appName = appName;
    payload.appIdOrQuery = appID;
    notifyResponse(payload);
}

void gdialServiceImpl::onApplicationStateRequest(string appName, string appID)
{
    ResponseHandlerPayload payload;
    payload.event = APP_STATE_REQUEST;

    payload.appName = appName;
    payload.appIdOrQuery = appID;
    notifyResponse(payload);
}
