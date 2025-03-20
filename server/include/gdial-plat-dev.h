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

#ifndef GDIAL_PLAT_DEV_H_
#define GDIAL_PLAT_DEV_H_

#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

bool gdial_plat_dev_set_power_state_on();
bool gdial_plat_dev_set_power_state_off();
bool gdial_plat_dev_toggle_power_state();
void gdial_plat_dev_nwstandby_mode_change(gboolean NetworkStandbyMode);

typedef void (*gdial_plat_dev_nwstandbymode_cb)(const bool );
void gdail_plat_dev_register_nwstandbymode_cb(gdial_plat_dev_nwstandbymode_cb cb);
typedef void (*gdial_plat_dev_powerstate_cb)(const char* );
void gdail_plat_dev_register_powerstate_cb(gdial_plat_dev_powerstate_cb cb);

#ifdef __cplusplus
}
#endif

#endif
