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
#include "rtabstractservice.hpp"
rtAbstractService::rtAbstractService(rtString serviceName)
    : mServiceName(serviceName)
    , mApiVersion(1), mEmit()
{
    mEmit = new rtEmit();
}
rtAbstractService::~rtAbstractService()
{
}
void rtAbstractService::setName(rtString n)
{
    mServiceName = n;
}
void rtAbstractService::setApiVersion(uint32_t v)
{
    mApiVersion = v;
}
rtError rtAbstractService::notify(const rtString& eventName, rtObjectRef e)
{
    mEmit.send(eventName, e);
    return RT_OK;
}
rtError rtAbstractService::bye()
{
    printf("%s : Sending bye event!!!\n",mServiceName.cString());
    rtObjectRef e = new rtMapObject;
    e.set("serviceName",mServiceName.cString());
    notify("bye", e);
    return RT_OK;
}
#include <stdio.h>
rtError rtAbstractService::name(rtString& v) const
{
    v = mServiceName;
    return RT_OK;
}
rtError rtAbstractService::version(uint32_t& v) const
{
    v = mApiVersion;
    return RT_OK;
}
rtError rtAbstractService::quirks(rtValue& v) const
{
    //rt services will override this method if they have any quirks
    rtObjectRef e = new rtMapObject;
    v = e;
    return RT_OK;
}
rtError rtAbstractService::addListener(rtString eventName, const rtFunctionRef &f)
{
    return mEmit->addListener(eventName, f);
}
rtError rtAbstractService::delListener(rtString eventName, const rtFunctionRef &f)
{
    return mEmit->delListener(eventName, f);
}
rtError rtAbstractService::ping(const rtObjectRef &params)
{
    printf("%s : ping!!!\n",mServiceName.cString());
    return RT_OK;
}
rtDefineObject(rtAbstractService, rtObject);
rtDefineProperty(rtAbstractService, name);
rtDefineProperty(rtAbstractService, version);
rtDefineProperty(rtAbstractService, quirks);
rtDefineMethod(rtAbstractService, addListener);
rtDefineMethod(rtAbstractService, delListener);
rtDefineMethod(rtAbstractService, ping);
