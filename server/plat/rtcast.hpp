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

#ifndef _RT_CAST_H_
#define _RT_CAST_H_

#include "rtabstractservice.hpp"
#include "rtRemote.h"
#include "rtObject.h"
#include "rtError.h"

/*
 * Error strings
 */
enum CastError {
  CAST_ERROR_NONE,
  CAST_ERROR_FORBIDDEN,
  CAST_ERROR_UNAVAILABLE,
  CAST_ERROR_INVALID,
  CAST_ERROR_INTERNAL
};

struct rtCastError {
  rtCastError(rtError errRt = RT_OK, CastError errCast = CAST_ERROR_NONE) : errRt(errRt),errCast(errCast){}
  rtError errRt;
  CastError errCast;
};

#define RTCAST_ERROR_NONE(v) ((v).errRt==RT_OK && (v).errCast==CAST_ERROR_NONE)
#define RTCAST_ERROR_RT(v) ((v).errRt)
#define RTCAST_ERROR_CAST(v) ((v).errCast)


class rtCastRemoteObject: public rtAbstractService
{
    rtDeclareObject(rtCastRemoteObject, rtAbstractService);

public:
    rtCastRemoteObject(rtString SERVICE_NAME) : rtAbstractService(SERVICE_NAME){}
    ~rtCastRemoteObject() {}

    rtMethod1ArgAndNoReturn("onApplicationStateChanged", applicationStateChanged, rtObjectRef);
    rtMethod1ArgAndNoReturn("onActivationChanged", activationChanged, rtObjectRef);
    rtMethod1ArgAndNoReturn("onFriendlyNameChanged", friendlyNameChanged, rtObjectRef);
    virtual rtError applicationStateChanged(const rtObjectRef& params){ printf("applicationStateChanged rtCastRemoteObject");}
    virtual rtError activationChanged (const rtObjectRef& params){ printf("activationChanged rtCastRemoteObject");}
    virtual rtError friendlyNameChanged (const rtObjectRef& params){ printf("friendlyNameChanged rtCastRemoteObject");}

  /*
    * rtCast implementation should emit these events:
    * onApplicationLaunchRequest
    * onApplicationStopRequest
    * onApplicationHideRequest
    * onApplicationResumeRequest
    * onApplicationStatusRequest
    * For details please reference XCAST spec
    */
};

#endif
