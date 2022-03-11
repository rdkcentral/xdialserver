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

#ifndef GDIAL_PLAT_APP_H_
#define GDIAL_PLAT_APP_H_

#include <glib.h>
#include "gdial-app.h"

G_BEGIN_DECLS

gint gdial_plat_init(GMainContext *g_main_context);
void gdial_plat_term();

typedef void * gdial_async_handler_t;
typedef void (*gdial_plat_activation_cb)(gboolean);
typedef void (*gdial_plat_friendlyname_cb)(const char*);
void gdail_plat_register_activation_cb(gdial_plat_activation_cb cb);
void gdail_plat_register_friendlyname_cb(gdial_plat_friendlyname_cb cb);

GDialAppError gdial_plat_application_start(const gchar *app_name, const gchar *payload, const gchar *query, const gchar *additional_data_url, gint *instance_id);
GDialAppError gdial_plat_application_hide(const gchar *app_name, gint instance_id);
GDialAppError gdial_plat_application_resume(const gchar *app_name, gint instance_id);
GDialAppError gdial_plat_application_stop(const gchar *app_name, gint instance_id);
GDialAppError gdial_plat_application_state(const gchar *app_name, gint instance_id, GDialAppState *state);

void * gdial_plat_application_start_async(const gchar *app_name, const gchar *payload, const gchar *query, const gchar *additional_data_url, void *user_data);
void * gdial_plat_application_state_async(const gchar *app_name, gint instance_id, void *user_data);
void * gdial_plat_application_hide_async(const gchar *app_name, gint instance_id, void *user_data);
void * gdial_plat_application_resume_async(const gchar *app_name, gint instance_id, void *user_data);
void * gdial_plat_application_stop_async(const gchar *app_name, gint instance_id, void *user_data);

void gdial_plat_application_remove_async_source(void *async_source);

typedef void (*gdial_plat_application_state_cb)(gint instance_id, GDialAppState state, gpointer user_data);
void gdial_plat_application_set_state_cb(gdial_plat_application_state_cb cb, gpointer user_data);

GDialAppError gdial_plat_system_app(GHashTable *query);

G_END_DECLS

#endif

