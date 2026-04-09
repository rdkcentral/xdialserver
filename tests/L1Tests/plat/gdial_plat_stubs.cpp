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
 * @brief Stub implementations for the platform application and device layers.
 *
 * gdial-app.c is compiled into the test binary and references every
 * gdial_plat_application_* and gdial_plat_dev_* symbol declared in
 * gdial-plat-app.h / gdial-plat-dev.h.  These stubs provide the
 * minimum behaviour required for unit tests:
 *
 *  - application_start  returns NONE and sets *instance_id = 1
 *  - application_state  returns NONE and sets *state = STOPPED
 *  - all other calls    return NONE (or void)
 */

#include <glib.h>
#include "gdial-plat-app.h"
#include "gdial-plat-dev.h"

/* ------------------------------------------------------------------ */
/* Platform init / term                                                 */
/* ------------------------------------------------------------------ */

gint gdial_plat_init(GMainContext *ctx)
{
    (void)ctx;
    return 0;
}

void gdial_plat_term(void) {}

/* ------------------------------------------------------------------ */
/* Callback registration                                               */
/* ------------------------------------------------------------------ */

void gdail_plat_register_activation_cb(gdial_plat_activation_cb cb)      { (void)cb; }
void gdail_plat_register_friendlyname_cb(gdial_plat_friendlyname_cb cb)  { (void)cb; }
void gdail_plat_register_registerapps_cb(gdial_plat_registerapps_cb cb)  { (void)cb; }
void gdail_plat_register_manufacturername_cb(gdial_plat_manufacturername_cb cb) { (void)cb; }
void gdail_plat_register_modelname_cb(gdial_plat_modelname_cb cb)        { (void)cb; }

static gdial_plat_application_state_cb s_state_cb = nullptr;
static gpointer s_state_cb_data = nullptr;

void gdial_plat_application_set_state_cb(
        gdial_plat_application_state_cb cb, gpointer user_data)
{
    s_state_cb      = cb;
    s_state_cb_data = user_data;
}

/* ------------------------------------------------------------------ */
/* Synchronous application operations                                  */
/* ------------------------------------------------------------------ */

GDialAppError gdial_plat_application_start(
        const gchar *app_name, const gchar *payload,
        const gchar *query, const gchar *additional_data_url,
        gint *instance_id)
{
    (void)app_name; (void)payload; (void)query; (void)additional_data_url;
    if (instance_id) *instance_id = 1;
    return GDIAL_APP_ERROR_NONE;
}

GDialAppError gdial_plat_application_hide(
        const gchar *app_name, gint instance_id)
{
    (void)app_name; (void)instance_id;
    return GDIAL_APP_ERROR_NONE;
}

GDialAppError gdial_plat_application_resume(
        const gchar *app_name, gint instance_id)
{
    (void)app_name; (void)instance_id;
    return GDIAL_APP_ERROR_NONE;
}

GDialAppError gdial_plat_application_stop(
        const gchar *app_name, gint instance_id)
{
    (void)app_name; (void)instance_id;
    return GDIAL_APP_ERROR_NONE;
}

GDialAppError gdial_plat_application_state(
        const gchar *app_name, gint instance_id, GDialAppState *state)
{
    (void)app_name; (void)instance_id;
    if (state) *state = GDIAL_APP_STATE_STOPPED;
    return GDIAL_APP_ERROR_NONE;
}

/* ------------------------------------------------------------------ */
/* Notification helpers                                                */
/* ------------------------------------------------------------------ */

GDialAppError gdial_plat_application_state_changed(
        const char *appName, const char *appId,
        const char *state, const char *error)
{
    (void)appName; (void)appId; (void)state; (void)error;
    return GDIAL_APP_ERROR_NONE;
}

GDialAppError gdial_plat_application_activation_changed(
        const char *activation, const char *friendlyname)
{
    (void)activation; (void)friendlyname;
    return GDIAL_APP_ERROR_NONE;
}

GDialAppError gdial_plat_application_friendlyname_changed(
        const char *friendlyname)
{
    (void)friendlyname;
    return GDIAL_APP_ERROR_NONE;
}

const char *gdial_plat_application_get_protocol_version(void)
{
    return "2.2.1";
}

GDialAppError gdial_plat_application_register_applications(void *p)
{
    (void)p;
    return GDIAL_APP_ERROR_NONE;
}

void gdial_plat_application_update_network_standby_mode(gboolean mode)
{
    (void)mode;
}

GDialAppError gdial_plat_application_update_manufacturer_name(
        const char *manufacturer)
{
    (void)manufacturer;
    return GDIAL_APP_ERROR_NONE;
}

GDialAppError gdial_plat_application_update_model_name(
        const char *model)
{
    (void)model;
    return GDIAL_APP_ERROR_NONE;
}

GDialAppError gdial_plat_application_service_notification(
        gboolean isNotifyRequired, void *notifier)
{
    (void)isNotifyRequired; (void)notifier;
    return GDIAL_APP_ERROR_NONE;
}

/* ------------------------------------------------------------------ */
/* Async operations (return nullptr = no async handle)                 */
/* ------------------------------------------------------------------ */

void *gdial_plat_application_start_async(
        const gchar *app_name, const gchar *payload,
        const gchar *query, const gchar *additional_data_url,
        void *user_data)
{
    (void)app_name; (void)payload; (void)query;
    (void)additional_data_url; (void)user_data;
    return nullptr;
}

void *gdial_plat_application_state_async(
        const gchar *app_name, gint instance_id, void *user_data)
{
    (void)app_name; (void)instance_id; (void)user_data;
    return nullptr;
}

void *gdial_plat_application_hide_async(
        const gchar *app_name, gint instance_id, void *user_data)
{
    (void)app_name; (void)instance_id; (void)user_data;
    return nullptr;
}

void *gdial_plat_application_resume_async(
        const gchar *app_name, gint instance_id, void *user_data)
{
    (void)app_name; (void)instance_id; (void)user_data;
    return nullptr;
}

void *gdial_plat_application_stop_async(
        const gchar *app_name, gint instance_id, void *user_data)
{
    (void)app_name; (void)instance_id; (void)user_data;
    return nullptr;
}

void gdial_plat_application_remove_async_source(void *async_source)
{
    (void)async_source;
}

/* ------------------------------------------------------------------ */
/* Device / power-state operations                                     */
/* ------------------------------------------------------------------ */

bool gdial_plat_dev_set_power_state_on(void)  { return true; }
bool gdial_plat_dev_set_power_state_off(void) { return true; }
bool gdial_plat_dev_toggle_power_state(void)  { return true; }
void gdial_plat_dev_nwstandby_mode_change(gboolean NetworkStandbyMode) { (void)NetworkStandbyMode; }
void gdail_plat_dev_register_nwstandbymode_cb(gdial_plat_dev_nwstandbymode_cb cb) { (void)cb; }
void gdail_plat_dev_register_powerstate_cb(gdial_plat_dev_powerstate_cb cb)       { (void)cb; }

