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

#ifndef GDIAL_REST_BUILDER_H_
#define GDIAL_REST_BUILDER_H_

#include <glib.h>

G_BEGIN_DECLS

GDIAL_STATIC_INLINE void *GET_APP_response_builder_new(const gchar *app_name);
GDIAL_STATIC_INLINE void *GET_APP_response_builder_set_option(void *builder, const gchar *option_name, const gchar *option_value);
GDIAL_STATIC_INLINE void *GET_APP_response_builder_set_state(void *builder, GDialAppState state);
GDIAL_STATIC_INLINE void *GET_APP_response_builder_set_installable(void *builder, const gchar *encoded_url);
GDIAL_STATIC_INLINE void *GET_APP_response_builder_set_link_href(void *builder, const gchar *encoded_href);
GDIAL_STATIC_INLINE void *GET_APP_response_builder_set_additionalData(void *builder, const gchar *additionalData);
GDIAL_STATIC_INLINE gchar *GET_APP_response_builder_build(void *builder, gsize *length);
GDIAL_STATIC_INLINE void GET_APP_response_builder_destroy(void *builder);

G_END_DECLS
#endif

