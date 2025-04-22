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


#include <string>
#include <map>
#include <fstream>
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
#ifndef DISABLE_SECURITY_TOKEN
#include <securityagent/SecurityTokenUtil.h>
#endif
#include "gdial-app.h"
#include "gdial-plat-dev.h"
#include "gdial-os-app.h"
#include "gdial-config.h"
#include "gdialappcache.hpp"
#include "gdial.hpp"
#include "gdial_app_registry.h"
#include "gdialservicelogging.h"

static GMainContext *main_context_ = nullptr;
static int gdialInitCompleted = 0;
//cache
GDialAppStatusCache* AppCache = nullptr;
static gdial_activation_cb g_activation_cb = NULL;
static gdial_friendlyname_cb g_friendlyname_cb = NULL;
static gdial_registerapps_cb g_registerapps_cb = NULL;
static gdial_manufacturername_cb g_manufacturername_cb = NULL;
static gdial_modelname_cb g_modelname_cb = NULL;

#define DIAL_MAX_NUM_OF_APPS (64)
#define DIAL_MAX_NUM_OF_APP_NAMES (64)
#define DIAL_MAX_NUM_OF_APP_PREFIXES (64)
#define DIAL_MAX_NUM_OF_APP_CORS (64)

class GDialCastObject
{
public:
    GDialCastObject(){}
    ~GDialCastObject() {}

    GDialErrorCode applicationStateChanged(const char *applicationName, const char *applicationId, const char *state, const char *error)
    {
        GDIAL_LOGTRACE("Entering ...");
        GDialErrorCode reterror = GDIAL_CAST_ERROR_INTERNAL;
        AppInfo* AppObj = new AppInfo(applicationName,applicationId,state,error);
        if ((nullptr != AppObj) && (nullptr != AppCache))
        {
            GDIAL_LOGINFO("AppName[%s] AppID[%s] State[%s] Error[%s]",
                            AppObj->appName.c_str(),
                            AppObj->appId.c_str(),
                            AppObj->appState.c_str(),
                            AppObj->appError.c_str());
            if ( AppCacheError_OK == AppCache->UpdateAppStatusCache(AppObj))
            {
                reterror = GDIAL_CAST_ERROR_NONE;
            }
        }
        if (nullptr != AppObj)
        {
            delete AppObj;
            AppObj = nullptr;
        }
        GDIAL_LOGTRACE("Exiting ...");
        return reterror;
    }

    GDialErrorCode friendlyNameChanged(const char* friendlyname)
    {
        GDIAL_LOGTRACE("Entering ...");
        GDialErrorCode error = GDIAL_CAST_ERROR_INTERNAL;
        if( g_friendlyname_cb && friendlyname )
        {
            GDIAL_LOGINFO("GDialCastObject::friendlyNameChanged:[%s]",friendlyname);
            g_friendlyname_cb(friendlyname);
            error = GDIAL_CAST_ERROR_NONE;
        }
        GDIAL_LOGTRACE("Exiting ...");
        return error;
    }

    GDialErrorCode registerApplications(void* appList)
    {
        GDIAL_LOGTRACE("Entering ...");
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
            GDIAL_LOGINFO("Application:[%d]", i);
            gAppPrefxes = g_list_prepend (gAppPrefxes, g_strdup(appEntry->prefixes.c_str()));
            GDIAL_LOGINFO("%s, ", appEntry->prefixes.c_str());
            GDIAL_LOGINFO("");

            allowed_origins = g_list_prepend (allowed_origins, g_strdup(appEntry->cors.c_str()));
            GDIAL_LOGINFO("%s, ", appEntry->cors.c_str());
            GDIAL_LOGINFO("");

            GHashTable *gProperties = g_hash_table_new_full(g_str_hash, g_str_equal, g_free, g_free);
            std::string appAllowStop = appEntry->allowStop ? "true" : "false";
            g_hash_table_insert(gProperties,g_strdup("allowStop"),g_strdup(appAllowStop.c_str()));
            GDIAL_LOGINFO("allowStop:[%s]", appAllowStop.c_str());
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
            GDIAL_LOGINFO("calling register_applications callback ");
            g_registerapps_cb(gAppList);
        }
        /*Free the applist*/
        if (gAppList) {
            g_list_free (gAppList);
            gAppList = NULL;
        }
        GDIAL_LOGINFO("[%p] Freeing appConfigList",appConfigList);
        delete appConfigList;
        appConfigList = nullptr;
        GDIAL_LOGTRACE("Exiting ...");
        return GDIAL_CAST_ERROR_NONE;
    }

    const char* getProtocolVersion()
    {
        GDIAL_LOGINFO("GDIAL_PROTOCOL_VERSION_STR[%s]",GDIAL_PROTOCOL_VERSION_STR);
        return GDIAL_PROTOCOL_VERSION_STR;
    }

    GDialErrorCode activationChanged(std::string status, std::string friendlyName)
    {
        GDIAL_LOGTRACE("Entering ...");
        GDialErrorCode error = GDIAL_CAST_ERROR_INTERNAL;
        GDIAL_LOGINFO("status[%s] friendlyname[%s]",status.c_str(),friendlyName.c_str());
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
            GDIAL_LOGINFO("status[%s]  g_activation_cb[%p]",status.c_str(), g_activation_cb);
            error = GDIAL_CAST_ERROR_NONE;
        }
        GDIAL_LOGTRACE("Exiting ...");
        return error;
    }

    void updateNetworkStandbyMode(gboolean nwstandbyMode)
    {
        GDIAL_LOGTRACE("Entering ...");
        gdial_plat_dev_nwstandby_mode_change(nwstandbyMode);
        GDIAL_LOGTRACE("Exiting ...");
    }

    GDialErrorCode updateManufacturerName(const char* manufacturerName)
    {
        GDIAL_LOGTRACE("Entering ...");
        GDialErrorCode error = GDIAL_CAST_ERROR_INTERNAL;
        if( g_manufacturername_cb && manufacturerName )
        {
            GDIAL_LOGINFO("Manufacturer[%s]",manufacturerName);
            g_manufacturername_cb(manufacturerName);
            error = GDIAL_CAST_ERROR_NONE;
        }
        GDIAL_LOGTRACE("Exiting ...");
        return error;
    }

    GDialErrorCode updateModelName(const char* modelName)
    {
        GDIAL_LOGTRACE("Entering ...");
        GDialErrorCode error = GDIAL_CAST_ERROR_INTERNAL;
        if( g_modelname_cb && modelName )
        {
            GDIAL_LOGINFO("ModelName[%s]",modelName);
            g_modelname_cb(modelName);
            error = GDIAL_CAST_ERROR_NONE;
        }
        GDIAL_LOGTRACE("Exiting ...");
        return error;
    }
    
    GDialErrorCode launchApplication(const char* appName, const char* args)
    {
        GDIAL_LOGTRACE("Entering ...");
        std::string applicationName = "",
                    parameter = "";

        if (nullptr!=appName){
            applicationName = appName;
        }

        if (nullptr!=args){
            parameter = args;
        }

        GDIAL_LOGINFO("App[%s] param[%s] observer[%p]",applicationName.c_str(),parameter.c_str(),m_observer);
        if (nullptr!=m_observer)
        {
            m_observer->onApplicationLaunchRequest(applicationName,parameter);
        }
        GDIAL_LOGTRACE("Exiting ...");
        return GDIAL_CAST_ERROR_NONE;
    }

    GDialErrorCode launchApplicationWithLaunchParams(const char *appName, const char *argPayload, const char *argQueryString, const char *argAdditionalDataUrl)
    {
        std::string applicationName = "",
                        payLoad = "",
                        queryString = "",
                        additionalDataUrl = "";
        GDIAL_LOGTRACE("Entering ...");

        if (nullptr!=appName){
            applicationName = appName;
        }
        if (nullptr!=argPayload){
            payLoad = argPayload;
        }
        if (nullptr!=argQueryString){
            queryString = argQueryString;
        }
        if (nullptr!=argAdditionalDataUrl){
            additionalDataUrl = argAdditionalDataUrl;
        }

        GDIAL_LOGINFO("App[%s] payload[%s] query_string[%s] additional_data_url[%s]observer[%p]",
                applicationName.c_str(), 
                payLoad.c_str(), 
                queryString.c_str(),
                additionalDataUrl.c_str(),
                m_observer);
        if (nullptr!=m_observer)
        {
            m_observer->onApplicationLaunchRequestWithLaunchParam( applicationName,
                                                                   payLoad,
                                                                   queryString,
                                                                   additionalDataUrl );
        }
        GDIAL_LOGTRACE("Exiting ...");
        return GDIAL_CAST_ERROR_NONE;
    }

    GDialErrorCode hideApplication(const char* appName, const char* appID)
    {
        GDIAL_LOGTRACE("Entering ...");
        std::string applicationName = "",
                    applicationId = "";
        if (nullptr!=appName){
            applicationName = appName;
        }
        if (nullptr!=appID){
            applicationId = appID;
        }

        GDIAL_LOGINFO("App[%s]ID[%s]observer[%p]",applicationName.c_str(),applicationId.c_str(),m_observer);
        if (nullptr!=m_observer)
        {
            m_observer->onApplicationHideRequest(applicationName,applicationId);
        }
        GDIAL_LOGTRACE("Exiting ...");
        return GDIAL_CAST_ERROR_NONE;
    }

    GDialErrorCode resumeApplication(const char* appName, const char* appID)
    {
        GDIAL_LOGTRACE("Entering ...");
        std::string applicationName = "",
                    applicationId = "";

        if (nullptr!=appName){
            applicationName = appName;
        }
        if (nullptr!=appID){
            applicationId = appID;
        }

        GDIAL_LOGINFO("App[%s]ID[%s]observer[%p]",applicationName.c_str(),applicationId.c_str(),m_observer);
        if (nullptr!=m_observer)
        {
            m_observer->onApplicationResumeRequest(applicationName,applicationId);
        }
        GDIAL_LOGTRACE("Exiting ...");
        return GDIAL_CAST_ERROR_NONE;
    }

    GDialErrorCode stopApplication(const char* appName, const char* appID)
    {
        GDIAL_LOGTRACE("Entering ...");
        std::string applicationName = "",
                    applicationId = "";

        if (nullptr!=appName){
            applicationName = appName;
        }
        if (nullptr!=appID){
            applicationId = appID;
        }

        GDIAL_LOGINFO("App[%s]ID[%s]observer[%p]",applicationName.c_str(),applicationId.c_str(),m_observer);
        if (nullptr!=m_observer)
        {
            m_observer->onApplicationStopRequest(applicationName,applicationId);
        }
        GDIAL_LOGTRACE("Exiting ...");
        return GDIAL_CAST_ERROR_NONE;
    }

    GDialErrorCode getApplicationState(const char* appName, const char* appID)
    {
        GDIAL_LOGTRACE("Entering ...");
        std::string applicationName = "",
                    applicationId = "";

        if (nullptr!=appName){
            applicationName = appName;
        }
        if (nullptr!=appID){
            applicationId = appID;
        }

        GDIAL_LOGINFO("App[%s]ID[%s]observer[%p]",applicationName.c_str(),applicationId.c_str(),m_observer);
        if (nullptr!=m_observer)
        {
            m_observer->onApplicationStateRequest(applicationName,applicationId);
        }
        GDIAL_LOGTRACE("Exiting ...");
        return GDIAL_CAST_ERROR_NONE;
    }

    void setService(GDialNotifier* service)
    {
        m_observer = service;
    }
private:
    GDialNotifier *m_observer;
};

GDialCastObject* GDialObjHandle = nullptr;

void gdial_register_activation_cb(gdial_activation_cb cb)
{
  g_activation_cb = cb;
}

void gdial_register_friendlyname_cb(gdial_friendlyname_cb cb)
{
   g_friendlyname_cb = cb;
}

void gdial_register_registerapps_cb(gdial_registerapps_cb cb)
{
   g_registerapps_cb = cb;
}

void gdial_register_manufacturername_cb(gdial_manufacturername_cb cb)
{
    g_manufacturername_cb = cb;
}

void gdial_register_modelname_cb(gdial_modelname_cb cb)
{
    g_modelname_cb = cb;
}

bool gdial_init(GMainContext *context)
{
    bool returnValue = false;

    GDIAL_LOGTRACE("Entering ...");
    if(gdialInitCompleted)
    {
        GDIAL_LOGTRACE("Exiting ...");
        return true;
    }
    else
    {
        AppCache = new GDialAppStatusCache();
        if (nullptr == AppCache)
        {
            GDIAL_LOGERROR("GDialAppStatusCache Failed");
        }
        else
        {
            GDialObjHandle = new GDialCastObject();
            if (nullptr == GDialObjHandle)
            {
                GDIAL_LOGERROR("GDialCastObject Failed");
                delete AppCache;
                AppCache = nullptr;
            }
            else
            {
                main_context_ = g_main_context_ref(context);
                gdialInitCompleted = 1;
                returnValue = true;
            }
        }
    }
    GDIAL_LOGTRACE("Exiting ...");
    return returnValue;
}

void gdial_term()
{
    GDIAL_LOGTRACE("Entering ...");
    if (GDialObjHandle)
    {
        delete GDialObjHandle;
        GDialObjHandle = nullptr;
    }

    if (main_context_)
    {
        g_main_context_unref(main_context_);
        main_context_ = NULL;
    }

    if (AppCache)
    {
        delete AppCache;
        AppCache = nullptr;
    }
    gdialInitCompleted = 0;
    GDIAL_LOGTRACE("Exiting ...");
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
    GDIAL_LOGTRACE("Entering ...");
    GDIAL_LOGINFO("App launch req: appName[%s]  query[%s], payload[%s], additionalDataUrl [%s] instance[%p]",
        app_name, query_string, payload, additional_data_url,instance_id);

    if (strcmp(app_name,"system") == 0) {
        auto parsed_query{parse_query(query_string)};
        if (parsed_query["action"] == "sleep") {
            const char *system_key = getenv("SYSTEM_SLEEP_REQUEST_KEY");
            if (system_key && parsed_query["key"] != system_key) {
                GDIAL_LOGINFO("system app request to change device to sleep mode, key comparison failed: user provided '%s'", parsed_query["key"].c_str());
                GDIAL_LOGTRACE("Exiting ...");
                return GDIAL_APP_ERROR_INTERNAL;
            }
            GDIAL_LOGINFO("system app request to change device to sleep mode");
            gdial_plat_dev_set_power_state_off();
            GDIAL_LOGTRACE("Exiting ...");
            return GDIAL_APP_ERROR_NONE;
        }
        else if (parsed_query["action"] == "togglepower") {
            const char *system_key = getenv("SYSTEM_SLEEP_REQUEST_KEY");
            if (system_key && parsed_query["key"] != system_key) {
                GDIAL_LOGINFO("system app request to toggle the power state, key comparison failed: user provided '%s'", parsed_query["key"].c_str());
                GDIAL_LOGTRACE("Exiting ...");
                return GDIAL_APP_ERROR_INTERNAL;
            }
            GDIAL_LOGINFO("system app request to toggle the power state ");
            gdial_plat_dev_toggle_power_state();
            GDIAL_LOGTRACE("Exiting ...");
            return GDIAL_APP_ERROR_NONE;
        }
    }
    if (nullptr == GDialObjHandle)
    {
        GDIAL_LOGERROR("GDialObjHandle NULL!!! ");
        GDIAL_LOGTRACE("Exiting ...");
        return GDIAL_APP_ERROR_INTERNAL;
    }
    GDialErrorCode ret = GDialObjHandle->launchApplicationWithLaunchParams(app_name, payload, query_string, additional_data_url);
    if (ret != GDIAL_CAST_ERROR_NONE) {
        GDIAL_LOGERROR("GDialObjHandle.launchApplication failed!!! Error=%x",ret);
        GDIAL_LOGTRACE("Exiting ...");
        return GDIAL_APP_ERROR_INTERNAL;
    }
    GDIAL_LOGTRACE("Exiting ...");
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
     string sToken = "";
     string query = "";
#ifdef DISABLE_SECURITY_TOKEN
    query = "token=" + sToken;
#else
     unsigned char buffer[MAX_LENGTH] = {0};

     //Obtaining controller object
     if (NULL == controllerRemoteObject) {
         int ret = GetSecurityToken(MAX_LENGTH,buffer);
         if(ret>0)
         {
           sToken = (char*)buffer;
           query = "token=" + sToken;
           GDIAL_LOGINFO("Security token[%s] ",query.c_str());
         }
     }
#endif
     controllerRemoteObject = new JSONRPC::LinkType<Core::JSON::IElement>(std::string(), false, query);

     std::string nfxstatus = "status@" + nfx_callsign;
     if(controllerRemoteObject->Get(1000, _T(nfxstatus), pluginResponse) == Core::ERROR_NONE)
     {
         GDIAL_LOGINFO("Obtained netflix status[%s]",nfxstatus.c_str());
         Core::JSON::ArrayType<PluginHost::MetaData::Service>::Iterator index(pluginResponse.Elements());
         while (index.Next() == true) {
                netflixState = index.Current().JSONState.Data();
         } //end of while loop
     } //end of if case for querrying
     GDIAL_LOGINFO("Netflix State[%s]",netflixState.c_str());
     return netflixState;
}
void stop_netflix()
{
   JsonObject parameters;
   JsonObject response;
   parameters["callsign"] = nfx_callsign;
   GDIAL_LOGTRACE("Entering ...");
   if (Core::ERROR_NONE == controllerRemoteObject->Invoke("deactivate", parameters, response)) {
        std::cout << "Netflix is stoppped" << std::endl;
   } else {
        std::cout << "Netflix could not be deactivated" << std::endl;
   }
   GDIAL_LOGTRACE("Exiting ...");
}

int gdial_os_application_stop(const char *app_name, int instance_id) {
    bool enable_stop = false;
    GDIAL_LOGTRACE("Entering ...");
    GDIAL_LOGINFO("AppName[%s] appID[%s]",app_name,std::to_string(instance_id).c_str());
    if((strcmp(app_name,"system") == 0)){
        GDIAL_LOGINFO("delete not supported for system app return GDIAL_APP_ERROR_BAD_REQUEST");
        return GDIAL_APP_ERROR_BAD_REQUEST;
    }
    std::string State = AppCache->SearchAppStatusInCache(app_name);
    /* always to issue stop request to have a failsafe strategy */
    if (0 && State != "running")
    {
        GDIAL_LOGTRACE("Exiting ...");
        return GDIAL_APP_ERROR_BAD_REQUEST;
    }

    std::ifstream netflixStopFile("/opt/enableXdialNetflixStop");
    if (netflixStopFile)
    {
        netflixStopFile.close();
        enable_stop = true;
    }

    if ( strcmp(app_name,"Netflix") == 0 && enable_stop ) {
        GDIAL_LOGINFO("NTS TESTING: force shutdown Netflix thunder plugin");
        stop_netflix();
        sleep(1);
    }
    if (nullptr == GDialObjHandle)
    {
        GDIAL_LOGERROR("GDialObjHandle NULL!!! ");
        GDIAL_LOGTRACE("Exiting ...");
        return GDIAL_APP_ERROR_INTERNAL;
    }
    GDialErrorCode ret = GDialObjHandle->stopApplication(app_name,std::to_string(instance_id).c_str()); 
    if (ret != GDIAL_CAST_ERROR_NONE) {
        GDIAL_LOGINFO("GDialObjHandle.stopApplication failed!!! Error=%x",ret);
        GDIAL_LOGTRACE("Exiting ...");
        return GDIAL_APP_ERROR_INTERNAL;
    }
    GDIAL_LOGTRACE("Exiting ...");
    return GDIAL_APP_ERROR_NONE;
}

int gdial_os_application_hide(const char *app_name, int instance_id)
{
    GDIAL_LOGTRACE("Entering ...");
    if((strcmp(app_name,"system") == 0)){
        GDIAL_LOGINFO("system app already in hidden state");
        GDIAL_LOGTRACE("Exiting ...");
        return GDIAL_APP_ERROR_NONE;
    }
    #if 0
    GDIAL_LOGINFO("gdial_os_application_hide-->stop: appName[%s] appID[%s]",app_name,std::to_string(instance_id).c_str());
    std::string State = AppCache->SearchAppStatusInCache(app_name);
    if (0 && State != "running") {
        return GDIAL_APP_ERROR_BAD_REQUEST;
    }
    // Report Hide request not implemented for Youtube for ceritifcation requirement.
    if(strncmp("YouTube", app_name, 7) != 0) {
       GDialErrorCode ret = GDialObjHandle->stopApplication(app_name,std::to_string(instance_id).c_str());
       if (ret != GDIAL_CAST_ERROR_NONE) {
           GDIAL_LOGINFO("GDialObjHandle.stopApplication failed!!! Error=%x",ret);
           return GDIAL_APP_ERROR_INTERNAL;
       }
       return GDIAL_APP_ERROR_NONE;
    }
    return GDIAL_APP_ERROR_NONE;
    #else
    GDIAL_LOGINFO("gdial_os_application_hide: appName[%s] appID[%s]",app_name,std::to_string(instance_id).c_str());
    std::string State = AppCache->SearchAppStatusInCache(app_name);
    if (State != "running")
    {
        GDIAL_LOGTRACE("Exiting ...");
        return GDIAL_APP_ERROR_BAD_REQUEST;
    }
    
    if (nullptr == GDialObjHandle)
    {
        GDIAL_LOGERROR("GDialObjHandle NULL!!! ");
        GDIAL_LOGTRACE("Exiting ...");
        return GDIAL_APP_ERROR_INTERNAL;
    }

    GDialErrorCode ret = GDialObjHandle->hideApplication(app_name,std::to_string(instance_id).c_str());
    if (ret != GDIAL_CAST_ERROR_NONE) {
        GDIAL_LOGINFO("GDialObjHandle.hideApplication failed!!! Error=%x",ret);
        GDIAL_LOGTRACE("Exiting ...");
        return GDIAL_APP_ERROR_INTERNAL;
    }
    GDIAL_LOGTRACE("Exiting ...");
    return GDIAL_APP_ERROR_NONE;
    #endif
}

int gdial_os_application_resume(const char *app_name, int instance_id) {
    GDIAL_LOGTRACE("Entering ...");
    GDIAL_LOGINFO("appName[%s] appID[%s]",app_name,std::to_string(instance_id).c_str());

    if((strcmp(app_name,"system") == 0)){
        GDIAL_LOGINFO("system app can not be resume");
        GDIAL_LOGTRACE("Exiting ...");
        return GDIAL_APP_ERROR_NONE;
    }
    std::string State = AppCache->SearchAppStatusInCache(app_name);
    if (State == "running")
    {
        GDIAL_LOGTRACE("Exiting ...");
        return GDIAL_APP_ERROR_BAD_REQUEST;
    }

    if (nullptr == GDialObjHandle)
    {
        GDIAL_LOGERROR("GDialObjHandle NULL!!! ");
        GDIAL_LOGTRACE("Exiting ...");
        return GDIAL_APP_ERROR_INTERNAL;
    }

    GDialErrorCode ret = GDialObjHandle->resumeApplication(app_name,std::to_string(instance_id).c_str());
    if (ret != GDIAL_CAST_ERROR_NONE) {
        GDIAL_LOGINFO("GDialObjHandle.resumeApplication failed!!! Error=%x",ret);
        GDIAL_LOGTRACE("Exiting ...");
        return GDIAL_APP_ERROR_INTERNAL;
    }
    GDIAL_LOGTRACE("Exiting ...");
    return GDIAL_APP_ERROR_NONE;
}

int gdial_os_application_state(const char *app_name, int instance_id, GDialAppState *state) {
    GDIAL_LOGTRACE("Entering ...");
    GDIAL_LOGINFO("App[%s] Id[%d]",app_name,instance_id);
    if((strcmp(app_name,"system") == 0)){
        *state = GDIAL_APP_STATE_HIDE;
        GDIAL_LOGINFO("getApplicationState: AppState = suspended ");
        GDIAL_LOGTRACE("Exiting ...");
        return GDIAL_APP_ERROR_NONE;
    }
    std::string State = AppCache->SearchAppStatusInCache(app_name);
    GDIAL_LOGINFO("getApplicationState: AppState[%s] ",State.c_str());
    /*
     *  return cache, but also trigger a refresh
     */
    if((strcmp(app_name,"system") != 0) &&( true || State == "NOT_FOUND")) {
        if (nullptr == GDialObjHandle)
        {
            GDIAL_LOGERROR("GDialObjHandle NULL!!! ");
            GDIAL_LOGTRACE("Exiting ...");
            return GDIAL_APP_ERROR_INTERNAL;
        }

        GDialErrorCode ret = GDialObjHandle->getApplicationState(app_name,NULL);
        if (ret != GDIAL_CAST_ERROR_NONE) {
            GDIAL_LOGINFO("GDialObjHandle.getApplicationState failed!!! Error: %x",ret);
            GDIAL_LOGTRACE("Exiting ...");
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
         GDIAL_LOGINFO("Presence of Netflix thunder plugin state[%s] to confirm state", app_state.c_str());
         if (app_state == "deactivated") {
           *state = GDIAL_APP_STATE_STOPPED;
           GDIAL_LOGINFO("app [%s] state converted to [%d]", app_name, *state);
         }
         else if (app_state == "suspended")
         {
            *state = GDIAL_APP_STATE_HIDE;
            GDIAL_LOGINFO("app [%s] state converted to [%d]", app_name, *state);
         }
	 else {
	        *state = GDIAL_APP_STATE_RUNNING;	 
         }		 
       }
    }
    GDIAL_LOGTRACE("Exiting ...");
    return GDIAL_APP_ERROR_NONE;
}

int gdial_os_application_state_changed(const char *applicationName, const char *applicationId, const char *state, const char *error)
{
    GDIAL_LOGTRACE("Entering ...");
    GDIAL_LOGINFO("appName[%s]  appId[%s], state[%s], error [%s]",applicationName, applicationId, state, error);

    if (nullptr == GDialObjHandle)
    {
        GDIAL_LOGERROR("GDialObjHandle NULL!!! ");
        GDIAL_LOGTRACE("Exiting ...");
        return GDIAL_APP_ERROR_INTERNAL;
    }

    GDialErrorCode ret = GDialObjHandle->applicationStateChanged(applicationName,applicationId,state,error);

    if (ret != GDIAL_CAST_ERROR_NONE) {
        GDIAL_LOGINFO("GDialObjHandle.applicationStateChanged failed!!! Error=%x",ret);
        GDIAL_LOGTRACE("Exiting ...");
        return GDIAL_APP_ERROR_INTERNAL;
    }
    GDIAL_LOGTRACE("Exiting ...");
    return GDIAL_APP_ERROR_NONE;
}

int gdial_os_application_activation_changed(const char *activation, const char *friendlyname)
{
    GDIAL_LOGTRACE("Entering ...");
    GDIAL_LOGINFO("activation[%s]  friendlyname[%s]",activation, friendlyname);

    if (nullptr == GDialObjHandle)
    {
        GDIAL_LOGERROR("GDialObjHandle NULL!!! ");
        GDIAL_LOGTRACE("Exiting ...");
        return GDIAL_APP_ERROR_INTERNAL;
    }

    GDialErrorCode ret = GDialObjHandle->activationChanged(activation,friendlyname);

    if (ret != GDIAL_CAST_ERROR_NONE) {
        GDIAL_LOGINFO("GDialObjHandle.friendlyNameChanged failed!!! Error=%x",ret);
        GDIAL_LOGTRACE("Exiting ...");
        return GDIAL_APP_ERROR_INTERNAL;
    }
    GDIAL_LOGTRACE("Exiting ...");
    return GDIAL_APP_ERROR_NONE;
}

int gdial_os_application_friendlyname_changed(const char *friendlyname)
{
    GDIAL_LOGTRACE("Entering ...");
    GDIAL_LOGINFO("friendlyname[%s]",friendlyname);

    if (nullptr == GDialObjHandle)
    {
        GDIAL_LOGERROR("GDialObjHandle NULL!!! ");
        GDIAL_LOGTRACE("Exiting ...");
        return GDIAL_APP_ERROR_INTERNAL;
    }

    GDialErrorCode ret = GDialObjHandle->friendlyNameChanged(friendlyname);

    if (ret != GDIAL_CAST_ERROR_NONE) {
        GDIAL_LOGINFO("GDialObjHandle.friendlyNameChanged failed!!! Error=%x",ret);
        GDIAL_LOGTRACE("Exiting ...");
        return GDIAL_APP_ERROR_INTERNAL;
    }
    GDIAL_LOGTRACE("Exiting ...");
    return GDIAL_APP_ERROR_NONE;
}

const char* gdial_os_application_get_protocol_version(void)
{
    GDIAL_LOGTRACE("Entering ...");

    if (nullptr == GDialObjHandle)
    {
        GDIAL_LOGERROR("GDialObjHandle NULL!!! ");
        GDIAL_LOGTRACE("Exiting ...");
        return GDIAL_PROTOCOL_VERSION_STR;
    }
    GDIAL_LOGTRACE("Exiting ...");
    return (GDialObjHandle->getProtocolVersion());
}

int gdial_os_application_register_applications(void* appList)
{
    GDIAL_LOGTRACE("Entering ...");

    if (nullptr == GDialObjHandle)
    {
        GDIAL_LOGERROR("GDialObjHandle NULL!!! ");
        GDIAL_LOGTRACE("Exiting ...");
        return GDIAL_APP_ERROR_INTERNAL;
    }

    GDialErrorCode ret = GDialObjHandle->registerApplications(appList);

    if (ret != GDIAL_CAST_ERROR_NONE) {
        GDIAL_LOGINFO("GDialObjHandle.registerApplications failed!!! Error=%x",ret);
        GDIAL_LOGTRACE("Exiting ...");
        return GDIAL_APP_ERROR_INTERNAL;
    }
    GDIAL_LOGTRACE("Exiting ...");
    return GDIAL_APP_ERROR_NONE;
}

void gdial_os_application_update_network_standby_mode(gboolean nwstandbymode)
{
    GDIAL_LOGTRACE("Entering ...");
    GDIAL_LOGINFO("nwstandbymode:%u",nwstandbymode);
    if (nullptr == GDialObjHandle)
    {
        GDIAL_LOGERROR("GDialObjHandle NULL!!! ");
    }
    else
    {
        GDialObjHandle->updateNetworkStandbyMode(nwstandbymode);
    }
    GDIAL_LOGTRACE("Exiting ...");
}

int gdial_os_application_update_manufacturer_name(const char *manufacturer)
{
    GDialErrorCode returnValue = GDIAL_CAST_ERROR_INTERNAL;
    GDIAL_LOGTRACE("Entering ...");
    if ((nullptr == GDialObjHandle)||(nullptr == manufacturer))
    {
        GDIAL_LOGERROR("NULL GDialObjHandle[%p] manufacturer[%p]",GDialObjHandle,manufacturer);
    }
    else
    {
        GDIAL_LOGINFO("Manufacturer[%s]", manufacturer);
        returnValue = GDialObjHandle->updateManufacturerName(manufacturer);
        if (returnValue != GDIAL_CAST_ERROR_NONE) {
            GDIAL_LOGERROR("Failed to updateManufacturerName. Error=%x",returnValue);
        }
    }

    GDIAL_LOGTRACE("Exiting ...");
    return returnValue;
}

int gdial_os_application_update_model_name(const char *model)
{
    GDialErrorCode returnValue = GDIAL_CAST_ERROR_INTERNAL;
    GDIAL_LOGTRACE("Entering ...");
    if ((nullptr == GDialObjHandle)||(nullptr == model))
    {
        GDIAL_LOGERROR("NULL GDialObjHandle[%p] manufacturer[%p]",GDialObjHandle,model);
    }
    else
    {
        GDIAL_LOGINFO("Model[%s]", model);
        returnValue = GDialObjHandle->updateModelName(model);
        if (returnValue != GDIAL_CAST_ERROR_NONE) {
            GDIAL_LOGERROR("Failed to updateModelName. Error=%x",returnValue);
        }
    }
    GDIAL_LOGTRACE("Exiting ...");
    return returnValue;
}

int gdial_os_application_service_notification(gboolean isNotifyRequired, void* notifier)
{
    GDIAL_LOGTRACE("Entering ...");
    if (nullptr == GDialObjHandle)
    {
        GDIAL_LOGERROR("GDialObjHandle NULL!!!");
        GDIAL_LOGTRACE("Exiting ...");
        return GDIAL_APP_ERROR_INTERNAL;
    }

    GDIAL_LOGINFO("isNotifyRequired[%u]",isNotifyRequired);
    if (isNotifyRequired)
    {
        GDialObjHandle->setService(static_cast<GDialNotifier*>(notifier));
    }
    else
    {
        GDialObjHandle->setService(nullptr);
    }
    GDIAL_LOGTRACE("Exiting ...");
    return GDIAL_APP_ERROR_NONE;
}