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
         printf("App cache Id of Netflix updated to %s\n",rtAppStatusCache::Netflix_AppCacheId.c_str());
     }
     else if(!strcmp(app_name,"YouTube"))
     {
         rtAppStatusCache::Youtube_AppCacheId = id;
         printf("App cache Id of Youtube updated to %s\n",rtAppStatusCache::Youtube_AppCacheId.c_str());
     }
     else
     {
         printf("Default App Name - id not cached \n");
     }

}

rtError rtAppStatusCache::UpdateAppStatusCache(rtValue app_status)
{
     printf("RTCACHE : %s\n",__FUNCTION__);

      rtError err;
      rtObjectRef temp = app_status.toObject();

      std::string App_name {temp.get<rtString>("applicationName").cString()};
      printf("RTCACHE : %s App Name = %s App ID = %s App State = %s Error = %s\n",__FUNCTION__,App_name.c_str(),temp.get<rtString>("applicationId").cString(),temp.get<rtString>("state").cString(),temp.get<rtString>("error").cString());

      std::string id = getAppCacheId(App_name.c_str());

      if(doIdExist(id)) {
          printf("erasing old data\n");
          err = ObjectCache->erase(id);
      }

      err = ObjectCache->insert(id,temp);
      return err;
}

std::string rtAppStatusCache::SearchAppStatusInCache(const char *app_name)
{
     printf("RTCACHE : %s\n",__FUNCTION__);

      std::string id = getAppCacheId(app_name);
      if(doIdExist(id))
      {
         rtObjectRef state_param = ObjectCache->findObject(id);

         std::string state {state_param.get<rtString>("state").cString()};
         printf("RTCACHE : %s App Name = %s App ID = %s Error = %s ",__FUNCTION__,state_param.get<rtString>("applicationName").cString(),state_param.get<rtString>("applicationId").cString(),state_param.get<rtString>("error").cString());
         printf("App State = %s\n",state.c_str());
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
