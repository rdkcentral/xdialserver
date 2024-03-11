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

#include "rtcache.hpp"

std::string rtAppStatusCache::Netflix_AppCacheId = "DialNetflix";
std::string rtAppStatusCache::Youtube_AppCacheId = "DialYoutube";

std::string rtAppStatusCache :: getAppCacheId(const char *app_name)
{
     printf("RTCACHE : %s\n",__FUNCTION__);

     if(!strcmp(app_name,"Netflix"))
          return rtAppStatusCache::Netflix_AppCacheId;
     else if(!strcmp(app_name,"YouTube"))
          return rtAppStatusCache::Youtube_AppCacheId;
     else
     {
          printf("default case : App Name is id\n");
          return app_name;
     }

}

void rtAppStatusCache :: setAppCacheId(const char *app_name,std::string id)
{
     printf("RTCACHE : %s\n",__FUNCTION__);
     if(!strcmp(app_name,"Netflix"))
     {
         rtAppStatusCache::Netflix_AppCacheId = id;
         printf("App cache Id of Netflix updated to %s\n",rtAppStatusCache::Netflix_AppCacheId);
     }
     else if(!strcmp(app_name,"YouTube"))
     {
         rtAppStatusCache::Youtube_AppCacheId = id;
         printf("App cache Id of Youtube updated to %s\n",rtAppStatusCache::Youtube_AppCacheId);
     }
     else
     {
         printf("Default App Name - id not cached \n");
     }

}

rtError rtAppStatusCache::UpdateAppStatusCache(rtValue app_status)
{
     const auto now = std::chrono::steady_clock::now();
     printf("RTCACHE : %s\n",__FUNCTION__);

      rtError err;
      rtObjectRef temp = app_status.toObject();

      const char *App_name = strdup(temp.get<rtString>("applicationName").cString());
      printf("RTCACHE : %s App Name = %s App ID = %s App State = %s Error = %s\n",__FUNCTION__,App_name,temp.get<rtString>("applicationId").cString(),temp.get<rtString>("state").cString(),temp.get<rtString>("error").cString());

      std::string id = getAppCacheId(App_name);

      if(doIdExist(id)) {
          printf("erasing old data\n");
          err = ObjectCache->erase(id);
      }

      err = ObjectCache->insert(id,temp);
      notifyStateChanged(App_name);
      if (err == RT_OK) {
          err = ObjectCache->markUnevictable(id, true);
          last_updated[id] = now;
      }
      return err;
}

rtAppStatusCache::StateChangedCallbackHandle rtAppStatusCache::registerStateChangedCallback(StateChangedCallback callback) {
     std::unique_lock<std::mutex> lock(state_changed_listeners_mutex);
     const auto handle = ++next_handle;
     state_changed_listeners[handle] = callback;
     return handle;
}

void rtAppStatusCache::unregisterStateChangedCallback(rtAppStatusCache::StateChangedCallbackHandle callbackId) {
     std::unique_lock<std::mutex> lock(state_changed_listeners_mutex);
     state_changed_listeners.erase(callbackId);
}

void rtAppStatusCache::notifyStateChanged(std::string& id) {
     std::unique_lock<std::mutex> lock(state_changed_listeners_mutex);
     for (auto& it : state_changed_listeners) {
          it.second(id);
     }
}


const char * rtAppStatusCache::SearchAppStatusInCache(const char *app_name)
{
     printf("RTCACHE : %s\n",__FUNCTION__);

      std::string id = getAppCacheId(app_name);
      if(doIdExist(id))
      {
         rtObjectRef state_param = ObjectCache->findObject(id);

         char *state = strdup(state_param.get<rtString>("state").cString());
         printf("RTCACHE : %s App Name = %s App ID = %s Error = %s ",__FUNCTION__,state_param.get<rtString>("applicationName").cString(),state_param.get<rtString>("applicationId").cString(),state_param.get<rtString>("error").cString());
         printf("App State = %s\n",state);
         return state;
      }

      return "NOT_FOUND";
}

bool rtAppStatusCache::doIdExist(std::string id)
{
    printf("RTCACHE : %s : \n",__FUNCTION__);
    auto now = std::chrono::steady_clock::now();

    if(ObjectCache->touch(id,now)!= RT_OK)
    {
       printf("False\n");
       return false;
    }
    printf("True\n");
    return true;
}

std::chrono::milliseconds rtAppStatusCache::getUpdateAge(const char *app_name)
{
     const auto now = std::chrono::steady_clock::now();
     std::string id = getAppCacheId(app_name);
     auto it = last_updated.find(id);
     if (it != last_updated.end()) {
          return std::chrono::duration_cast<std::chrono::milliseconds>(now - it->second);
     } else {
          return std::chrono::milliseconds::max();
     }
}
