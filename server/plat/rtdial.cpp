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


#include <string>
#include <map>
#include <unistd.h>
#include <pthread.h>
#include <glib.h>

#include "Module.h"
#include <core.h>
#include <plugins.h>
#include <json/JsonData_Netflix.h>
#include <json/JsonData_StateControl.h>
#include <com/Ids.h>
#include <curl/curl.h>
#include <securityagent/SecurityTokenUtil.h>
#include "gdial-app.h"
#include "gdial-plat-dev.h"
#include "gdial-os-app.h"
#include "gdial-config.h"
#include "rtcast.hpp"
#include "rtcache.hpp"
#include "rtdial.hpp"
#include "gdial_app_registry.h"

static GSource *remoteSource = nullptr;
static GMainContext *main_context_ = nullptr;
static int INIT_COMPLETED = 0;
//cache
rtAppStatusCache* AppCache;
static rtdial_activation_cb g_activation_cb = NULL;
static rtdial_friendlyname_cb g_friendlyname_cb = NULL;
static rtdial_registerapps_cb g_registerapps_cb = NULL;

#define DIAL_MAX_NUM_OF_APPS (64)
#define DIAL_MAX_NUM_OF_APP_NAMES (64)
#define DIAL_MAX_NUM_OF_APP_PREFIXES (64)
#define DIAL_MAX_NUM_OF_APP_CORS (64)

class DialCastObject
{

public:

    DialCastObject(const char* SERVICE_NAME){ printf("DialCastObject() const %s\n",SERVICE_NAME); }

    ~DialCastObject() {}

    rtCastError applicationStateChanged(const char *applicationName, const char *applicationId, const char *state, const char *error)
    {
        rtCastError reterror(CAST_ERROR_NONE);
        printf("RTDIAL: DialCastObject::applicationStateChanged \n");
        AppInfo* AppObj = new AppInfo(applicationName,applicationId,state,error);
        printf("applicationStateChanged AppName : %s AppID : %s State : %s Error : %s\n",
                AppObj->appName.c_str(),
                AppObj->appId.c_str(),
                AppObj->appState.c_str(),
                AppObj->appError.c_str());
        AppCache->UpdateAppStatusCache(AppObj);
        return reterror;
    }

    rtCastError friendlyNameChanged(const char* friendlyname)
    {
        rtCastError error(CAST_ERROR_NONE);
        if( g_friendlyname_cb && friendlyname )
        {
            printf("RTDIAL: DialCastObject::friendlyNameChanged :%s \n",friendlyname);
            g_friendlyname_cb(friendlyname);
        }
        return error;
    }

    rtCastError registerApplications(void* appList)
    {
        rtCastError error(CAST_ERROR_NONE);
        printf("RTDIAL : DialCastObject::registerApplications\n");
        RegisterAppEntryList* appConfigList = static_cast<RegisterAppEntryList*>(appList);
        GList *gAppList = NULL;
        int i = 0;
    
        for (RegisterAppEntry* appEntry : appConfigList->getValues()) 
        {
            GList *gAppPrefxes = nullptr,
                  *allowed_origins = nullptr;
            if (DIAL_MAX_NUM_OF_APPS<=i)
            {
                break;
            }
            printf("Application: %d \n", i);
            gAppPrefxes = g_list_prepend (gAppPrefxes, g_strdup(appEntry->prefixes.c_str()));
            printf("%s, ", appEntry->prefixes.c_str());
            printf("\n");

            allowed_origins = g_list_prepend (allowed_origins, g_strdup(appEntry->cors.c_str()));
            printf("%s, ", appEntry->cors.c_str());
            printf("\n");

            GHashTable *gProperties = g_hash_table_new_full(g_str_hash, g_str_equal, g_free, g_free);
            std::string appAllowStop = appEntry->allowStop ? "true" : "false";
            g_hash_table_insert(gProperties,g_strdup("allowStop"),g_strdup(appAllowStop.c_str()));
            printf("allowStop: %s", appAllowStop.c_str());
            printf("\n");

            GDialAppRegistry*  app_registry = gdial_app_registry_new( g_strdup(appEntry->Names.c_str()),
                                                                      gAppPrefxes,
                                                                      gProperties,
                                                                      TRUE,
                                                                      TRUE,
                                                                      allowed_origins);
            gAppList = g_list_prepend (gAppList, app_registry);
            printf("%s, ", appEntry->Names.c_str());
            printf("\n");
            ++i;
        }

        int appListSize = g_list_length (gAppList);
        if( g_registerapps_cb ) {
            printf("RTDIAL: DialCastObject:: calling register_applications callback \n");
            g_registerapps_cb(gAppList);
        }
        /*Free the applist*/
        if (gAppList) {
            g_list_free (gAppList);
            gAppList = NULL;
        }
        delete appConfigList;
        return error;
    }

    const char* getProtocolVersion()
    {
        printf("RTDIAL : DialCastObject::getProtocolVersion \n");
        return GDIAL_PROTOCOL_VERSION_STR;
    }

    rtCastError activationChanged(std::string status, std::string friendlyName)
    {
        rtCastError error(CAST_ERROR_NONE);
        printf("RTDIAL: DialCastObject::activationChanged status: %s friendlyname: %s \n",
                status.c_str(),friendlyName.c_str());
        if( g_activation_cb )
        {
            if(!strcmp(status.c_str(), "true"))
            {
                g_activation_cb(1,friendlyName.c_str());
            }
            else
            {
                g_activation_cb(0,friendlyName.c_str());
            }
            printf("RTDIAL: DialCastObject:: status: %s  g_activation_cb :%d \n",status.c_str(), g_activation_cb);
        }
        return error;
    }
    
    rtCastError launchApplication(const char* appName, const char* args){
        rtCastError error(CAST_ERROR_NONE);
        printf("RTDIAL: DialCastObject::launchApplication App:%s  args:%s\n",appName,args);
        m_observer->onApplicationLaunchRequest(appName,args);
        return error;
    }

    rtCastError launchApplicationWithLaunchParams(const char *appName, const char *argPayload, const char *argQueryString, const char *argAdditionalDataUrl) {
        rtCastError error(CAST_ERROR_NONE);
        printf("RTDIAL: DialCastObject::launchApplicationWithLaunchParams App:%s  payload:%s query_string:%s additional_data_url:%s \n",
                appName, argPayload, argQueryString, argAdditionalDataUrl);
        m_observer->onApplicationLaunchRequestWithLaunchParam(appName,argPayload,argQueryString,argAdditionalDataUrl);
        return error;
    }

    rtCastError hideApplication(const char* appName, const char* appID) {
        rtCastError error(CAST_ERROR_NONE);
        printf("RTDIAL: DialCastObject::hideApplication App:%s  ID:%s\n",appName,appID);
        m_observer->onApplicationHideRequest(appName,appID);
        return error;
    }

    rtCastError resumeApplication(const char* appName, const char* appID) {
        rtCastError error(CAST_ERROR_NONE);
        printf("RTDIAL: DialCastObject::resumeApplication App:%s  ID:%s\n",appName,appID);
        m_observer->onApplicationResumeRequest(appName,appID);
        return error;
    }

    rtCastError stopApplication(const char* appName, const char* appID) {
        rtCastError error(CAST_ERROR_NONE);
        printf("RTDIAL: DialCastObject::stopApplication App:%s  ID:%s\n",appName,appID);
        m_observer->onApplicationStopRequest(appName,appID);
        return error;
    }

    rtCastError getApplicationState(const char* appName, const char* appID) {
        rtCastError error(CAST_ERROR_NONE);
        printf("RTDIAL: DialCastObject::getApplicationState App:%s  ID:%s\n",appName,appID);
        m_observer->onApplicationStateRequest(appName,appID);
        return error;
    }

    void setService(GDialNotifier* service)
    {
        m_observer = service;
    }
private:
    GDialNotifier *m_observer;
};

DialCastObject* DialObj;

#if 0
static gboolean pumpRemoteObjectQueue(gpointer data)
{
//    printf("### %s  :  %s  :  %d   ### \n",__FILE__,__func__,__LINE__);
    CastError err;
    GSource *source = (GSource *)data;
    do {
        g_source_set_ready_time(source, -1);
        //err = rtRemoteProcessSingleItem();
    } while (err == CAST_ERROR_NONE);
    if (err != CAST_ERROR_NONE && err != RT_ERROR_QUEUE_EMPTY) {
//        printf("RTDIAL: rtRemoteProcessSingleItem() returned %s", rtStrError(err));
        return G_SOURCE_CONTINUE;
    }
    return G_SOURCE_CONTINUE;
}

static GSource *attachRtRemoteSource()
{
//    printf("### %s  :  %s  :  %d   ###\n",__FILE__,__func__,__LINE__);
    static GSourceFuncs g_sourceFuncs =
        {
            nullptr, // prepare
            nullptr, // check
            [](GSource *source, GSourceFunc callback, gpointer data) -> gboolean // dispatch
            {
                if (g_source_get_ready_time(source) != -1) {
                    g_source_set_ready_time(source, -1);
                    return callback(data);
                }
                return G_SOURCE_CONTINUE;
            },
            nullptr, // finalize
            nullptr, // closure_callback
            nullptr, // closure_marshall
        };
    GSource *source = g_source_new(&g_sourceFuncs, sizeof(GSource));
    g_source_set_name(source, "RT Remote Event dispatcher");
    g_source_set_can_recurse(source, TRUE);
    g_source_set_callback(source, pumpRemoteObjectQueue, source, nullptr);
    g_source_set_priority(source, G_PRIORITY_HIGH);

    CastError e = rtRemoteRegisterQueueReadyHandler(env, [](void *data) -> void {
        GSource *source = (GSource *)data;
        g_source_set_ready_time(source, 0);
    }, source);

    if (e != CAST_ERROR_NONE)
    {
        printf("RTDIAL: Failed to register queue handler: %d", e);
        g_source_destroy(source);
        return nullptr;
    }
    g_source_attach(source, main_context_);
    return source;
}
#endif

void rtdail_register_activation_cb(rtdial_activation_cb cb)
{
  g_activation_cb = cb;
}

void rtdail_register_friendlyname_cb(rtdial_friendlyname_cb cb)
{
   g_friendlyname_cb = cb;
}

void rtdail_register_registerapps_cb(rtdial_registerapps_cb cb)
{
   g_registerapps_cb = cb;
}

bool rtdial_init(GMainContext *context) {
    if(INIT_COMPLETED)
       return true;
    CastError err;
    const char* objName;

    gdial_plat_dev_initialize();
    //env = rtEnvironmentGetGlobal();
    //err = rtRemoteInit(env);

//cache
    //AppCache = new rtAppStatusCache(env);
    AppCache = new rtAppStatusCache();

    printf("RTDIAL: %s\n",__func__);

    if (err != CAST_ERROR_NONE){
        printf("RTDIAL: rtRemoteinit Failed\n");
        return false;
    }

    main_context_ = g_main_context_ref(context);
    //remoteSource = attachRtRemoteSource();

    //if (!remoteSource)
    //   printf("RTDIAL: Failed to attach rt remote source");

    objName =  getenv("PX_WAYLAND_CLIENT_REMOTE_OBJECT_NAME");
    if(!objName) objName = "com.comcast.xdialcast";

    DialObj = new DialCastObject("com.comcast.xdialcast");

    INIT_COMPLETED =1;
    return true;
}

void rtdial_term() {
    printf("RTDIAL: %s \n",__FUNCTION__);

    gdial_plat_dev_deinitialize();
    g_main_context_unref(main_context_);
    g_source_unref(remoteSource);

    //DialObj->bye();
    delete (AppCache);
#if 0
    CastError e = rtRemoteShutdown();
    if (e != CAST_ERROR_NONE)
    {
      printf("RTDIAL: rtRemoteShutdown failed: %s \n", rtStrError(e));
    }
#endif
    //delete(DialObj);
}

/*
 * The maximum DIAL payload accepted per the DIAL 1.6.1 specification.
 */
#define DIAL_MAX_PAYLOAD (4096)

/*
 * The maximum additionalDataUrl length
 */

#define DIAL_MAX_ADDITIONALURL (1024)

map<string,string> parse_query(const char* query_string) {
    if (!query_string) return {};
    char *unescaped = g_uri_unescape_string(query_string, nullptr);
    // if unescaping failed due to invalid characters in the string, g_uri_unescape_segment returns null
    // e.g. '%' character should be encoded as '%25'; we still fallback to undecoded string in such case
    std::string query {unescaped ? unescaped : query_string};
    if (unescaped) {
        g_free(unescaped);
        unescaped = nullptr;
    }

    map<string,string> ret;
    size_t begin = 0, end;

    while (begin < query.size()) {
        end = query.find('&', begin);
        if (end == string::npos) end = query.size();
        string next {query.substr(begin, end - begin)};
        size_t split = next.find('=');
        if (split > 0 && split != string::npos) {
            ret[next.substr(0, split)] = next.substr(split + 1,string::npos);
        }
        begin = end + 1;
    }
    return ret;
}

int gdial_os_application_start(const char *app_name, const char *payload, const char *query_string, const char *additional_data_url, int *instance_id) {
    printf("RTDIAL gdial_os_application_start : Application launch request: appName: %s  query: [%s], payload: [%s], additionalDataUrl [%s]\n",
        app_name, query_string, payload, additional_data_url);

    if (strcmp(app_name,"system") == 0) {
        auto parsed_query{parse_query(query_string)};
        if (parsed_query["action"] == "sleep") {
            const char *system_key = getenv("SYSTEM_SLEEP_REQUEST_KEY");
            if (system_key && parsed_query["key"] != system_key) {
                printf("RTDIAL: system app request to change device to sleep mode, key comparison failed: user provided '%s'\n", parsed_query["key"].c_str());
                return GDIAL_APP_ERROR_INTERNAL;
            }
            printf("RTDIAL: system app request to change device to sleep mode\n");
            gdial_plat_dev_set_power_state_off();
            return GDIAL_APP_ERROR_NONE;
        }
        else if (parsed_query["action"] == "togglepower") {
            const char *system_key = getenv("SYSTEM_SLEEP_REQUEST_KEY");
            if (system_key && parsed_query["key"] != system_key) {
                printf("RTDIAL: system app request to toggle the power state, key comparison failed: user provided '%s'\n", parsed_query["key"].c_str());
                return GDIAL_APP_ERROR_INTERNAL;
            }
            printf("RTDIAL: system app request to toggle the power state \n");
            gdial_plat_dev_toggle_power_state();
            return GDIAL_APP_ERROR_NONE;
        }
    }
    gdial_plat_dev_set_power_state_on();
    rtCastError ret = DialObj->launchApplicationWithLaunchParams(app_name, payload, query_string, additional_data_url);
    if (RTCAST_ERROR_CAST(ret) != CAST_ERROR_NONE) {
        printf("RTDIAL: DialObj.launchApplication failed!!! Error=%x\n",RTCAST_ERROR_CAST(ret));
        return GDIAL_APP_ERROR_INTERNAL;
    }
    return GDIAL_APP_ERROR_NONE;
}

using namespace WPEFramework;
JSONRPC::LinkType<Core::JSON::IElement> *netflixRemoteObject = NULL;
JSONRPC::LinkType<Core::JSON::IElement> *controllerRemoteObject = NULL;
#define MAX_LENGTH 1024

#ifdef NETFLIX_CALLSIGN_0
const std::string nfx_callsign = "Netflix-0";
#else
const std::string nfx_callsign = "Netflix";
#endif

std::string GetCurrentState() {
     std::cout<<"GetCurrentState()"<<std::endl;
     std::string netflixState = "";
     Core::JSON::ArrayType<PluginHost::MetaData::Service> pluginResponse;
     Core::SystemInfo::SetEnvironment(_T("THUNDER_ACCESS"), (_T("127.0.0.1:9998")));
     unsigned char buffer[MAX_LENGTH] = {0};

     //Obtaining controller object
     if (NULL == controllerRemoteObject) {
         int ret = GetSecurityToken(MAX_LENGTH,buffer);
         if(ret<0)
         {
           controllerRemoteObject = new JSONRPC::LinkType<Core::JSON::IElement>(std::string());
         } else {
           string sToken = (char*)buffer;
           string query = "token=" + sToken;
           printf("Security token = %s \n",query.c_str());
           controllerRemoteObject = new JSONRPC::LinkType<Core::JSON::IElement>(std::string(), false, query);
         }
     }
     std::string nfxstatus = "status@" + nfx_callsign;
     if(controllerRemoteObject->Get(1000, _T(nfxstatus), pluginResponse) == Core::ERROR_NONE)
     {
         printf("Obtained netflix status = %s\n",nfxstatus.c_str());
         Core::JSON::ArrayType<PluginHost::MetaData::Service>::Iterator index(pluginResponse.Elements());
         while (index.Next() == true) {
                netflixState = index.Current().JSONState.Data();
         } //end of while loop
     } //end of if case for querrying
     printf("Netflix State = %s\n",netflixState.c_str());
     return netflixState;
}
void stop_netflix()
{
   JsonObject parameters;
   JsonObject response;
   parameters["callsign"] = nfx_callsign;
   if (Core::ERROR_NONE == controllerRemoteObject->Invoke("deactivate", parameters, response)) {
        std::cout << "Netflix is stoppped" << std::endl;
   } else {
        std::cout << "Netflix could not be deactivated" << std::endl;
   }
}

int gdial_os_application_stop(const char *app_name, int instance_id) {
    printf("RTDIAL gdial_os_application_stop: appName = %s appID = %s\n",app_name,std::to_string(instance_id).c_str());
    if((strcmp(app_name,"system") == 0)){
        printf("RTDIAL delete not supported for system app return GDIAL_APP_ERROR_BAD_REQUEST\n");
        return GDIAL_APP_ERROR_BAD_REQUEST;
    }
    std::string State = AppCache->SearchAppStatusInCache(app_name);
    /* always to issue stop request to have a failsafe strategy */
    if (0 && State != "running")
        return GDIAL_APP_ERROR_BAD_REQUEST;

    char* enable_stop = getenv("ENABLE_NETFLIX_STOP");
    if ( enable_stop != NULL ) {
       if ( strcmp(app_name,"Netflix") == 0 && strcmp(enable_stop,"true") == 0) {
           printf("NTS TESTING: force shutdown Netflix thunder plugin\n");
           stop_netflix();
           sleep(1);
       }
    }
    rtCastError ret = DialObj->stopApplication(app_name,std::to_string(instance_id).c_str()); 
    if (RTCAST_ERROR_CAST(ret) != CAST_ERROR_NONE) {
        printf("RTDIAL: DialObj.stopApplication failed!!! Error=%x\n",RTCAST_ERROR_CAST(ret));
        return GDIAL_APP_ERROR_INTERNAL;
    }
    return GDIAL_APP_ERROR_NONE;
}

int gdial_os_application_hide(const char *app_name, int instance_id) {
    if((strcmp(app_name,"system") == 0)){
        printf("RTDIAL system app already in hidden state\n");
        return GDIAL_APP_ERROR_NONE;
    }
    #if 0
    printf("RTDIAL gdial_os_application_hide-->stop: appName = %s appID = %s\n",app_name,std::to_string(instance_id).c_str());
    std::string State = AppCache->SearchAppStatusInCache(app_name);
    if (0 && State != "running") {
        return GDIAL_APP_ERROR_BAD_REQUEST;
    }
    // Report Hide request not implemented for Youtube for ceritifcation requirement.
    if(strncmp("YouTube", app_name, 7) != 0) {
       rtCastError ret = DialObj->stopApplication(app_name,std::to_string(instance_id).c_str());
       if (RTCAST_ERROR_CAST(ret) != CAST_ERROR_NONE) {
           printf("RTDIAL: DialObj.stopApplication failed!!! Error=%x\n",RTCAST_ERROR_CAST(ret));
           return GDIAL_APP_ERROR_INTERNAL;
       }
       return GDIAL_APP_ERROR_NONE;
    }
    return GDIAL_APP_ERROR_NONE;
    #else
    printf("RTDIAL gdial_os_application_hide: appName = %s appID = %s\n",app_name,std::to_string(instance_id).c_str());
    std::string State = AppCache->SearchAppStatusInCache(app_name);
    if (State != "running")
        return GDIAL_APP_ERROR_BAD_REQUEST;
    rtCastError ret = DialObj->hideApplication(app_name,std::to_string(instance_id).c_str());
    if (RTCAST_ERROR_CAST(ret) != CAST_ERROR_NONE) {
        printf("RTDIAL: DialObj.hideApplication failed!!! Error=%x\n",RTCAST_ERROR_CAST(ret));
        return GDIAL_APP_ERROR_INTERNAL;
    }
    return GDIAL_APP_ERROR_NONE;
    #endif
}

int gdial_os_application_resume(const char *app_name, int instance_id) {
    printf("RTDIAL gdial_os_application_resume: appName = %s appID = %s\n",app_name,std::to_string(instance_id).c_str());

    if((strcmp(app_name,"system") == 0)){
        printf("RTDIAL system app can not be resume\n");
        return GDIAL_APP_ERROR_NONE;
    }
    std::string State = AppCache->SearchAppStatusInCache(app_name);
    if (State == "running")
        return GDIAL_APP_ERROR_BAD_REQUEST;
    rtCastError ret = DialObj->resumeApplication(app_name,std::to_string(instance_id).c_str());
    if (RTCAST_ERROR_CAST(ret) != CAST_ERROR_NONE) {
        printf("RTDIAL: DialObj.resumeApplication failed!!! Error=%x\n",RTCAST_ERROR_CAST(ret));
        return GDIAL_APP_ERROR_INTERNAL;
    }
    return GDIAL_APP_ERROR_NONE;
}

int gdial_os_application_state(const char *app_name, int instance_id, GDialAppState *state) {
    printf("RTDIAL gdial_os_application_state: App = %s \n",app_name);
    if((strcmp(app_name,"system") == 0)){
        *state = GDIAL_APP_STATE_HIDE;
        printf("RTDIAL getApplicationState: AppState = suspended \n");
        return GDIAL_APP_ERROR_NONE;
    }
    std::string State = AppCache->SearchAppStatusInCache(app_name);
    printf("RTDIAL getApplicationState: AppState = %s \n",State.c_str());
    /*
     *  return cache, but also trigger a refresh
     */
    if((strcmp(app_name,"system") != 0) &&( true || State == "NOT_FOUND")) {
        rtCastError ret = DialObj->getApplicationState(app_name,NULL);
        if (RTCAST_ERROR_CAST(ret) != CAST_ERROR_NONE) {
            printf("RTDIAL: DialObj.getApplicationState failed!!! Error: %x\n",RTCAST_ERROR_CAST(ret));
            return GDIAL_APP_ERROR_INTERNAL;
        }
    }

    if (State == "running") {
        *state = GDIAL_APP_STATE_RUNNING;
    }
    else if (State == "suspended" || State == "hidden") {
        *state = GDIAL_APP_STATE_HIDE;
    }
    else {
        *state = GDIAL_APP_STATE_STOPPED;
    }

    char* enable_stop = getenv("ENABLE_NETFLIX_STOP");
    if ( enable_stop != NULL ) {
       if (strcmp(app_name,"Netflix") == 0 && strcmp(enable_stop,"true") == 0) {
         std::string app_state = GetCurrentState();
         printf("RTDIAL: Presence of Netflix thunder plugin state = %s to confirm state\r\n", app_state.c_str());
         if (app_state == "deactivated") {
           *state = GDIAL_APP_STATE_STOPPED;
           printf("RTDIAL: app [%s] state converted to [%d]\r\n", app_name, *state);
         }
         else if (app_state == "suspended" || app_state == "hidden")
         {
            *state = GDIAL_APP_STATE_HIDE;
            printf("RTDIAL: app [%s] state converted to [%d]\r\n", app_name, *state);
         }
	 else {
	    *state = GDIAL_APP_STATE_RUNNING;	 
         }		 
       }
    }

    return GDIAL_APP_ERROR_NONE;
}

int gdial_os_application_state_changed(const char *applicationName, const char *applicationId, const char *state, const char *error)
{
    printf("RTDIAL gdial_os_application_state_changed : appName: %s  appId: [%s], state: [%s], error [%s]\n",
        applicationName, applicationId, state, error);

    rtCastError ret = DialObj->applicationStateChanged(applicationName,applicationId,state,error);

    if (RTCAST_ERROR_CAST(ret) != CAST_ERROR_NONE) {
        printf("RTDIAL: DialObj.applicationStateChanged failed!!! Error=%x\n",RTCAST_ERROR_CAST(ret));
        return GDIAL_APP_ERROR_INTERNAL;
    }
    return GDIAL_APP_ERROR_NONE;
}

int gdial_os_application_activation_changed(const char *activation, const char *friendlyname)
{
    printf("RTDIAL gdial_os_application_activation_changed : activation: %s  friendlyname: [%s]\n",activation, friendlyname);

    rtCastError ret = DialObj->activationChanged(activation,friendlyname);

    if (RTCAST_ERROR_CAST(ret) != CAST_ERROR_NONE) {
        printf("RTDIAL: DialObj.friendlyNameChanged failed!!! Error=%x\n",RTCAST_ERROR_CAST(ret));
        return GDIAL_APP_ERROR_INTERNAL;
    }
    return GDIAL_APP_ERROR_NONE;
}

int gdial_os_application_friendlyname_changed(const char *friendlyname)
{
    printf("RTDIAL gdial_os_application_friendlyname_changed : friendlyname: [%s]\n",friendlyname);

    rtCastError ret = DialObj->friendlyNameChanged(friendlyname);

    if (RTCAST_ERROR_CAST(ret) != CAST_ERROR_NONE) {
        printf("RTDIAL: DialObj.friendlyNameChanged failed!!! Error=%x\n",RTCAST_ERROR_CAST(ret));
        return GDIAL_APP_ERROR_INTERNAL;
    }
    return GDIAL_APP_ERROR_NONE;
}

const char* gdial_os_application_get_protocol_version(void)
{
    printf("RTDIAL gdial_os_application_activation_changed\n");

    return (DialObj->getProtocolVersion());
}

int gdial_os_application_register_applications(void* appList)
{
    printf("RTDIAL gdial_os_application_register_applications :\n");

    rtCastError ret = DialObj->registerApplications(appList);

    if (RTCAST_ERROR_CAST(ret) != CAST_ERROR_NONE) {
        printf("RTDIAL: DialObj.registerApplications failed!!! Error=%x\n",RTCAST_ERROR_CAST(ret));
        return GDIAL_APP_ERROR_INTERNAL;
    }
    return GDIAL_APP_ERROR_NONE;
}

int gdial_os_application_service_notification(gboolean isNotifyRequired, void* notifier)
{
    printf("RTDIAL gdial_os_application_register_applications : %u\n",isNotifyRequired);
    if (isNotifyRequired)
    {
        DialObj->setService(static_cast<GDialNotifier*>(notifier));
    }
    else
    {
        DialObj->setService(nullptr);
    }
}
