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
 * @file gdial_os_stubs.cpp
 *
 * Stubs for the layer *below* gdial-plat-app.c:
 *   - gdial_os_application_*  (gdial-os-app.h)
 *   - gdial_init / gdial_term / gdial_register_*  (gdial.hpp)
 *
 * Also provides two gdial_plat_application_*_async symbols that are declared in
 * gdial-plat-app.h but intentionally absent from gdial-plat-app.c:
 *   - gdial_plat_application_hide_async
 *   - gdial_plat_application_resume_async
 *
 * The gdial_plat_stub_reset_behavior / _set_app_state / _set_errors control
 * interface is preserved at this layer so that any future test code that uses
 * those helpers continues to work.
 */

#include <glib.h>

#include "gdial-app.h"      /* GDialAppState, GDialAppError */
#include "gdial-os-app.h"
#include "gdial-plat-app.h" /* hide_async / resume_async declarations */
#include "gdial.hpp"

/* ------------------------------------------------------------------ */
/* Injectable error / state controls                                   */
/* ------------------------------------------------------------------ */

static GDialAppState s_app_state  = GDIAL_APP_STATE_STOPPED;
static int           s_start_err  = 0;
static int           s_hide_err   = 0;
static int           s_resume_err = 0;
static int           s_stop_err   = 0;
static int           s_state_err  = 0;

extern "C" void gdial_plat_stub_reset_behavior(void)
{
    s_app_state  = GDIAL_APP_STATE_STOPPED;
    s_start_err  = 0;
    s_hide_err   = 0;
    s_resume_err = 0;
    s_stop_err   = 0;
    s_state_err  = 0;
}

extern "C" void gdial_plat_stub_set_app_state(GDialAppState state)
{
    s_app_state = state;
}

extern "C" void gdial_plat_stub_set_errors(
        GDialAppError start_err,
        GDialAppError hide_err,
        GDialAppError resume_err,
        GDialAppError stop_err,
        GDialAppError state_err)
{
    s_start_err  = (int)start_err;
    s_hide_err   = (int)hide_err;
    s_resume_err = (int)resume_err;
    s_stop_err   = (int)stop_err;
    s_state_err  = (int)state_err;
}

/* ------------------------------------------------------------------ */
/* gdial.hpp — GLib integration layer called by gdial-plat-app.c      */
/* ------------------------------------------------------------------ */

bool gdial_init(GMainContext *context) { (void)context; return true; }
void gdial_term(void) {}

void gdial_register_activation_cb(gdial_activation_cb cb)             { (void)cb; }
void gdial_register_friendlyname_cb(gdial_friendlyname_cb cb)         { (void)cb; }
void gdial_register_registerapps_cb(gdial_registerapps_cb cb)         { (void)cb; }
void gdial_register_manufacturername_cb(gdial_manufacturername_cb cb) { (void)cb; }
void gdial_register_modelname_cb(gdial_manufacturername_cb cb)        { (void)cb; }

/* ------------------------------------------------------------------ */
/* gdial_os_application_* — one level below gdial-plat-app.c          */
/* ------------------------------------------------------------------ */

int gdial_os_application_start(
        const char *name, const char *payload,
        const char *query, const char *url, int *instance_id)
{
    (void)name; (void)payload; (void)query; (void)url;
    if (s_start_err) return s_start_err;
    if (instance_id) *instance_id = 1;
    return 0;
}

int gdial_os_application_hide(const char *name, int id)
{
    (void)name; (void)id;
    return s_hide_err;
}

int gdial_os_application_resume(const char *name, int id)
{
    (void)name; (void)id;
    return s_resume_err;
}

int gdial_os_application_stop(const char *name, int id)
{
    (void)name; (void)id;
    return s_stop_err;
}

int gdial_os_application_state(const char *name, int id, GDialAppState *state)
{
    (void)name; (void)id;
    if (s_state_err) return s_state_err;
    if (state) *state = s_app_state;
    return 0;
}

int gdial_os_application_state_changed(
        const char *name, const char *id, const char *state, const char *error)
{
    (void)name; (void)id; (void)state; (void)error;
    return 0;
}

int gdial_os_application_activation_changed(
        const char *activation, const char *friendly)
{
    (void)activation; (void)friendly;
    return 0;
}

int gdial_os_application_friendlyname_changed(const char *name)
{
    (void)name;
    return 0;
}

const char *gdial_os_application_get_protocol_version(void)
{
    return "2.2.1";
}

int gdial_os_application_register_applications(void *p)
{
    (void)p;
    return 0;
}

void gdial_os_application_update_network_standby_mode(gboolean mode)
{
    (void)mode;
}

int gdial_os_application_update_manufacturer_name(const char *name)
{
    (void)name;
    return 0;
}

int gdial_os_application_update_model_name(const char *model)
{
    (void)model;
    return 0;
}

int gdial_os_application_service_notification(gboolean req, void *notifier)
{
    (void)req; (void)notifier;
    return 0;
}

/* ------------------------------------------------------------------ */
/* Async stubs — declared in gdial-plat-app.h but absent from         */
/* gdial-plat-app.c                                                    */
/* ------------------------------------------------------------------ */

void *gdial_plat_application_hide_async(
        const gchar *name, gint id, void *user_data)
{
    (void)name; (void)id; (void)user_data;
    return nullptr;
}

void *gdial_plat_application_resume_async(
        const gchar *name, gint id, void *user_data)
{
    (void)name; (void)id; (void)user_data;
    return nullptr;
}
