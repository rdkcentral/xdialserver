/*
 * If not stated otherwise in this file or this component's LICENSE file the
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
#include <stdio.h>
#include <unistd.h>

static gdial_plat_dev_nwstandbymode_cb g_nwstandbymode_cb = NULL;
static gdial_plat_dev_powerstate_cb g_powerstate_cb = NULL;

void gdial_plat_dev_nwstandby_mode_change(gboolean NetworkStandbyMode)
{
    if(g_nwstandbymode_cb) g_nwstandbymode_cb(NetworkStandbyMode);
    printf("gdial_plat_dev_nwstandby_mode_change  new nwstandby_mode :%d",NetworkStandbyMode);
}

bool gdial_plat_dev_set_power_state_on() {
  if(g_powerstate_cb) {
    g_powerstate_cb("ON");
  }
  return true;
}

bool gdial_plat_dev_set_power_state_off() {
  if(g_powerstate_cb) {
    g_powerstate_cb("STANDBY");
  }
  return true;
}

bool gdial_plat_dev_toggle_power_state() {
  if(g_powerstate_cb) {
    g_powerstate_cb("TOGGLE");
  }
  return true;
}

void gdail_plat_dev_register_nwstandbymode_cb(gdial_plat_dev_nwstandbymode_cb cb)
{
   g_nwstandbymode_cb = cb;
}

void gdail_plat_dev_register_powerstate_cb(gdial_plat_dev_powerstate_cb cb)
{
   g_powerstate_cb = cb;
}