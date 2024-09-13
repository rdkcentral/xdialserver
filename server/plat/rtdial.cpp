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
#include "gdialservicelogging.h"

//static GSource *remoteSource = nullptr;
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

    DialCastObject(const char* SERVICE_NAME){ GDIAL_LOGINFO("DialCastObject() const %s",SERVICE_NAME); }

    ~DialCastObject() {}

    rtCastError applicationStateChanged(const char *applicationName, const char *applicationId, const char *state, const char *error)
    {
        rtCastError reterror(CAST_ERROR_NONE);
        GDIAL_LOGINFO("RTDIAL: DialCastObject::applicationStateChanged ");
        AppInfo* AppObj = new AppInfo(applicationName,applicationId,state,error);
        GDIAL_LOGINFO("applicationStateChanged AppName : %s AppID : %s State : %s Error : %s",
                AppObj->appName.c_str(),
                AppObj->appId.c_str(),
                AppObj->appState.c_str(),
                AppObj->appError.c_str());
        AppCache->UpdateAppStatusCache(AppObj);
        GDIAL_LOGINFO("Exiting ...");
        return reterror;
    }

    rtCastError friendlyNameChanged(const char* friendlyname)
    {
        GDIAL_LOGINFO("Entering ...");
        rtCastError error(CAST_ERROR_NONE);
        if( g_friendlyname_cb && friendlyname )
        {
            GDIAL_LOGINFO("RTDIAL: DialCastObject::friendlyNameChanged :%s ",friendlyname);
            g_friendlyname_cb(friendlyname);
        }
        GDIAL_LOGINFO("Exiting ...");
        return error;
    }

    rtCastError registerApplications(void* appList)
    {
        rtCastError error(CAST_ERROR_NONE);
        GDIAL_LOGINFO("RTDIAL : DialCastObject::registerApplications");
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
            GDIAL_LOGINFO("Application: %d ", i);
            gAppPrefxes = g_list_prepend (gAppPrefxes, g_strdup(appEntry->prefixes.c_str()));
            GDIAL_LOGINFO("%s, ", appEntry->prefixes.c_str());
            GDIAL_LOGINFO("");

            allowed_origins = g_list_prepend (allowed_origins, g_strdup(appEntry->cors.c_str()));
            GDIAL_LOGINFO("%s, ", appEntry->cors.c_str());
            GDIAL_LOGINFO("");

            GHashTable *gProperties = g_hash_table_new_full(g_str_hash, g_str_equal, g_free, g_free);
            std::string appAllowStop = appEntry->allowStop ? "true" : "false";
            g_hash_table_insert(gProperties,g_strdup("allowStop"),g_strdup(appAllowStop.c_str()));
            GDIAL_LOGINFO("allowStop: %s", appAllowStop.c_str());
            GDIAL_LOGINFO("");

            GDialAppRegistry*  app_registry = gdial_app_registry_new( g_strdup(appEntry->Names.c_str()),
                                                                      gAppPrefxes,
                                                                      gProperties,
                                                                      TRUE,
                                                                      TRUE,
                                                                      allowed_origins);
            gAppList = g_list_prepend (gAppList, app_registry);
            GDIAL_LOGINFO("%s, ", appEntry->Names.c_str());
            GDIAL_LOGINFO("");
            ++i;
        }

        //int appListSize = g_list_length (gAppList);
        if( g_registerapps_cb ) {
            GDIAL_LOGINFO("RTDIAL: DialCastObject:: calling register_applications callback ");
            g_registerapps_cb(gAppList);
        }
        /*Free the applist*/
        if (gAppList) {
            g_list_free (gAppList);
            gAppList = NULL;
        }

        GDIAL_LOGINFO("[%p] Freeing appConfigList",appConfigList);
        delete appConfigList;
        GDIAL_LOGINFO("Exiting ...");
        return error;
    }

    const char* getProtocolVersion()
    {
        GDIAL_LOGINFO("RTDIAL : DialCastObject::getProtocolVersion ");
        return GDIAL_PROTOCOL_VERSION_STR;
    }

    rtCastError activationChanged(std::string status, std::string friendlyName)
    {
        rtCastError error(CAST_ERROR_NONE);
        GDIAL_LOGINFO("RTDIAL: DialCastObject::activationChanged status: %s friendlyname: %s ",
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
            GDIAL_LOGINFO("RTDIAL: DialCastObject:: status: %s  g_activation_cb :%p",status.c_str(), g_activation_cb);
        }
        GDIAL_LOGINFO("Exiting ...");
        return error;
    }

    void updateNetworkStandbyMode(gboolean nwstandbyMode)
    {
        gdial_plat_dev_nwstandby_mode_change(nwstandbyMode);
    }
    
    rtCastError launchApplication(const char* appName, const char* args){
        rtCastError error(CAST_ERROR_NONE);
        GDIAL_LOGINFO("RTDIAL: DialCastObject::launchApplication App:%s  args:%s",appName,args);
        if (nullptr!=m_observer)
        {
            std::string applicationName = "",
                        parameter = "";
            if (nullptr!=appName)
                applicationName = appName;
            if (nullptr!=args)
                parameter = args;
            m_observer->onApplicationLaunchRequest(applicationName,parameter);
        }
        GDIAL_LOGINFO("Exiting ...");
        return error;
    }

    rtCastError launchApplicationWithLaunchParams(const char *appName, const char *argPayload, const char *argQueryString, const char *argAdditionalDataUrl) {
        rtCastError error(CAST_ERROR_NONE);
        GDIAL_LOGINFO("RTDIAL: DialCastObject::launchApplicationWithLaunchParams App:%s  payload:%s query_string:%s additional_data_url:%s ",
                appName, argPayload, argQueryString,argAdditionalDataUrl );
        if (nullptr!=m_observer)
        {
            std::string applicationName = "",
                        payLoad = "",
                        queryString = "",
                        additionalDataUrl = "";
            if (nullptr!=appName)
                applicationName = appName;
            if (nullptr!=argPayload)
                payLoad = argPayload;
            if (nullptr!=argQueryString)
                queryString = argQueryString;
            if (nullptr!=argAdditionalDataUrl)
                additionalDataUrl = argAdditionalDataUrl;
            m_observer->onApplicationLaunchRequestWithLaunchParam( applicationName,
                                                                   payLoad,
                                                                   queryString,
                                                                   additionalDataUrl );
        }
        GDIAL_LOGINFO("Exiting ...");
        return error;
    }

    rtCastError hideApplication(const char* appName, const char* appID) {
        rtCastError error(CAST_ERROR_NONE);
        GDIAL_LOGINFO("RTDIAL: DialCastObject::hideApplication App:%s  ID:%s",appName,appID);
        if (nullptr!=m_observer)
        {
            std::string applicationName = "",
                        applicationId = "";
            if (nullptr!=appName)
                applicationName = appName;
            if (nullptr!=appID)
                applicationId = appID;
            m_observer->onApplicationHideRequest(applicationName,applicationId);
        }
        GDIAL_LOGINFO("Exiting ...");
        return error;
    }

    rtCastError resumeApplication(const char* appName, const char* appID) {
        rtCastError error(CAST_ERROR_NONE);
        GDIAL_LOGINFO("RTDIAL: DialCastObject::resumeApplication App:%s  ID:%s",appName,appID);
        if (nullptr!=m_observer)
        {
            std::string applicationName = "",
                        applicationId = "";
            if (nullptr!=appName)
                applicationName = appName;
            if (nullptr!=appID)
                applicationId = appID;
            m_observer->onApplicationResumeRequest(applicationName,applicationId);
        }
        GDIAL_LOGINFO("Exiting ...");
        return error;
    }

    rtCastError stopApplication(const char* appName, const char* appID) {
        rtCastError error(CAST_ERROR_NONE);
        GDIAL_LOGINFO("RTDIAL: DialCastObject::stopApplication App:%s  ID:%s",appName,appID);
        if (nullptr!=m_observer)
        {
            std::string applicationName = "",
                        applicationId = "";
            if (nullptr!=appName)
                applicationName = appName;
            if (nullptr!=appID)
                applicationId = appID;
            m_observer->onApplicationStopRequest(applicationName,applicationId);
        }
        GDIAL_LOGINFO("Exiting ...");
        return error;
    }

    rtCastError getApplicationState(const char* appName, const char* appID) {
        rtCastError error(CAST_ERROR_NONE);
        GDIAL_LOGINFO("RTDIAL: DialCastObject::getApplicationState App:%s  ID:%s",appName,appID);
        if (nullptr!=m_observer)
        {
            std::string applicationName = "",
                        applicationId = "";
            if (nullptr!=appName)
                applicationName = appName;
            if (nullptr!=appID)
                applicationId = appID;
            m_observer->onApplicationStateRequest(applicationName,applicationId);
        }
        GDIAL_LOGINFO("Exiting ...");
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
//    GDIAL_LOGINFO("### %s  :  %s  :  %d   ### ",__FILE__,__func__,__LINE__);
    CastError err;
    GSource *source = (GSource *)data;
    do {
        g_source_set_ready_time(source, -1);
        //err = rtRemoteProcessSingleItem();
    } while (err == CAST_ERROR_NONE);
    if (err != CAST_ERROR_NONE && err != RT_ERROR_QUEUE_EMPTY) {
//        GDIAL_LOGINFO("RTDIAL: rtRemoteProcessSingleItem() returned %s", rtStrError(err));
        return G_SOURCE_CONTINUE;
    }
    return G_SOURCE_CONTINUE;
}

static GSource *attachRtRemoteSource()
{
//    GDIAL_LOGINFO("### %s  :  %s  :  %d   ###",__FILE__,__func__,__LINE__);
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
        GDIAL_LOGINFO("RTDIAL: Failed to register queue handler: %d", e);
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
    CastError err = CAST_ERROR_NONE;
    const char* objName;

    //gdial_plat_dev_initialize();
    //env = rtEnvironmentGetGlobal();
    //err = rtRemoteInit(env);

//cache
    //AppCache = new rtAppStatusCache(env);
    AppCache = new rtAppStatusCache();

    GDIAL_LOGINFO("RTDIAL: %s",__func__);

    if (err != CAST_ERROR_NONE){
        GDIAL_LOGINFO("RTDIAL: rtRemoteinit Failed");
        return false;
    }

    main_context_ = g_main_context_ref(context);
    //remoteSource = attachRtRemoteSource();

    //if (!remoteSource)
    //   GDIAL_LOGINFO("RTDIAL: Failed to attach rt remote source");

    objName =  getenv("PX_WAYLAND_CLIENT_REMOTE_OBJECT_NAME");
    if(!objName) objName = "com.comcast.xdialcast";

    DialObj = new DialCastObject("com.comcast.xdialcast");

    INIT_COMPLETED =1;
    return true;
}

void rtdial_term() {
    GDIAL_LOGINFO("RTDIAL: %s ",__FUNCTION__);

    //gdial_plat_dev_deinitialize();
    g_main_context_unref(main_context_);
    //g_source_unref(remoteSource);

    //DialObj->bye();
    delete (AppCache);
#if 0
    CastError e = rtRemoteShutdown();
    if (e != CAST_ERROR_NONE)
    {
      GDIAL_LOGINFO("RTDIAL: rtRemoteShutdown failed: %s ", rtStrError(e));
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
    GDIAL_LOGINFO("RTDIAL gdial_os_application_start : Application launch request: appName: %s  query: [%s], payload: [%s], additionalDataUrl [%s] instance[%p]",
        app_name, query_string, payload, additional_data_url,instance_id);

    if (strcmp(app_name,"system") == 0) {
        auto parsed_query{parse_query(query_string)};
        if (parsed_query["action"] == "sleep") {
            const char *system_key = getenv("SYSTEM_SLEEP_REQUEST_KEY");
            if (system_key && parsed_query["key"] != system_key) {
                GDIAL_LOGINFO("RTDIAL: system app request to change device to sleep mode, key comparison failed: user provided '%s'", parsed_query["key"].c_str());
                return GDIAL_APP_ERROR_INTERNAL;
            }
            GDIAL_LOGINFO("RTDIAL: system app request to change device to sleep mode");
            gdial_plat_dev_set_power_state_off();
            return GDIAL_APP_ERROR_NONE;
        }
        else if (parsed_query["action"] == "togglepower") {
            const char *system_key = getenv("SYSTEM_SLEEP_REQUEST_KEY");
            if (system_key && parsed_query["key"] != system_key) {
                GDIAL_LOGINFO("RTDIAL: system app request to toggle the power state, key comparison failed: user provided '%s'", parsed_query["key"].c_str());
                return GDIAL_APP_ERROR_INTERNAL;
            }
            GDIAL_LOGINFO("RTDIAL: system app request to toggle the power state ");
            gdial_plat_dev_toggle_power_state();
            return GDIAL_APP_ERROR_NONE;
        }
    }
    gdial_plat_dev_set_power_state_on();
    rtCastError ret = DialObj->launchApplicationWithLaunchParams(app_name, payload, query_string, additional_data_url);
    if (RTCAST_ERROR_CAST(ret) != CAST_ERROR_NONE) {
        GDIAL_LOGINFO("RTDIAL: DialObj.launchApplication failed!!! Error=%x",RTCAST_ERROR_CAST(ret));
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
           GDIAL_LOGINFO("Security token = %s ",query.c_str());
           controllerRemoteObject = new JSONRPC::LinkType<Core::JSON::IElement>(std::string(), false, query);
         }
     }
     std::string nfxstatus = "status@" + nfx_callsign;
     if(controllerRemoteObject->Get(1000, _T(nfxstatus), pluginResponse) == Core::ERROR_NONE)
     {
         GDIAL_LOGINFO("Obtained netflix status = %s",nfxstatus.c_str());
         Core::JSON::ArrayType<PluginHost::MetaData::Service>::Iterator index(pluginResponse.Elements());
         while (index.Next() == true) {
                netflixState = index.Current().JSONState.Data();
         } //end of while loop
     } //end of if case for querrying
     GDIAL_LOGINFO("Netflix State = %s",netflixState.c_str());
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
    GDIAL_LOGINFO("RTDIAL gdial_os_application_stop: appName = %s appID = %s",app_name,std::to_string(instance_id).c_str());
    if((strcmp(app_name,"system") == 0)){
        GDIAL_LOGINFO("RTDIAL delete not supported for system app return GDIAL_APP_ERROR_BAD_REQUEST");
        return GDIAL_APP_ERROR_BAD_REQUEST;
    }
    std::string State = AppCache->SearchAppStatusInCache(app_name);
    /* always to issue stop request to have a failsafe strategy */
    if (0 && State != "running")
        return GDIAL_APP_ERROR_BAD_REQUEST;

    char* enable_stop = getenv("ENABLE_NETFLIX_STOP");
    if ( enable_stop != NULL ) {
       if ( strcmp(app_name,"Netflix") == 0 && strcmp(enable_stop,"true") == 0) {
           GDIAL_LOGINFO("NTS TESTING: force shutdown Netflix thunder plugin");
           stop_netflix();
           sleep(1);
       }
    }
    rtCastError ret = DialObj->stopApplication(app_name,std::to_string(instance_id).c_str()); 
    if (RTCAST_ERROR_CAST(ret) != CAST_ERROR_NONE) {
        GDIAL_LOGINFO("RTDIAL: DialObj.stopApplication failed!!! Error=%x",RTCAST_ERROR_CAST(ret));
        return GDIAL_APP_ERROR_INTERNAL;
    }
    return GDIAL_APP_ERROR_NONE;
}

int gdial_os_application_hide(const char *app_name, int instance_id) {
    if((strcmp(app_name,"system") == 0)){
        GDIAL_LOGINFO("RTDIAL system app already in hidden state");
        return GDIAL_APP_ERROR_NONE;
    }
    #if 0
    GDIAL_LOGINFO("RTDIAL gdial_os_application_hide-->stop: appName = %s appID = %s",app_name,std::to_string(instance_id).c_str());
    std::string State = AppCache->SearchAppStatusInCache(app_name);
    if (0 && State != "running") {
        return GDIAL_APP_ERROR_BAD_REQUEST;
    }
    // Report Hide request not implemented for Youtube for ceritifcation requirement.
    if(strncmp("YouTube", app_name, 7) != 0) {
       rtCastError ret = DialObj->stopApplication(app_name,std::to_string(instance_id).c_str());
       if (RTCAST_ERROR_CAST(ret) != CAST_ERROR_NONE) {
           GDIAL_LOGINFO("RTDIAL: DialObj.stopApplication failed!!! Error=%x",RTCAST_ERROR_CAST(ret));
           return GDIAL_APP_ERROR_INTERNAL;
       }
       return GDIAL_APP_ERROR_NONE;
    }
    return GDIAL_APP_ERROR_NONE;
    #else
    GDIAL_LOGINFO("RTDIAL gdial_os_application_hide: appName = %s appID = %s",app_name,std::to_string(instance_id).c_str());
    std::string State = AppCache->SearchAppStatusInCache(app_name);
    if (State != "running")
        return GDIAL_APP_ERROR_BAD_REQUEST;
    rtCastError ret = DialObj->hideApplication(app_name,std::to_string(instance_id).c_str());
    if (RTCAST_ERROR_CAST(ret) != CAST_ERROR_NONE) {
        GDIAL_LOGINFO("RTDIAL: DialObj.hideApplication failed!!! Error=%x",RTCAST_ERROR_CAST(ret));
        return GDIAL_APP_ERROR_INTERNAL;
    }
    return GDIAL_APP_ERROR_NONE;
    #endif
}

int gdial_os_application_resume(const char *app_name, int instance_id) {
    GDIAL_LOGINFO("RTDIAL gdial_os_application_resume: appName = %s appID = %s",app_name,std::to_string(instance_id).c_str());

    if((strcmp(app_name,"system") == 0)){
        GDIAL_LOGINFO("RTDIAL system app can not be resume");
        return GDIAL_APP_ERROR_NONE;
    }
    std::string State = AppCache->SearchAppStatusInCache(app_name);
    if (State == "running")
        return GDIAL_APP_ERROR_BAD_REQUEST;
    rtCastError ret = DialObj->resumeApplication(app_name,std::to_string(instance_id).c_str());
    if (RTCAST_ERROR_CAST(ret) != CAST_ERROR_NONE) {
        GDIAL_LOGINFO("RTDIAL: DialObj.resumeApplication failed!!! Error=%x",RTCAST_ERROR_CAST(ret));
        return GDIAL_APP_ERROR_INTERNAL;
    }
    return GDIAL_APP_ERROR_NONE;
}

int gdial_os_application_state(const char *app_name, int instance_id, GDialAppState *state) {
    GDIAL_LOGINFO("RTDIAL gdial_os_application_state: App = %s Id = %d",app_name,instance_id);
    if((strcmp(app_name,"system") == 0)){
        *state = GDIAL_APP_STATE_HIDE;
        GDIAL_LOGINFO("RTDIAL getApplicationState: AppState = suspended ");
        return GDIAL_APP_ERROR_NONE;
    }
    std::string State = AppCache->SearchAppStatusInCache(app_name);
    GDIAL_LOGINFO("RTDIAL getApplicationState: AppState = %s ",State.c_str());
    /*
     *  return cache, but also trigger a refresh
     */
    if((strcmp(app_name,"system") != 0) &&( true || State == "NOT_FOUND")) {
        rtCastError ret = DialObj->getApplicationState(app_name,NULL);
        if (RTCAST_ERROR_CAST(ret) != CAST_ERROR_NONE) {
            GDIAL_LOGINFO("RTDIAL: DialObj.getApplicationState failed!!! Error: %x",RTCAST_ERROR_CAST(ret));
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
         GDIAL_LOGINFO("RTDIAL: Presence of Netflix thunder plugin state = %s to confirm state", app_state.c_str());
         if (app_state == "deactivated") {
           *state = GDIAL_APP_STATE_STOPPED;
           GDIAL_LOGINFO("RTDIAL: app [%s] state converted to [%d]", app_name, *state);
         }
         else if (app_state == "suspended")
         {
            *state = GDIAL_APP_STATE_HIDE;
            GDIAL_LOGINFO("RTDIAL: app [%s] state converted to [%d]", app_name, *state);
         }
	 else {
	        *state = GDIAL_APP_STATE_RUNNING;	 
         }		 
       }
    }
    GDIAL_LOGINFO("Exiting ...");
    return GDIAL_APP_ERROR_NONE;
}

int gdial_os_application_state_changed(const char *applicationName, const char *applicationId, const char *state, const char *error)
{
    GDIAL_LOGINFO("RTDIAL gdial_os_application_state_changed : appName: %s  appId: [%s], state: [%s], error [%s]",
        applicationName, applicationId, state, error);

    rtCastError ret = DialObj->applicationStateChanged(applicationName,applicationId,state,error);

    if (RTCAST_ERROR_CAST(ret) != CAST_ERROR_NONE) {
        GDIAL_LOGINFO("RTDIAL: DialObj.applicationStateChanged failed!!! Error=%x",RTCAST_ERROR_CAST(ret));
        return GDIAL_APP_ERROR_INTERNAL;
    }
    GDIAL_LOGINFO("Exiting ...");
    return GDIAL_APP_ERROR_NONE;
}

int gdial_os_application_activation_changed(const char *activation, const char *friendlyname)
{
    GDIAL_LOGINFO("RTDIAL gdial_os_application_activation_changed : activation: %s  friendlyname: [%s]",activation, friendlyname);

    rtCastError ret = DialObj->activationChanged(activation,friendlyname);

    if (RTCAST_ERROR_CAST(ret) != CAST_ERROR_NONE) {
        GDIAL_LOGINFO("RTDIAL: DialObj.friendlyNameChanged failed!!! Error=%x",RTCAST_ERROR_CAST(ret));
        return GDIAL_APP_ERROR_INTERNAL;
    }
    GDIAL_LOGINFO("Exiting ...");
    return GDIAL_APP_ERROR_NONE;
}

int gdial_os_application_friendlyname_changed(const char *friendlyname)
{
    GDIAL_LOGINFO("RTDIAL gdial_os_application_friendlyname_changed : friendlyname: [%s]",friendlyname);

    rtCastError ret = DialObj->friendlyNameChanged(friendlyname);

    if (RTCAST_ERROR_CAST(ret) != CAST_ERROR_NONE) {
        GDIAL_LOGINFO("RTDIAL: DialObj.friendlyNameChanged failed!!! Error=%x",RTCAST_ERROR_CAST(ret));
        return GDIAL_APP_ERROR_INTERNAL;
    }
    return GDIAL_APP_ERROR_NONE;
}

const char* gdial_os_application_get_protocol_version(void)
{
    GDIAL_LOGINFO("RTDIAL gdial_os_application_activation_changed");

    return (DialObj->getProtocolVersion());
}

int gdial_os_application_register_applications(void* appList)
{
    GDIAL_LOGINFO("RTDIAL gdial_os_application_register_applications :");

    rtCastError ret = DialObj->registerApplications(appList);

    if (RTCAST_ERROR_CAST(ret) != CAST_ERROR_NONE) {
        GDIAL_LOGINFO("RTDIAL: DialObj.registerApplications failed!!! Error=%x",RTCAST_ERROR_CAST(ret));
        return GDIAL_APP_ERROR_INTERNAL;
    }
    return GDIAL_APP_ERROR_NONE;
}

void gdial_os_application_update_network_standby_mode(gboolean nwstandbymode)
{
    GDIAL_LOGINFO("nwstandbymode:%u",nwstandbymode);

    DialObj->updateNetworkStandbyMode(nwstandbymode);
}

int gdial_os_application_service_notification(gboolean isNotifyRequired, void* notifier)
{
    GDIAL_LOGINFO("Entering isNotifyRequired : %u",isNotifyRequired);
    if (isNotifyRequired)
    {
        DialObj->setService(static_cast<GDialNotifier*>(notifier));
    }
    else
    {
        DialObj->setService(nullptr);
    }
    GDIAL_LOGINFO("Exiting ...");;
    //sleep(3);
    return GDIAL_APP_ERROR_NONE;
}
