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

#ifndef _GDIAL_SERVICE_COMMON_H_
#define _GDIAL_SERVICE_COMMON_H_

#include <string>
#include <vector>
#include <glib.h>
#include <stdint.h>

using namespace std;

typedef enum gdialServiceErrorCodes
{
    GDIAL_SERVICE_ERROR_NONE,
    GDIAL_SERVICE_ERROR_FAILED
}
GDIAL_SERVICE_ERROR_CODES;

class RegisterAppEntry
{
public:
    std::string Names;
    std::string prefixes;
    std::string cors;
    int allowStop;
};

class RegisterAppEntryList
{
public:
    void pushBack(RegisterAppEntry* appEntry)
	{
        appEntries.push_back(appEntry);
    }

    const std::vector<RegisterAppEntry*>& getValues() const
	{
        return appEntries;
    }

    ~RegisterAppEntryList()
	{
        for (RegisterAppEntry* appEntry : appEntries)
		{
            delete appEntry;
        }
        appEntries.clear();
    }

private:
    std::vector<RegisterAppEntry*> appEntries;
};

class GDialNotifier
{
public:
    virtual void onApplicationLaunchRequest(string appName, string parameter)=0;
    virtual void onApplicationLaunchRequestWithLaunchParam (string appName,string strPayLoad, string strQuery, string strAddDataUrl)=0;
    virtual void onApplicationStopRequest(string appName, string appID)=0;
    virtual void onApplicationHideRequest(string appName, string appID)=0;
    virtual void onApplicationResumeRequest(string appName, string appID)=0;
    virtual void onApplicationStateRequest(string appName, string appID)=0;
    virtual void onStopped(void)=0;
    virtual void updatePowerState(string powerState)=0;
};
#endif /* _GDIAL_SERVICE_COMMON_H_ */