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

#ifndef _GDIAL_H_
#define _GDIAL_H_

#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/*
 ** Error strings
 **
 */
typedef enum _GDialErrorCode
{
	GDIAL_CAST_ERROR_NONE,
	GDIAL_CAST_ERROR_FORBIDDEN,
	GDIAL_CAST_ERROR_UNAVAILABLE,
	GDIAL_CAST_ERROR_INVALID,
	GDIAL_CAST_ERROR_INTERNAL
}
GDialErrorCode;

bool gdial_init(GMainContext *context);
void gdial_term();
typedef void (*gdial_activation_cb)(bool, const gchar *);
typedef void (*gdial_friendlyname_cb)(const gchar *);
typedef void (*gdial_registerapps_cb)(gpointer);
void gdial_register_activation_cb(gdial_activation_cb cb);
void gdial_register_friendlyname_cb(gdial_friendlyname_cb cb);
void gdial_register_registerapps_cb(gdial_registerapps_cb cb);

#ifdef __cplusplus
}
#endif

#endif
