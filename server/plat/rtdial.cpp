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
#include "gdial-app.h"
#include "gdial-os-app.h"
#include "rtcast.hpp"
#include "rtcache.hpp"
#include "rtdial.hpp"

rtRemoteEnvironment* env;
static GSource *remoteSource = nullptr;
static GMainContext *main_context_ = nullptr;
static int INIT_COMPLETED = 0;
//cache
rtAppStatusCache* AppCache;
static rtdial_activation_cb g_activation_cb = NULL;

#define XCAST_SYSTEM_OBJECT_SERVICE_NAME "com.comcast.xcast_system"
#define RTDIAL_CONNECT_TO_XCAST_SYSTEM_TIMEOUT_MS 500
#define RTDIAL_CONNECT_TO_XCAST_SYSTEM_PERIOD_MS 5000

static bool connecting_to_xcast_system {false};
static rtObjectRef xcastSystemObj = nullptr;

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
        printf("AppName : %s\nAppID : %s\nState : %s\nError : %s\n",app.cString(),id.cString(),state.cString(),error.cString());
        AppCache->UpdateAppStatusCache(rtValue(AppObj));
        return RT_OK;
    }

    rtError activationChanged(const rtObjectRef& params) {
        rtObjectRef AppObj = new rtMapObject;
        AppObj = params;
        rtString status;
        AppObj.get("activation",status);
        printf("RTDIAL: rtDialCastRemoteObject::activationChanged status: %s \n",status.cString());
        if( g_activation_cb )
        {
            if(!strcmp(status.cString(), "true"))
            {
                g_activation_cb(1);
            }
            else
            {
                g_activation_cb(0);
            }
        }
        return RT_OK;
    }

    rtCastError launchApplication(const char* appName, const char* args) {
        printf("RTDIAL: rtDialCastRemoteObject::launchApplication App:%s  args:%s\n",appName,args);
        rtObjectRef AppObj = new rtMapObject;
        AppObj.set("applicationName",appName);
        AppObj.set("parameters",args);

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

static void rtdial_connect_to_xcast_system();

static void rtdial_remote_disconnect_callback(void*)
{
    // WARNING: xcastSystemObj is at this point dangling reference (has been deleted from rtRemoteObject::Release)
    // WARNING: and this is how it's supposed to work; trying to delete or ::Release it will lead to coredump
    xcastSystemObj = nullptr;
    rtdial_connect_to_xcast_system();
}

static gboolean rtdial_connect_to_xcast_system_async_callack(gpointer user_data)
{
    rtError err = rtRemoteLocateObject(rtEnvironmentGetGlobal(), XCAST_SYSTEM_OBJECT_SERVICE_NAME, xcastSystemObj, RTDIAL_CONNECT_TO_XCAST_SYSTEM_TIMEOUT_MS, &rtdial_remote_disconnect_callback, NULL);
    if (err == RT_OK) {
        connecting_to_xcast_system = false;
    } else {
        g_log(nullptr, G_LOG_LEVEL_INFO, "rtdial_connect_to_xcast_system_async_callack: couldn't connect to %s: %d\n", XCAST_SYSTEM_OBJECT_SERVICE_NAME, int(err));
    }
    // return true if we have to try again, false when connected!
    return connecting_to_xcast_system;
}

static void rtdial_connect_to_xcast_system_async() {
    connecting_to_xcast_system = true;
    g_timeout_add_full(G_PRIORITY_DEFAULT, RTDIAL_CONNECT_TO_XCAST_SYSTEM_PERIOD_MS, rtdial_connect_to_xcast_system_async_callack, /*data*/nullptr, nullptr /*GDestroyNotify*/);
}

static void rtdial_connect_to_xcast_system()
{
    if (!connecting_to_xcast_system) rtdial_connect_to_xcast_system_async();
}

bool rtdial_init(GMainContext *context) {
    if(INIT_COMPLETED)
       return true;
    rtError err;
    const char* objName;

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

    rtdial_connect_to_xcast_system();

    INIT_COMPLETED =1;
    return true;
}

void rtdial_term() {
    printf("RTDIAL: %s \n",__FUNCTION__);

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

    char url[DIAL_MAX_PAYLOAD+DIAL_MAX_ADDITIONALURL+100] = {0,};
    if(strcmp(app_name,"YouTube") == 0) {
        if ((payload != NULL) && (additional_data_url != NULL)){
            sprintf( url, "https://www.youtube.com/tv?%s&additionalDataUrl=%s", payload, additional_data_url);
        }else if (payload != NULL){
            sprintf( url, "https://www.youtube.com/tv?%s", payload);
        }else{
            sprintf( url, "https://www.youtube.com/tv");
        }
    }

    else if(strcmp(app_name,"Netflix") == 0) {
        memset( url, 0, sizeof(url) );
        strcat( url, "source_type=12" );
        if(payload != NULL)
        {
            const char * pUrlEncodedParams;
            pUrlEncodedParams = payload;
            if( pUrlEncodedParams ){
                strcat( url, "&dial=");
                strcat( url, pUrlEncodedParams );
            }
        }

        if(additional_data_url != NULL){
            strcat(url, "&additionalDataUrl=");
            strcat(url, additional_data_url);
        }
    }
    else {
        int url_len = sizeof(url);
        {
            memset( url, 0, url_len );
            url_len -= DIAL_MAX_ADDITIONALURL+1; //save for &additionalDataUrl
            url_len -= 1; //save for nul byte
            printf("query_string=[%s]\r\n", query_string);
            int has_query = query_string && strlen(query_string);
            int has_payload = 0;
            if (has_query) {
                strcat(url, query_string);
                url_len -= strlen(query_string);
            }
            if(payload && strlen(payload)) {
                if (has_query) url_len -=1;  //for &
                const char payload_key[] = "dialpayload=";
                url_len -= sizeof(payload_key) - 1;
                url_len -= strlen(payload);
                if(url_len >= 0){
                    if (has_query) strcat(url, "&");
                    strcat(url, payload_key);
                    strcat(url, payload);
                    has_payload = 1;
        }
                else {
                    printf("there is no enough room for payload\r\n");
                }
            }

        if(additional_data_url != NULL){
                if (has_query || has_payload) strcat(url, "&");
                strcat(url, "additionalDataUrl=");
            strcat(url, additional_data_url);
            }
            printf("url is [%s]\r\n", url);
        }
    }

    rtCastError ret = DialObj->launchApplication(app_name,url);
    if (RTCAST_ERROR_RT(ret) != RT_OK) {
        printf("RTDIAL: DialObj.launchApplication failed!!! Error=%s\n",rtStrError(RTCAST_ERROR_RT(ret)));
        return GDIAL_APP_ERROR_INTERNAL;
    }
    return GDIAL_APP_ERROR_NONE;
}

int gdial_os_application_stop(const char *app_name, int instance_id) {
    printf("RTDIAL gdial_os_application_stop: appName = %s appID = %s\n",app_name,std::to_string(instance_id).c_str());
    const char* State = AppCache->SearchAppStatusInCache(app_name);
    /* always to issue stop request to have a failsafe strategy */
    if (0 && strcmp(State,"running") != 0)
        return GDIAL_APP_ERROR_BAD_REQUEST;
    rtCastError ret = DialObj->stopApplication(app_name,std::to_string(instance_id).c_str());

    if (RTCAST_ERROR_RT(ret) != RT_OK) {
        printf("RTDIAL: DialObj.stopApplication failed!!! Error=%s\n",rtStrError(RTCAST_ERROR_RT(ret)));
        return GDIAL_APP_ERROR_INTERNAL;
    }
    return GDIAL_APP_ERROR_NONE;
}

int gdial_os_application_hide(const char *app_name, int instance_id) {
    #if 1
    printf("RTDIAL gdial_os_application_hide-->stop: appName = %s appID = %s\n",app_name,std::to_string(instance_id).c_str());
    const char* State = AppCache->SearchAppStatusInCache(app_name);
    /* always to issue hide request to have a failsafe strategy */
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
    const char* State = AppCache->SearchAppStatusInCache(app_name);
    printf("RTDIAL getApplicationState: AppState = %s \n",State);
    /*
     *  return cache, but also trigger a refresh
     */
    if(true || strcmp(State,"NOT_FOUND") == 0) {
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

    return GDIAL_APP_ERROR_NONE;
}

int gdial_os_system_app(GHashTable *query) {
    g_log(nullptr, G_LOG_LEVEL_INFO, "RTDIAL gdial_os_system_app\n");
    if (xcastSystemObj) {
        rtObjectRef params = new rtMapObject;
        if (query) {
            g_hash_table_foreach(query, [](gpointer key, gpointer value, gpointer user_data) {
                rtObjectRef& params_ = *static_cast<rtObjectRef*>(user_data);
                params_.set(static_cast<gchar*>(key),static_cast<gchar*>(value));
            }, &params);
        }
        rtError err = xcastSystemObj.send("systemRequest", params);
        return err == RT_OK ? GDIAL_APP_ERROR_NONE : GDIAL_APP_ERROR_INTERNAL;
    }
    else {
        g_log(nullptr, G_LOG_LEVEL_WARNING, "gdial_os_system_app: not connected!\n");
        return GDIAL_APP_ERROR_INTERNAL;
    }
}
