/*
 * If not stated otherwise in this file or this component's LICENSE file the
 * following copyright and licenses apply:
 *
 * Copyright 2024 RDK Management
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

/**
 * @file gdial_plat_stubs.cpp
 * @brief Stub implementations for the platform device layer.
 *
 * gdial-plat-app.c is now compiled directly into the test binary, providing
 * the real gdial_plat_application_* and gdial_plat_init/term implementations.
 * gdial_os_stubs.cpp provides the gdial_os_* and gdial_init/term/register_*
 * stubs that gdial-plat-app.c delegates into.
 *
 * This file contains only the device-layer stubs (gdial_plat_dev_* and
 * gdail_plat_dev_*) which are not covered by gdial-plat-app.c.
 */

#include <glib.h>
#include "gdial-plat-dev.h"

/* ------------------------------------------------------------------ */
/* Device / power-state operations                                     */
/* ------------------------------------------------------------------ */

bool gdial_plat_dev_set_power_state_on(void)  { return true; }
bool gdial_plat_dev_set_power_state_off(void) { return true; }
bool gdial_plat_dev_toggle_power_state(void)  { return true; }
void gdial_plat_dev_nwstandby_mode_change(gboolean NetworkStandbyMode) { (void)NetworkStandbyMode; }
void gdail_plat_dev_register_nwstandbymode_cb(gdial_plat_dev_nwstandbymode_cb cb) { (void)cb; }
void gdail_plat_dev_register_powerstate_cb(gdial_plat_dev_powerstate_cb cb)       { (void)cb; }

