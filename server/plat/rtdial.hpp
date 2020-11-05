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

#ifndef RTDIAL_H_
#define RTDIAL_H_


#ifdef __cplusplus
extern "C" {
#include <stdbool.h>
#endif

bool rtdial_init(GMainContext *context);
void rtdial_term();
typedef void (*rtdial_activation_cb)(bool);
void rtdail_register_activation_cb(rtdial_activation_cb cb);

#ifdef __cplusplus
}
#endif

#endif
