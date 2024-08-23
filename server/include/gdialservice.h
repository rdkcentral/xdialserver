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

#ifndef _GDIAL_SERVICE_H_
#define _GDIAL_SERVICE_H_

#include <string>
#include <vector>
#include <glib.h>
#include <pthread.h>
#include <stdint.h>
#include "gdialservicecommon.h"

using namespace std;

class gdialService
{
public:
    static gdialService* getInstance(GDialNotifier* observer, const std::vector<std::string>& gdial_args);
    static void destroyInstance();

    int ApplicationStateChanged(string applicationName, string applicationId, string state, string error);
    int ActivationChanged(string activation, string friendlyname);
    int FriendlyNameChanged(string friendlyname);
    string getProtocolVersion(void);
    int RegisterApplications(RegisterAppEntryList* appConfigList);

private:
    GDialNotifier* m_observer{nullptr};
    gdialService(){};
    virtual ~gdialService(){};
};
#endif /* gdialService_hpp */