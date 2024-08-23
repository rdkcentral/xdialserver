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

#include <iostream>
#include "gdialservice.h"

class gdialServiceTest: public GDialNotifier
{
    public:
        gdialServiceTest(const std::vector<std::string>& args)
        {
            service = gdialService::getInstance(this,args);
        }
        virtual void onApplicationLaunchRequestWithLaunchParam(string appName,string strPayLoad, string strQuery, string strAddDataUrl) override;
        virtual void onApplicationLaunchRequest(string appName, string parameter) override;
        virtual void onApplicationStopRequest(string appName, string appID) override;
        virtual void onApplicationHideRequest(string appName, string appID) override;
        virtual void onApplicationResumeRequest(string appName, string appID) override;
        virtual void onApplicationStateRequest(string appName, string appID) override;
    private:
        gdialService* service{nullptr};
};

void gdialServiceTest::onApplicationLaunchRequestWithLaunchParam(string appName,string strPayLoad, string strQuery, string strAddDataUrl )
{
    std::cout << "[" << __FUNCTION__ << "] "
              << "appName: " << appName.c_str() << ", "
              << "strPayLoad: " << strPayLoad.c_str() << ", "
              << "strQuery: " << strQuery.c_str() << ", "
              << "strAddDataUrl: " << strAddDataUrl.c_str() << std::endl;
}

void gdialServiceTest::onApplicationLaunchRequest(string appName, string parameter)
{
    std::cout << "[" << __FUNCTION__ << "] "
              << "appName: " << appName.c_str() << ", "
              << "parameter: " << parameter.c_str() << std::endl;
}

void gdialServiceTest::onApplicationStopRequest(string appName, string appID)
{
    std::cout << "[" << __FUNCTION__ << "] "
              << "appName: " << appName.c_str() << ", "
              << "appID: " << appID.c_str() << std::endl;
}

void gdialServiceTest::onApplicationHideRequest(string appName, string appID)
{
    std::cout << "[" << __FUNCTION__ << "] "
              << "appName: " << appName.c_str() << ", "
              << "appID: " << appID.c_str() << std::endl;
}

void gdialServiceTest::onApplicationResumeRequest(string appName, string appID)
{
    std::cout << "[" << __FUNCTION__ << "] "
              << "appName: " << appName.c_str() << ", "
              << "appID: " << appID.c_str() << std::endl;
}

void gdialServiceTest::onApplicationStateRequest(string appName, string appID)
{
    std::cout << "[" << __FUNCTION__ << "] "
              << "appName: " << appName.c_str() << ", "
              << "appID: " << appID.c_str() << std::endl;
}

int main(int argc, char *argv[])
{
    std::vector<std::string> gdial_args;
    gdial_args.push_back("-F");
    gdial_args.push_back("Element_ELTE11MWR");
    
    gdial_args.push_back("-R");
    gdial_args.push_back("Element");

    gdial_args.push_back("-M");
    gdial_args.push_back("ELTE11MWR");

    gdial_args.push_back("-U");
    gdial_args.push_back("881b6ed8-5185-499e-8c33-58f467186764");

    gdial_args.push_back("-A");
    gdial_args.push_back("youtube:spotify:netflix:pairing:youtubetv:youtubekids:system");
    gdialServiceTest* testObject = new gdialServiceTest(gdial_args);

    return 0;
}