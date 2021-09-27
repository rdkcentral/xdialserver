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

#include <string.h>
#include <stdlib.h>
#include <glib.h>
#include <gio/gio.h>
#include "gdial-config.h"
#include "gdial-plat-app.h"
#include "gdial-app.h"


static void gdial_app_state_cb_default(gint instance_id, GDialAppState state, gpointer user_data);
static GMainContext *g_main_context_ = NULL;
static gdial_plat_application_state_cb gdial_app_state_cb_ = gdial_app_state_cb_default;
static gpointer gdial_app_state_cb_user_data_ = NULL;
static gdial_plat_activation_cb g_activation_cb = NULL;
static GHashTable *appmgr_app_table = NULL;

typedef struct AppContext_ {
  GDialAppState state;
} AppContext_t;

static AppContext_t *find_appcontext_by_name(const gchar *app_name) {
  g_return_val_if_fail(app_name != NULL, NULL);
  return (AppContext_t *)g_hash_table_lookup(appmgr_app_table, app_name);
}

static void gdial_app_state_cb_default(gint instance_id, GDialAppState state, gpointer user_data) {
  g_print("instance [%d] state = [%d]\r\n", instance_id, state);
}

void gdail_plat_register_activation_cb(gdial_plat_activation_cb cb)
{
  g_activation_cb = cb;
}

gint gdial_plat_init(GMainContext *main_context) {
  g_return_val_if_fail(main_context != NULL, GDIAL_APP_ERROR_INTERNAL);
  g_return_val_if_fail((g_main_context_ == NULL || g_main_context_ == main_context), GDIAL_APP_ERROR_INTERNAL);

  if (appmgr_app_table == NULL) appmgr_app_table = g_hash_table_new_full(g_str_hash, g_str_equal, g_free, g_free);
  else g_hash_table_ref(appmgr_app_table);

  g_main_context_ = g_main_context_ref(main_context);

  return GDIAL_APP_ERROR_NONE;
}

/*
 * upon return, the app must be in running state. An immediate 2nd invocation of this API
 * for singleton app should not cause a 2nd instance.
 */
GDialAppError gdial_plat_application_start(const gchar *app_name, const gchar *payload, const gchar *query, const gchar *additional_data_url, gint *instance_id) {
  g_return_val_if_fail(app_name != NULL, GDIAL_APP_ERROR_BAD_REQUEST);
  g_return_val_if_fail(instance_id != NULL, GDIAL_APP_ERROR_BAD_REQUEST);

  AppContext_t *appCtxt  = find_appcontext_by_name(app_name);
  if (appCtxt == NULL) {
    appCtxt = (AppContext_t *)malloc(sizeof(*appCtxt));
    g_hash_table_insert(appmgr_app_table,  g_strdup(app_name), appCtxt);
  }

  appCtxt->state = GDIAL_APP_STATE_RUNNING;
  return GDIAL_APP_ERROR_NONE;
}

GDialAppError gdial_plat_application_hide(const gchar *app_name, gint instance_id) {
  g_return_val_if_fail(app_name != NULL, GDIAL_APP_ERROR_BAD_REQUEST);
  g_return_val_if_fail(instance_id != GDIAL_APP_INSTANCE_NONE, GDIAL_APP_ERROR_BAD_REQUEST);

  AppContext_t *appCtxt  = find_appcontext_by_name(app_name);
  if (appCtxt != NULL) {
    appCtxt->state = GDIAL_APP_STATE_HIDE;
    return GDIAL_APP_ERROR_NONE;
  }
  else {
    return GDIAL_APP_ERROR_BAD_REQUEST;
  }
}

GDialAppError gdial_plat_application_resume(const gchar *app_name, gint instance_id) {
  g_return_val_if_fail(app_name != NULL, GDIAL_APP_ERROR_BAD_REQUEST);
  g_return_val_if_fail(instance_id != GDIAL_APP_INSTANCE_NONE, GDIAL_APP_ERROR_BAD_REQUEST);

  AppContext_t *appCtxt  = find_appcontext_by_name(app_name);
  if (appCtxt != NULL) {
    appCtxt->state = GDIAL_APP_STATE_RUNNING;
    return GDIAL_APP_ERROR_NONE;
  }
  else {
    return GDIAL_APP_ERROR_BAD_REQUEST;
  }
}

GDialAppError gdial_plat_application_stop(const gchar *app_name, gint instance_id) {
  g_return_val_if_fail(app_name != NULL, GDIAL_APP_ERROR_BAD_REQUEST);
  g_return_val_if_fail(instance_id != GDIAL_APP_INSTANCE_NONE, GDIAL_APP_ERROR_BAD_REQUEST);

  gboolean found  = g_hash_table_remove(appmgr_app_table, (gconstpointer)app_name);
  return found ? GDIAL_APP_ERROR_NONE : GDIAL_APP_ERROR_BAD_REQUEST;
}

GDialAppError gdial_plat_application_state(const gchar *app_name, gint instance_id, GDialAppState *state) {
  g_print("GDIAL : Inside gdial_plat_application_state\n");
  g_return_val_if_fail(app_name != NULL, GDIAL_APP_ERROR_BAD_REQUEST);
  g_return_val_if_fail(state != NULL, GDIAL_APP_ERROR_BAD_REQUEST);
  g_return_val_if_fail(instance_id != GDIAL_APP_INSTANCE_NONE, GDIAL_APP_ERROR_BAD_REQUEST);

  AppContext_t *appCtxt  = find_appcontext_by_name(app_name);
  if (appCtxt != NULL) {
    *state = appCtxt->state;
  }
  else {
    *state = GDIAL_APP_STATE_STOPPED;
  }

  return GDIAL_APP_ERROR_NONE;
}

void * gdial_plat_application_state_async(const gchar *app_name, gint instance_id, void *user_data) {
  GDialAppState state = GDIAL_APP_STATE_MAX;
  gdial_plat_application_state(app_name, instance_id, &state);
  return NULL;
}

void gdial_plat_application_set_state_cb(gdial_plat_application_state_cb cb, gpointer user_data) {
  gdial_app_state_cb_ = cb;
  gdial_app_state_cb_user_data_ = user_data;
}

void gdial_plat_term() {
  g_main_context_unref(g_main_context_);
  if (appmgr_app_table) g_hash_table_unref(appmgr_app_table);
  return;
}
