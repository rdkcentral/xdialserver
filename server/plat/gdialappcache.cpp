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

#include "gdialappcache.hpp"

std::string GDialAppStatusCache::Netflix_AppCacheId = "DialNetflix";
std::string GDialAppStatusCache::Youtube_AppCacheId = "DialYoutube";

std::string GDialAppStatusCache :: getAppCacheId(const char* app_name)
{
    if (nullptr == app_name)
    {
        return std::string("");
    }
    if(!strcmp(app_name,"Netflix"))
        return GDialAppStatusCache::Netflix_AppCacheId;
    else if(!strcmp(app_name,"YouTube"))
        return GDialAppStatusCache::Youtube_AppCacheId;
    else
    {
        GDIAL_LOGINFO("default case : App Name is id");
        return app_name;
    }
}

void GDialAppStatusCache :: setAppCacheId(std::string app_name,std::string id)
{
    GDIAL_LOGTRACE("Entering ...");
    if(!strcmp(app_name.c_str(),"Netflix"))
    {
        GDialAppStatusCache::Netflix_AppCacheId = std::move(id);
        GDIAL_LOGINFO("App cache Id of Netflix updated to %s",GDialAppStatusCache::Netflix_AppCacheId.c_str());
    }
    else if(!strcmp(app_name.c_str(),"YouTube"))
    {
        GDialAppStatusCache::Youtube_AppCacheId = std::move(id);
        GDIAL_LOGINFO("App cache Id of Youtube updated to %s",GDialAppStatusCache::Youtube_AppCacheId.c_str());
    }
    else
    {
        GDIAL_LOGINFO("Default App Name - id not cached ");
    }
    GDIAL_LOGTRACE("Exiting ...");
}

AppCacheErrorCodes GDialAppStatusCache::UpdateAppStatusCache(AppInfo* appEntry)
{
    GDIAL_LOGTRACE("Entering ...");
    AppCacheErrorCodes err;
    GDIAL_LOGINFO("APPCache: AppName[%s] AppID[%s] AppState[%s] Error[%s]",
                    appEntry->appName.c_str(),
                    appEntry->appId.c_str(),
                    appEntry->appState.c_str(),
                    appEntry->appError.c_str());

    std::string id = getAppCacheId(appEntry->appName.c_str());

    if(doIdExist(id)) {
        GDIAL_LOGINFO("erasing old data");
        err = ObjectCache->erase(id);
    }
    err = ObjectCache->insert(std::move(id),appEntry);
    GDIAL_LOGTRACE("Exiting ...");
    return err;
}

std::string GDialAppStatusCache::SearchAppStatusInCache(const char* app_name)
{
    GDIAL_LOGTRACE("Entering ...");
    std::string state = "NOT_FOUND";

    std::string id = getAppCacheId(app_name);
    if(doIdExist(id))
    {
        AppInfo* appEntry = ObjectCache->findObject(id);

        state = appEntry->appState;
        GDIAL_LOGINFO("APPCache: App Name[%s] AppID[%s] Error[%s]",
            appEntry->appName.c_str(),
            appEntry->appId.c_str(),
            appEntry->appError.c_str());
    }
    GDIAL_LOGINFO("App State = %s ",state.c_str());
    GDIAL_LOGTRACE("Exiting ...");
    return state;
}

bool GDialAppStatusCache::doIdExist(std::string id)
{
    bool returnValue = false;
    GDIAL_LOGTRACE("Entering [%p] ...",ObjectCache);
    if(ObjectCache->touch(id) == AppCacheError_OK)
    {
       returnValue = true;
    }
    GDIAL_LOGINFO("IdExist [%s]",returnValue ? "true" : "false");
    GDIAL_LOGTRACE("Exiting ...");
    return returnValue;
}
