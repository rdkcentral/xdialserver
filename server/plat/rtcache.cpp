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

std::string rtAppStatusCache :: getAppCacheId(const char* app_name)
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

void rtAppStatusCache :: setAppCacheId(std::string app_name,std::string id)
{
     printf("RTCACHE : %s\n",__FUNCTION__);
     if(!strcmp(app_name.c_str(),"Netflix"))
     {
         rtAppStatusCache::Netflix_AppCacheId = id;
         printf("App cache Id of Netflix updated to %s\n",rtAppStatusCache::Netflix_AppCacheId.c_str());
     }
     else if(!strcmp(app_name.c_str(),"YouTube"))
     {
         rtAppStatusCache::Youtube_AppCacheId = id;
         printf("App cache Id of Youtube updated to %s\n",rtAppStatusCache::Youtube_AppCacheId.c_str());
     }
     else
     {
         printf("Default App Name - id not cached \n");
     }

}

AppCacheErrorCodes rtAppStatusCache::UpdateAppStatusCache(AppInfo* appEntry)
{
     printf("RTCACHE : %s\n",__FUNCTION__);

      AppCacheErrorCodes err;

      printf("RTCACHE : %s App Name = %s App ID = %s App State = %s Error = %s\n",
                __FUNCTION__,appEntry->appName,appEntry->appId,appEntry->appState,appEntry->appError);

      std::string id = getAppCacheId(appEntry->appName.c_str());

      if(doIdExist(id)) {
          printf("erasing old data\n");
          err = ObjectCache->erase(id);
      }

      err = ObjectCache->insert(id,appEntry);
      return err;
}

const char* rtAppStatusCache::SearchAppStatusInCache(const char* app_name)
{
     printf("RTCACHE : %s\n",__FUNCTION__);

      std::string id = getAppCacheId(app_name);
      if(doIdExist(id))
      {
         AppInfo* appEntry = ObjectCache->findObject(id);

         std::string state = appEntry->appState;
         printf("RTCACHE : %s App Name = %s App ID = %s Error = %s ",
                __FUNCTION__,
                appEntry->appName.c_str(),
                appEntry->appId.c_str(),
                appEntry->appError.c_str());
         printf("App State = %s\n",state.c_str());
         return state.c_str();
      }

      return "NOT_FOUND";
}

bool rtAppStatusCache::doIdExist(std::string id)
{
    printf("RTCACHE : %s : \n",__FUNCTION__);

    if(ObjectCache->touch(id)!= AppCacheErrorCodes::OK)
    {
       printf("False\n");
       return false;
    }
    printf("True\n");
    return true;
}
