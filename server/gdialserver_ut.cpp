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
#include <csignal>
#include <atomic>
#include <thread>
#include <unistd.h>
#include "gdialservice.h"
#include "gdialservicelogging.h"

class gdialServiceTest: public GDialNotifier
{
    public:
        gdialServiceTest(const std::vector<std::string>& args)
        {
            service = gdialService::getInstance(this,args,"GDialUT");
        }

        ~gdialServiceTest()
        {
            gdialService::destroyInstance();
            service = nullptr;
        }

        void ActivationChanged(bool status)
        {
            GDIAL_LOGINFO("Entering ...");
            std::string activation = status ? "true" : "false";
            service->ActivationChanged(activation,"SampleTest");
            GDIAL_LOGINFO("Exiting ...");
        }

        void RegisterApplications(void)
        {
            RegisterAppEntryList *appReqList = new RegisterAppEntryList;
            GDIAL_LOGINFO("[%p] Freeing appConfigList",appReqList);
            for (int i=0; i < 9;i++)
            {
                RegisterAppEntry* appReq = new RegisterAppEntry;
                std::string Names;
                std::string prefixes;
                std::string cors;
                int allowStop = 0;


                switch(i)
                {
                    case 0:
                    {
                        Names = "Netflix";
                        prefixes = "";
                        cors = ".netflix.com";
                        allowStop = 0;
                    }
                    break;
                    case 1:
                    {
                        Names = "YouTube";
                        prefixes = "";
                        cors = ".youtube.com";
                        allowStop = 0;
                    }
                    break;
                    case 2:
                    {
                        Names = "YouTubeTV";
                        prefixes = "";
                        cors = ".youtube.com";
                        allowStop = 0;
                    }
                    break;
                    case 3:
                    {
                        Names = "YouTubeKids";
                        prefixes = "";
                        cors = ".youtube.com";
                        allowStop = 0;
                    }
                    break;
                    case 4:
                    {
                        Names = "AmazonInstantVideo";
                        prefixes = "";
                        cors = ".amazonprime.com";
                        allowStop = 0;
                    }
                    break;
                    case 5:
                    {
                        Names = "com.spotify.Spotify.TV";
                        prefixes = "com.spotify";
                        cors = ".spotify.com";
                        allowStop = 0;
                    }
                    break;
                    case 6:
                    {
                        Names = "com.apple.appletv";
                        prefixes = "com.apple";
                        cors = "tv.apple.com";
                        allowStop = 0;
                    }
                    break;
                    case 7:
                    {
                        Names = "Pairing";
                        prefixes = "";
                        cors = ".comcast.com";
                        allowStop = 0;
                    }
                    break;
                    case 8:
                    default:
                    {
                        Names = "Hello";
                        prefixes = "Hello";
                        cors = "Hello";
                        allowStop = 0;
                    }
                    break;
                }
                
                appReq->Names = Names;
                appReq->prefixes = prefixes;
                appReq->cors = prefixes;
                appReq->allowStop = allowStop;

                appReqList->pushBack(appReq);
            }
            service->RegisterApplications(appReqList);
        }        

        virtual void onApplicationLaunchRequestWithLaunchParam(string appName,string strPayLoad, string strQuery, string strAddDataUrl) override
        {
            GDIAL_LOGINFO("App:%s  payload:%s query_string:%s additional_data_url:%s",
                            appName.c_str(),
                            strPayLoad.c_str(),
                            strQuery.c_str(),
                            strAddDataUrl.c_str());
        }

        virtual void onApplicationLaunchRequest(string appName, string parameter) override
        {
            GDIAL_LOGINFO("App:%s  parameter:%s",appName.c_str(),parameter.c_str());
        }

        virtual void onApplicationStopRequest(string appName, string appID) override
        {
            GDIAL_LOGINFO("App:%s  appID:%s",appName.c_str(),appID.c_str());
        }

        virtual void onApplicationHideRequest(string appName, string appID) override
        {
            GDIAL_LOGINFO("App:%s  appID:%s",appName.c_str(),appID.c_str());
        }

        virtual void onApplicationResumeRequest(string appName, string appID) override
        {
            GDIAL_LOGINFO("App:%s  appID:%s",appName.c_str(),appID.c_str());
        }

        virtual void onApplicationStateRequest(string appName, string appID) override
        {
            GDIAL_LOGINFO("App:%s  appID:%s",appName.c_str(),appID.c_str());
        }

        virtual void onDisconnect() override
        {
            GDIAL_LOGINFO("~~~~~~~~~~~");
        }

        virtual void updatePowerState(string powerState) override
        {
            GDIAL_LOGINFO("powerState : %s",powerState.c_str());
        }
    private:
        gdialService* service{nullptr};
};

std::atomic<bool> running(true);

void signalHandler(int signum)
{
    GDIAL_LOGINFO("Interrupt signal:%d",signum);
    running = false;
}

int main(int argc, char *argv[])
{
    std::signal(SIGINT, signalHandler);
    std::vector<std::string> gdial_args;
    for (int i = 1; i < argc; ++i)
    {
        gdial_args.push_back(argv[i]);
    }
    gdialServiceTest* testObject = new gdialServiceTest(gdial_args);

    std::string input;
    GDIAL_LOGINFO("Enter commands (type 'q' to quit):");

    while (running) {
        std::cout << "> ";
        std::getline(std::cin, input);

        if (input == "q") {
            GDIAL_LOGINFO("Exiting ...");
            break;
        }

        if (input == "enable") {
            GDIAL_LOGINFO("Activation enabled");
            testObject->ActivationChanged(true);
        }
        else if (input == "disable") {
            GDIAL_LOGINFO("Activation disabled");
            testObject->ActivationChanged(false);
        }
        else if (input == "register")
        {
            GDIAL_LOGINFO("RegisterApps");
            testObject->RegisterApplications();
        }
        else{
            GDIAL_LOGINFO("Unknown option: ");
        }
    }
    delete testObject;

    return 0;
}