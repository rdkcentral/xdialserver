/*
  * This header defines the public methods and names specified by XCAST Spec as in
  *  https://etwiki.sys.comcast.net/display/APPS/XCAST
  */
#ifndef RT_CAST_H
#define RT_CAST_H

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
    virtual rtError applicationStateChanged(const rtObjectRef& params){ printf("applicationStateChanged rtCastRemoteObject");}
    virtual rtError activationChanged (const rtObjectRef& params){ printf("activationChanged rtCastRemoteObject");}

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


#endif //RT_CAST_H
