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

#ifndef GDIAL_OS_APP_H_
#define GDIAL_OS_APP_H_

#include "gdial-config.h"
#include "gdial-app.h"

#ifdef __cplusplus
extern "C" {
#endif

int gdial_os_application_start(const char *app_name, const char *payload, const char *query_string, const char *additional_data_url, int *instance_id);
int gdial_os_application_hide(const char *app_name, int instance_id);
int gdial_os_application_resume(const char *app_name, int instance_id);
int gdial_os_application_stop(const char *app_name, int instance_id);
int gdial_os_application_state(const char *app_name, int instance_id, GDialAppState *state);

int gdial_os_application_state_changed(const char *applicationName, const char *applicationId, const char *state, const char *error);
int gdial_os_application_activation_changed(const char *activation, const char *friendlyname);
int gdial_os_application_friendlyname_changed(const char *friendlyname);
const char* gdial_os_application_get_protocol_version();
int gdial_os_application_register_applications(void*);
int gdial_os_application_service_notification(gboolean isNotifyRequired, void* notifier);

#ifdef __cplusplus
}
#endif

#endif

