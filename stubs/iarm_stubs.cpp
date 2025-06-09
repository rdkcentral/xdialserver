#include "libIBus.h"
#include "libIARMCore.h"
using namespace std;

IARM_Result_t IARM_Malloc(IARM_MemType_t type, size_t size, void **ptr)
{
    return IARM_RESULT_SUCCESS;
}

IARM_Result_t IARM_Free(IARM_MemType_t type, void *alloc)
{
    return IARM_RESULT_SUCCESS;
}

IARM_Result_t IARM_Bus_BroadcastEvent(const char *ownerName, IARM_EventId_t eventId, void *arg, size_t argLen)
{
    return IARM_RESULT_SUCCESS;
}

IARM_Result_t IARM_Bus_Init(const char* name)
{
    return IARM_RESULT_SUCCESS;
}

IARM_Result_t IARM_Bus_Connect()
{
    return IARM_RESULT_SUCCESS;
}

IARM_Result_t IARM_Bus_IsConnected(const char* memberName, int* isRegistered)
{
    return IARM_RESULT_SUCCESS;
}

IARM_Result_t IARM_Bus_RegisterEventHandler(const char* ownerName, IARM_EventId_t eventId, IARM_EventHandler_t handler)
{
    return IARM_RESULT_SUCCESS;
}

IARM_Result_t IARM_Bus_UnRegisterEventHandler(const char* ownerName, IARM_EventId_t eventId)
{
    return IARM_RESULT_SUCCESS;
}

IARM_Result_t IARM_Bus_RemoveEventHandler(const char* ownerName, IARM_EventId_t eventId, IARM_EventHandler_t handler)
{
    return IARM_RESULT_SUCCESS;
}
IARM_Result_t IARM_Bus_RegisterCall(const char *methodName, IARM_BusCall_t handler)
{
return IARM_RESULT_SUCCESS;
}
IARM_Result_t IARM_Bus_Term(void)
{
return IARM_RESULT_SUCCESS;
}
IARM_Result_t IARM_Bus_Disconnect(void)
{
return IARM_RESULT_SUCCESS;
}
IARM_Result_t IARM_Bus_RegisterEvent(IARM_EventId_t maxEventId)
{
return IARM_RESULT_SUCCESS;
}
IARM_Result_t IARM_Bus_Call(const char* ownerName, const char* methodName, void* arg, size_t argLen)
{
    return IARM_RESULT_SUCCESS;
}
