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

#ifndef _GDIAL_SERVICE_IMPL_H_
#define _GDIAL_SERVICE_IMPL_H_

#include <iostream>
#include <string>
#include <vector>
#include <glib.h>
#include <pthread.h>
#include <mutex>
#include <condition_variable>
#include <queue>
#include <stdint.h>

typedef enum _AppRequestEvents
{
    APP_STATE_CHANGED,
    ACTIVATION_CHANGED,
    FRIENDLYNAME_CHANGED,
    REGISTER_APPLICATIONS,
    INVALID_REQUEST
}
AppRequestEvents;

typedef enum _AppResponseEvents
{
    APP_LAUNCH_REQUEST_WITH_PARAMS,
    APP_LAUNCH_REQUEST,
    APP_STOP_REQUEST,
    APP_HIDE_REQUEST,
    APP_STATE_REQUEST,
    APP_RESUME_REQUEST,
    APP_INVALID_STATE
}
AppResponseEvents;

typedef struct _RequestHandlerPayload
{
    std::string appNameOrfriendlyname;
    std::string appIdOractivation;
    std::string state;
    std::string error;
    void* data_param;
    AppRequestEvents event;
}RequestHandlerPayload;

typedef struct _ResponseHandlerPayload
{
    std::string appName;
    std::string parameterOrPayload;
    std::string appIdOrQuery;
    std::string AddDataUrl;
    AppResponseEvents event;
}ResponseHandlerPayload;

class gdialServiceImpl: public GDialNotifier
{
public:
    gdialServiceImpl() {
        std::cout << "gdialServiceImpl Constructor Called" << std::endl;
    };
    ~gdialServiceImpl() {
        std::cout << "gdialServiceImpl Destructor Called" << std::endl;
    };
    int start_GDialServer(int argc, char *argv[]);
    bool stop_GDialServer();
    void sendRequest( const RequestHandlerPayload& payload );
    void setService(GDialNotifier* service)
    {
        m_observer = service;
    }

    virtual void onApplicationLaunchRequest(string appName, string parameter) override;
    virtual void onApplicationLaunchRequestWithLaunchParam (string appName,string strPayLoad, string strQuery, string strAddDataUrl) override;
    virtual void onApplicationStopRequest(string appName, string appID) override;
    virtual void onApplicationHideRequest(string appName, string appID) override;
    virtual void onApplicationResumeRequest(string appName, string appID) override;
    virtual void onApplicationStateRequest(string appName, string appID) override;

private:
    GDialNotifier *m_observer;
    pthread_t m_gdialserver_main_thread{0};

    static void *GDialMain(void *ctx);
    GMainLoop *m_main_loop{nullptr};
    GMainContext *m_main_loop_context{nullptr};

    static void *GDialRequestHandler(void *ctx);
    pthread_t m_gdialserver_request_handler_thread{0};
    bool m_RequestHandlerThreadExit{0};
    bool m_RequestHandlerThreadRun{0};
    std::mutex m_RequestHandlerEventMutex;
    std::queue<RequestHandlerPayload> m_RequestHandlerQueue;
    std::condition_variable m_RequestHandlerCV;

    static void *GDialResponseHandler(void *ctx);
    pthread_t m_gdialserver_response_handler_thread{0};
    bool m_ResponseHandlerThreadExit{0};
    bool m_ResponseHandlerThreadRun{0};
    std::mutex m_ResponseHandlerEventMutex;
    std::queue<ResponseHandlerPayload> m_ResponseHandlerQueue;
    std::condition_variable m_ResponseHandlerCV;

    void notifyResponse( const ResponseHandlerPayload& payload );
};
#endif /* gdialServiceImpl_hpp */