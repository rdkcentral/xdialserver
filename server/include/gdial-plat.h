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

#ifndef GDIAL_PLAT_H_
#define GDIAL_PLAT_H_

#include <glib.h>

G_BEGIN_DECLS

gint gdial_plat_init(GMainContext *g_main_context);
void gdial_plat_term();

typedef void * gdial_async_handler_t;
typedef void (*gdial_plat_activation_cb)(gboolean, const gchar *);
typedef void (*gdial_plat_friendlyname_cb)(const gchar *);
typedef void (*gdial_plat_registerapps_cb)(gpointer);
void gdail_plat_register_activation_cb(gdial_plat_activation_cb cb);
void gdail_plat_register_friendlyname_cb(gdial_plat_friendlyname_cb cb);
void gdail_plat_register_registerapps_cb(gdial_plat_registerapps_cb cb);

G_END_DECLS

#endif

