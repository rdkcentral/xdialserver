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

#include <glib.h>
#include "gdial-plat-dev.h"
#include "libIBus.h"
#include "pwrMgr.h"

IARM_Bus_PWRMgr_PowerState_t m_powerstate = IARM_BUS_PWRMGR_POWERSTATE_STANDBY;
static int m_sleeptime = 1;
static bool m_is_restart_req = false;

static gdial_plat_dev_nwstandbymode_cb g_nwstandbymode_cb = NULL;

const char * gdial_plat_dev_get_manufacturer() {
  return g_getenv("GDIAL_DEV_MANUFACTURER");
}

const char * gdial_plat_dev_get_model() {
  return g_getenv("GDIAL_DEV_MODEL");
}

void gdial_plat_dev_power_mode_change(const char *owner, IARM_EventId_t eventId, void *data, size_t len)
{
  if ((strcmp(owner, IARM_BUS_PWRMGR_NAME)  == 0) && ( eventId == IARM_BUS_PWRMGR_EVENT_MODECHANGED )) {
    IARM_Bus_PWRMgr_EventData_t *param = (IARM_Bus_PWRMgr_EventData_t *)data;
    m_powerstate = param->data.state.newState;
    if(m_powerstate == IARM_BUS_PWRMGR_POWERSTATE_ON) {
      m_sleeptime = 1;
      if (m_is_restart_req) {
        //xdial restart to work in deepsleep wakeup
        system("systemctl restart xdial.service");
        m_is_restart_req = false;
      }
    }
    else if(m_powerstate == IARM_BUS_PWRMGR_POWERSTATE_STANDBY_DEEP_SLEEP) {
      m_sleeptime = 3;
      //After DEEPSLEEP, restart xdial again for next transition.
      m_is_restart_req = true;
    }
    printf("gdial_plat_dev_power_mode_change new power state: %d m_sleeptime:%d \n ",m_powerstate,m_sleeptime );
  }
}

void gdial_plat_dev_nwstandby_mode_change(const char *owner, IARM_EventId_t eventId, void *data, size_t len)
{
  if ((strcmp(owner, IARM_BUS_PWRMGR_NAME)  == 0) && ( eventId == IARM_BUS_PWRMGR_EVENT_NETWORK_STANDBYMODECHANGED )) {
    IARM_Bus_PWRMgr_EventData_t *param = (IARM_Bus_PWRMgr_EventData_t *)data;
    if(g_nwstandbymode_cb) g_nwstandbymode_cb(param->data.bNetworkStandbyMode);
    printf("gdial_plat_dev_nwstandby_mode_change  new nwstandby_mode :%d \n ",param->data.bNetworkStandbyMode);
  }
}

bool gdial_plat_dev_initialize() {
  IARM_Bus_Init("xdialserver");
  IARM_Bus_Connect();
  IARM_Result_t res;

  IARM_Bus_RegisterEventHandler(IARM_BUS_PWRMGR_NAME,IARM_BUS_PWRMGR_EVENT_MODECHANGED, gdial_plat_dev_power_mode_change);
  IARM_Bus_RegisterEventHandler(IARM_BUS_PWRMGR_NAME,IARM_BUS_PWRMGR_EVENT_NETWORK_STANDBYMODECHANGED, gdial_plat_dev_nwstandby_mode_change);

  IARM_Bus_PWRMgr_GetPowerState_Param_t param;
  res = IARM_Bus_Call(IARM_BUS_PWRMGR_NAME, IARM_BUS_PWRMGR_API_GetPowerState,
                (void *)&param, sizeof(param));
  if (res == IARM_RESULT_SUCCESS) {
    m_powerstate = param.curState;
  }

  printf("gdial_plat_dev_initialize m_powerstate :%d \n",m_powerstate);
  return true;
}

bool gdial_plat_dev_deinitialize() {
  IARM_Bus_Disconnect();
  IARM_Bus_Term();
  return true;
}

bool gdial_plat_dev_set_power_state_on() {
  bool ret = true;
  if(IARM_BUS_PWRMGR_POWERSTATE_ON != m_powerstate) {
    printf("gdial_plat_dev_set_power_state_on set power state to ON m_sleeptime:%d\n",m_sleeptime);
    IARM_Bus_PWRMgr_SetPowerState_Param_t param;
    param.newState = IARM_BUS_PWRMGR_POWERSTATE_ON;
    IARM_Result_t res = IARM_Bus_Call(IARM_BUS_PWRMGR_NAME, IARM_BUS_PWRMGR_API_SetPowerState,
                  (void *)&param, sizeof(param));
    if(res != IARM_RESULT_SUCCESS) {
      ret = false;
    }
    sleep(m_sleeptime);
  }
  return ret;
}

bool gdial_plat_dev_set_power_state_off() {
  bool ret = true;
  if(IARM_BUS_PWRMGR_POWERSTATE_ON == m_powerstate) {
    printf("gdial_plat_dev_set_power_state_off set power state STANDBY \n");
    IARM_Bus_PWRMgr_SetPowerState_Param_t param;
    param.newState = IARM_BUS_PWRMGR_POWERSTATE_STANDBY;
    IARM_Result_t res = IARM_Bus_Call(IARM_BUS_PWRMGR_NAME, IARM_BUS_PWRMGR_API_SetPowerState,
                  (void *)&param, sizeof(param));
    if(res != IARM_RESULT_SUCCESS) {
      ret = false;
    }
  }
  return ret;
}
void gdail_plat_dev_register_nwstandbymode_cb(gdial_plat_dev_nwstandbymode_cb cb)
{
   g_nwstandbymode_cb = cb;
}

bool gdial_plat_dev_get_nwstandby_mode() {
  bool nw_standby_mode = false;
  IARM_Bus_PWRMgr_NetworkStandbyMode_Param_t param;
  IARM_Result_t res = IARM_Bus_Call(IARM_BUS_PWRMGR_NAME,
                         IARM_BUS_PWRMGR_API_GetNetworkStandbyMode, (void *)&param,
                         sizeof(param));
  if(res == IARM_RESULT_SUCCESS) {
     nw_standby_mode = param.bStandbyMode;
  }
  printf("gdial_plat_dev_get_nwstandby_mode  nwstandby_mode:%d \n",nw_standby_mode);
  return nw_standby_mode;
}
