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

#ifndef GDIAL_H_
#define GDIAL_H_

#ifdef __cplusplus
extern "C" {
#endif

#ifndef GDIAL_BUILD_TEST
#define GDIAL_STATIC_DISABLED static
#else
#define GDIAL_STATIC_DISABLED extern
#endif

#define GDIAL_HIDDEN __attribute__ ((visibility ("hidden")))

#define GDIAL_PROTOCOL_VERSION 2.2.1
#define GDIAL_PROTOCOL_VERSION_STR "2.2.1"
#define GDIAL_PROTOCOL_XMLNS_SCHEMA "urn:dial-multiscreen-org:schemas:dial"

/*
 * Checks enforced by GDial impl
 */
#define GDIAL_CHECK(str)

/*
 * A marker that indicate a unresolved coredump case
 */
#define GDIAL_EXPECT_CORED(any)

/*
 * Get compile time strlen
 */
#define GDIAL_STR_SIZEOF(str) (sizeof(str)-1)
#define GDIAL_STR_ENDS_WITH(s1, s2) ((s1 != NULL) && (s2 != NULL) && ((strlen(s2) == 0) || (g_str_has_suffix(s1, s2))))
#define GDIAL_STR_STARTS_WITH(s1, s2) ((s1 != NULL) && (s2 != NULL) && ((strlen(s2) == 0) || (g_str_has_prefix(s1, s2))))

/*
 * os setting
*/

/*
 * server cmdline options
 */
#define GDIAL_IFACE_NAME_DEFAULT "lo"
#define GDIAL_REST_HTTP_PORT 56889
#define GDIAL_SSDP_HTTP_PORT 56890

#define GDIAL_REST_HTTP_MAX_URI_LEN 1024
#define GDIAL_REST_HTTP_RUN_URI "/run"
#define GDIAL_REST_HTTP_HIDE_URI "/hide"
#define GDIAL_REST_HTTP_PATH_COMPONENT_MAX_LEN (64)
#define GDIAL_REST_HTTP_DIAL_DATA_URI "/dial_data"

#define GDIAL_REST_HTTP_MAX_PAYLOAD (4096)
#define GDIAL_INVALID_PORT (65565+1)

#define GDIAL_APP_INSTANCE_NULL (~0)
#define GDIAL_APP_DIAL_DATA_DIR "/tmp/"
#define GDIAL_APP_DIAL_DATA_MAX_LEN (8*1024)
#define GDIAL_APP_DIAL_DATA_MAX_KV_LEN (255)
#define GDIAL_APP_DIAL_DATA_MAX_KV_LEN_STR "255"
#define GDIAL_THROTTLE_DELAY_US  100000
#define GDIAL_DEBUG g_print

enum {
 GDIAL_ERROR_NONE = 0,
 GDIAL_ERROR_NOT_REGISTERED,
 GDIAL_ERROR_FAIL_TO_START,
} GDialError;

#define GDIAL_GERROR_CHECK_AND_FREE(err, msg) \
{\
  if (err) {\
    g_printerr("%s err=%s\r\n", msg, err->message);\
    g_error_free(err);\
    err = NULL;\
  }\
}



#ifdef __cplusplus
}
#endif

#endif
