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
#include "rtcast.hpp"
#include "rtcache.hpp"
#include "rtdial.hpp"
#include "gdial_app_registry.h"

rtRemoteEnvironment* env;
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

class rtDialCastRemoteObject : public rtCastRemoteObject
{

public:

    rtDialCastRemoteObject(rtString SERVICE_NAME): rtCastRemoteObject(SERVICE_NAME) {printf("rtDialCastRemoteObject() const %s\n",SERVICE_NAME.cString());}

    ~rtDialCastRemoteObject() {}

    rtError applicationStateChanged(const rtObjectRef& params) {
        printf("RTDIAL: rtDialCastRemoteObject::applicationStateChanged \n");
        rtObjectRef AppObj = new rtMapObject;
        AppObj = params;
        rtString app, id, state, error;
        AppObj.get("applicationName",app);
        AppObj.get("applicationId",id);
        AppObj.get("state",state);
        AppObj.get("error",error);
        printf("applicationStateChanged AppName : %s AppID : %s State : %s Error : %s\n",app.cString(),id.cString(),state.cString(),error.cString());
        AppCache->UpdateAppStatusCache(rtValue(AppObj));
        return RT_OK;
    }

    rtError friendlyNameChanged(const rtObjectRef& params) {
        rtObjectRef AppObj = new rtMapObject;
        AppObj = params;
        rtString friendlyName;
        AppObj.get("friendlyname",friendlyName);
        printf("RTDIAL: rtDialCastRemoteObject::friendlyNameChanged :%s \n",friendlyName.cString());
        if( g_friendlyname_cb )
        {
            g_friendlyname_cb(friendlyName.cString());
        }
        return RT_OK;
    }

    rtError registerApplications(const rtObjectRef& params) {
        printf("RTDIAL : rtDialCastRemoteObject::registerApplications\n");
        rtString appFirstName;

        rtObjectRef AppObj = params;
        GList *gAppList = NULL;
        for(int i = 0 ; i< (DIAL_MAX_NUM_OF_APPS); i ++) {
            rtObjectRef appInfo;
            int err = AppObj.get(i,appInfo);
            if(err == RT_OK) {
                printf("Application: %d \n", i);

                rtObjectRef appPrefxes;
                GList *gAppPrefxes = NULL;
                err = appInfo.get("prefixes",appPrefxes);
                if(err == RT_OK) {
                    for(int i = 0 ; i< (DIAL_MAX_NUM_OF_APP_PREFIXES); i ++) {
                        rtString appPrefx;
                        err = appPrefxes.get(i, appPrefx);
                        if(err == RT_OK) {
                            gAppPrefxes = g_list_prepend (gAppPrefxes, g_strdup(appPrefx.cString()));
                            printf("%s, ", appPrefx.cString());
                        }
                        else {
                            break;
                        }
                    }
                    printf("\n");
                }

                rtObjectRef appCors;
                GList *allowed_origins = NULL;
                err = appInfo.get("cors", appCors);
                if(err == RT_OK) {
                    for(int i = 0 ; i< (DIAL_MAX_NUM_OF_APP_CORS); i ++) {
                       rtString appCor;
                       err = appCors.get(i,appCor);
                       if(err == RT_OK) {
                           allowed_origins = g_list_prepend (allowed_origins, g_strdup(appCor.cString()));
                           printf("%s, ", appCor.cString());
                       }
                       else {
                           break;
                       }
                   }
                   printf("\n");
               }

               rtObjectRef appProp;
               GHashTable *gProperties = g_hash_table_new_full(g_str_hash, g_str_equal, g_free, g_free);
               err = appInfo.get("properties", appProp);
               if (RT_OK == err) {
                   rtString appAllowStop;
                   err = appProp.get("allowStop", appAllowStop);
                   if (RT_OK == err) {
                        g_hash_table_insert(gProperties,g_strdup("allowStop"),g_strdup(appAllowStop.cString()));
                        printf("allowStop: %s", appAllowStop.cString());
                   }
                   printf("\n");
               }

               rtObjectRef appNames;
               err = appInfo.get("Names",appNames);
               if(err == RT_OK) {
                  for(int i = 0 ; i< (DIAL_MAX_NUM_OF_APP_NAMES); i ++) {
                      rtString appName;
                      err = appNames.get(i,appName);
                      if(err == RT_OK) {
                          GDialAppRegistry*  app_registry = gdial_app_registry_new (
                                                                g_strdup(appName.cString()),
                                                                gAppPrefxes,
                                                                gProperties,
                                                                TRUE,
                                                                TRUE,
                                                                allowed_origins);
                          gAppList = g_list_prepend (gAppList, app_registry);
                          printf("%s, ", appName.cString());
                      }
                      else {
                          break;
                      }
                  }
                  printf("\n");
              }

           }
       }
       int appListSize = g_list_length (gAppList);
       if( g_registerapps_cb && appListSize) {
           printf("RTDIAL: rtDialCastRemoteObject:: calling register_applications callback \n");
           g_registerapps_cb(gAppList);
       }

       /*Free the applist*/
       if (gAppList) {
           g_list_free (gAppList);
           gAppList = NULL;
       }
       return RT_OK;
    }

    rtError activationChanged(const rtObjectRef& params) {
        rtObjectRef AppObj = new rtMapObject;
        AppObj = params;
        rtString status;
        rtString friendlyName;
        AppObj.get("activation",status);
        AppObj.get("friendlyname",friendlyName);
        printf("RTDIAL: rtDialCastRemoteObject::activationChanged status: %s friendlyname: %s \n",status.cString(),friendlyName.cString());
        if( g_activation_cb )
        {
            if(!strcmp(status.cString(), "true"))
            {
                g_activation_cb(1,friendlyName.cString());
            }
            else
            {
                g_activation_cb(0,friendlyName.cString());
            }
            printf("RTDIAL: rtDialCastRemoteObject:: status: %s  g_activation_cb :%d \n",status.cString(), g_activation_cb);
        }
        return RT_OK;
    }

    rtCastError launchApplication(const char* appName, const char* args) {
        printf("RTDIAL: rtDialCastRemoteObject::launchApplication App:%s  args:%s\n",appName,args);
        rtObjectRef AppObj = new rtMapObject;
        AppObj.set("applicationName",appName);
        AppObj.set("parameters",args);
        AppObj.set("isUrl","true");

        rtCastError error(RT_OK,CAST_ERROR_NONE);
        RTCAST_ERROR_RT(error) = notify("onApplicationLaunchRequest",AppObj);
        return error;
    }

    rtCastError launchApplicationWithLaunchParams(const char *appName, const char *argPayload, const char *argQueryString, const char *argAdditionalDataUrl) {
        printf("RTDIAL: rtDialCastRemoteObject::launchApplicationWithLaunchParams App:%s  payload:%s query_string:%s additional_data_url\n",
                                                                        appName, argPayload, argQueryString, argAdditionalDataUrl);
        rtObjectRef AppObj = new rtMapObject;
        AppObj.set("applicationName",appName);
        AppObj.set("payload",argPayload);
        AppObj.set("query",argQueryString);
        AppObj.set("addDataUrl",argAdditionalDataUrl);
        AppObj.set("isUrl","false");

        rtCastError error(RT_OK,CAST_ERROR_NONE);
        RTCAST_ERROR_RT(error) = notify("onApplicationLaunchRequest",AppObj);
        return error;
    }

    rtCastError hideApplication(const char* appName, const char* appID) {
        printf("RTDIAL: rtDialCastRemoteObject::hideApplication App:%s  ID:%s\n",appName,appID);
        rtObjectRef AppObj = new rtMapObject;
        AppObj.set("applicationName",appName);
        if(appID != NULL)
            AppObj.set("applicationId",appID);

        rtCastError error(RT_OK,CAST_ERROR_NONE);
        RTCAST_ERROR_RT(error) = notify("onApplicationHideRequest",AppObj);
        return error;
    }

    rtCastError resumeApplication(const char* appName, const char* appID) {
        printf("RTDIAL: rtDialCastRemoteObject::resumeApplication App:%s  ID:%s\n",appName,appID);
        rtObjectRef AppObj = new rtMapObject;
        AppObj.set("applicationName",appName);
        if(appID != NULL)
            AppObj.set("applicationId",appID);

        rtCastError error(RT_OK,CAST_ERROR_NONE);
        RTCAST_ERROR_RT(error) = notify("onApplicationResumeRequest",AppObj);
        return error;
    }

    rtCastError stopApplication(const char* appName, const char* appID) {
        printf("RTDIAL: rtDialCastRemoteObject::stopApplication App:%s  ID:%s\n",appName,appID);
        rtObjectRef AppObj = new rtMapObject;
        AppObj.set("applicationName",appName);
        if(appID != NULL)
            AppObj.set("applicationId",appID);

        rtCastError error(RT_OK,CAST_ERROR_NONE);
        RTCAST_ERROR_RT(error) = notify("onApplicationStopRequest",AppObj);
        return error;
    }

    rtCastError getApplicationState(const char* appName, const char* appID) {
        printf("RTDIAL: rtDialCastRemoteObject::getApplicationState App:%s  ID:%s\n",appName,appID);
        rtObjectRef AppObj = new rtMapObject;
        AppObj.set("applicationName",appName);
        if(appID != NULL)
            AppObj.set("applicationId",appID);

        rtCastError error(RT_OK,CAST_ERROR_NONE);
        RTCAST_ERROR_RT(error) = notify("onApplicationStateRequest",AppObj);
        return error;
    }


private:

};

rtDefineObject(rtCastRemoteObject, rtAbstractService);
rtDefineMethod(rtCastRemoteObject, applicationStateChanged);
rtDefineMethod(rtCastRemoteObject, activationChanged);
rtDefineMethod(rtCastRemoteObject, friendlyNameChanged);
rtDefineMethod(rtCastRemoteObject, registerApplications);

rtDialCastRemoteObject* DialObj;

static gboolean pumpRemoteObjectQueue(gpointer data)
{
//    printf("### %s  :  %s  :  %d   ### \n",__FILE__,__func__,__LINE__);
    rtError err;
    GSource *source = (GSource *)data;
    do {
        g_source_set_ready_time(source, -1);
        err = rtRemoteProcessSingleItem();
    } while (err == RT_OK);
    if (err != RT_OK && err != RT_ERROR_QUEUE_EMPTY) {
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

    rtError e = rtRemoteRegisterQueueReadyHandler(env, [](void *data) -> void {
        GSource *source = (GSource *)data;
        g_source_set_ready_time(source, 0);
    }, source);

    if (e != RT_OK)
    {
        printf("RTDIAL: Failed to register queue handler: %d", e);
        g_source_destroy(source);
        return nullptr;
    }
    g_source_attach(source, main_context_);
    return source;
}

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
    rtError err;
    const char* objName;

    gdial_plat_dev_initialize();
    env = rtEnvironmentGetGlobal();
    err = rtRemoteInit(env);

//cache
    AppCache = new rtAppStatusCache(env);

    printf("RTDIAL: %s\n",__func__);

    if (err != RT_OK){
        printf("RTDIAL: rtRemoteinit Failed\n");
        return false;
    }

    main_context_ = g_main_context_ref(context);
    remoteSource = attachRtRemoteSource();

    if (!remoteSource)
       printf("RTDIAL: Failed to attach rt remote source");

    objName =  getenv("PX_WAYLAND_CLIENT_REMOTE_OBJECT_NAME");
    if(!objName) objName = "com.comcast.xdialcast";

    DialObj = new rtDialCastRemoteObject("com.comcast.xdialcast");
    err = rtRemoteRegisterObject(env, objName, DialObj);
    if (err != RT_OK){
        printf("RTDIAL: rtRemoteRegisterObject for %s failed! error:%s !\n", objName, rtStrError(err));
        return false;
    }

    INIT_COMPLETED =1;
    return true;
}

void rtdial_term() {
    printf("RTDIAL: %s \n",__FUNCTION__);

    gdial_plat_dev_deinitialize();
    g_main_context_unref(main_context_);
    g_source_unref(remoteSource);

    DialObj->bye();
    delete (AppCache);
    rtError e = rtRemoteShutdown();
    if (e != RT_OK)
    {
      printf("RTDIAL: rtRemoteShutdown failed: %s \n", rtStrError(e));
    }
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


int gdial_os_application_start(const char *app_name, const char *payload, const char *query_string, const char *additional_data_url, int *instance_id) {
    printf("RTDIAL gdial_os_application_start : Application launch request: appName: %s  query: [%s], payload: [%s], additionalDataUrl [%s]\n",
        app_name, query_string, payload, additional_data_url);
    if (strcmp(app_name,"system") == 0 && strcmp(query_string,"action=sleep") == 0 ) {
        if(strcmp(query_string,"action=sleep") == 0){
            printf("RTDIAL: system app request to change device to sleep mode");
            gdial_plat_dev_set_power_state_off();
        }
        return GDIAL_APP_ERROR_NONE;
    }
    gdial_plat_dev_set_power_state_on();
    rtCastError ret = DialObj->launchApplicationWithLaunchParams(app_name, payload, query_string, additional_data_url);
    if (RTCAST_ERROR_RT(ret) != RT_OK) {
        printf("RTDIAL: DialObj.launchApplication failed!!! Error=%s\n",rtStrError(RTCAST_ERROR_RT(ret)));
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
        return GDIAL_APP_ERROR_BAD_REQUEST;
    }
    const char* State = AppCache->SearchAppStatusInCache(app_name);
    if (0 && strcmp(State,"running") != 0)
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
    if (RTCAST_ERROR_RT(ret) != RT_OK) {
        printf("RTDIAL: DialObj.stopApplication failed!!! Error=%s\n",rtStrError(RTCAST_ERROR_RT(ret)));
        return GDIAL_APP_ERROR_INTERNAL;
    }
    return GDIAL_APP_ERROR_NONE;
}

int gdial_os_application_hide(const char *app_name, int instance_id) {
    if((strcmp(app_name,"system") == 0)){
        return GDIAL_APP_ERROR_NONE;
    }
    #if 0
    printf("RTDIAL gdial_os_application_hide-->stop: appName = %s appID = %s\n",app_name,std::to_string(instance_id).c_str());
    const char* State = AppCache->SearchAppStatusInCache(app_name);
    if (0 && strcmp(State,"running") != 0) {
        return GDIAL_APP_ERROR_BAD_REQUEST;
    }
    rtCastError ret = DialObj->stopApplication(app_name,std::to_string(instance_id).c_str());
    if (RTCAST_ERROR_RT(ret) != RT_OK) {
        printf("RTDIAL: DialObj.stopApplication failed!!! Error=%s\n",rtStrError(RTCAST_ERROR_RT(ret)));
        return GDIAL_APP_ERROR_INTERNAL;
    }
    return GDIAL_APP_ERROR_NONE;
    #else
    printf("RTDIAL gdial_os_application_hide: appName = %s appID = %s\n",app_name,std::to_string(instance_id).c_str());
    const char* State = AppCache->SearchAppStatusInCache(app_name);
    if (strcmp(State,"running") != 0)
        return GDIAL_APP_ERROR_BAD_REQUEST;
    rtCastError ret = DialObj->hideApplication(app_name,std::to_string(instance_id).c_str());
    if (RTCAST_ERROR_RT(ret) != RT_OK) {
        printf("RTDIAL: DialObj.hideApplication failed!!! Error=%s\n",rtStrError(RTCAST_ERROR_RT(ret)));
        return GDIAL_APP_ERROR_INTERNAL;
    }
    return GDIAL_APP_ERROR_NONE;
    #endif
}

int gdial_os_application_resume(const char *app_name, int instance_id) {
    printf("RTDIAL gdial_os_application_resume: appName = %s appID = %s\n",app_name,std::to_string(instance_id).c_str());
    if((strcmp(app_name,"system") == 0)){
        return GDIAL_APP_ERROR_NONE;
    }
    const char* State = AppCache->SearchAppStatusInCache(app_name);
    if (strcmp(State,"running") == 0)
        return GDIAL_APP_ERROR_BAD_REQUEST;
    rtCastError ret = DialObj->resumeApplication(app_name,std::to_string(instance_id).c_str());
    if (RTCAST_ERROR_RT(ret) != RT_OK) {
        printf("RTDIAL: DialObj.resumeApplication failed!!! Error=%s\n",rtStrError(RTCAST_ERROR_RT(ret)));
        return GDIAL_APP_ERROR_INTERNAL;
    }
    return GDIAL_APP_ERROR_NONE;
}

int gdial_os_application_state(const char *app_name, int instance_id, GDialAppState *state) {
    printf("RTDIAL gdial_os_application_state: App = %s \n",app_name);
    if((strcmp(app_name,"system") == 0)){
        *state = GDIAL_APP_STATE_HIDE;
        return GDIAL_APP_ERROR_NONE;
    }
    const char* State = AppCache->SearchAppStatusInCache(app_name);
    printf("RTDIAL getApplicationState: AppState = %s \n",State);
    /*
     *  return cache, but also trigger a refresh
     */
    if((strcmp(app_name,"system") != 0) &&( true || strcmp(State,"NOT_FOUND") == 0)) {
        rtCastError ret = DialObj->getApplicationState(app_name,NULL);
        if (RTCAST_ERROR_RT(ret) != RT_OK) {
            printf("RTDIAL: DialObj.getApplicationState failed!!! Error: %s\n",rtStrError(RTCAST_ERROR_RT(ret)));
            return GDIAL_APP_ERROR_INTERNAL;
        }
    }

    if (strcmp(State,"running") == 0) {
        *state = GDIAL_APP_STATE_RUNNING;
    }
    else if (strcmp(State,"suspended") == 0) {
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
         else if (app_state == "suspended")
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
