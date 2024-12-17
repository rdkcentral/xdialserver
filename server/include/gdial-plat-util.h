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

#ifndef GDIAL_PLAT_UTIL_H_
#define GDIAL_PLAT_UTIL_H_

#ifdef __cplusplus
extern "C" {
#endif

const char * gdial_plat_util_get_iface_ipv4_addr(const char *ifname);
const char * gdial_plat_util_get_iface_mac_addr(const char *ifname);
int gdial_plat_util_is_process_running(const char *cmd_name, const char*cmd_pattern);
int gdial_plat_util_run_command( const char * const args[], int *instance_id);

#ifdef __cplusplus
}
#endif

#endif
