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
#include <gio/gio.h>
#include "gdial-config.h"
#include "gdial-plat-app.h"
#include "gdial-os-app.h"
#include "rtdial.hpp"

static void gdial_app_state_cb_default(gint instance_id, GDialAppState state, gpointer user_data);
static GMainContext *g_main_context_ = NULL;
static gdial_plat_application_state_cb gdial_app_state_cb_ = gdial_app_state_cb_default;
static gpointer gdial_app_state_cb_user_data_ = NULL;

static gdial_plat_activation_cb g_activation_cb = NULL;

#define GDIAL_PLAT_APP_ASYNC_CONTEXT_TYPE_COMMON 0
#define GDIAL_PLAT_APP_ASYNC_CONTEXT_TYPE_START  1

typedef struct {
  gint type;
  gchar *name;
  gint instance_id;
  gpointer user_data;
  guint async_gsource;
  gchar *type_str;
} GDialPlatAppAsyncContext;

typedef struct {
  GDialPlatAppAsyncContext common;
  gchar *payload;
  gchar *query;
  gchar *additional_data_url;
} GDialPlatAppStartContext;


static GHashTable *gdial_plat_app_async_contexts = NULL;

static void gdial_app_state_cb_default(gint instance_id, GDialAppState state, gpointer user_data) {
  g_print("instance [%d] state = [%d]\r\n", instance_id, state);
}

static gboolean GSourceFunc_application_start_async_cb(gpointer user_data) {
  GDialPlatAppAsyncContext *app_async_context = (GDialPlatAppAsyncContext *)user_data;
  g_return_val_if_fail(app_async_context->type == GDIAL_PLAT_APP_ASYNC_CONTEXT_TYPE_START, FALSE);

  GDialPlatAppStartContext *app_start_context = (GDialPlatAppStartContext *)app_async_context;
  static gint instance_id_ = 0xACAC0000;
  instance_id_++;

  if (g_strcmp0(app_start_context->common.name, "Netflix") == 0) {
    gdial_plat_application_start(app_start_context->common.name, app_start_context->payload, app_start_context->query, app_start_context->additional_data_url, &app_start_context->common.instance_id);
    gdial_plat_application_state_async(app_async_context->name, app_async_context->instance_id, app_async_context->user_data);
  }
  else if (g_strcmp0(app_start_context->common.name, "Youtube") == 0) {
    gdial_plat_application_start(app_start_context->common.name, app_start_context->payload, app_start_context->query, app_start_context->additional_data_url, &app_start_context->common.instance_id);
    gdial_plat_application_state_async(app_async_context->name, app_async_context->instance_id, app_async_context->user_data);
  }
  else {
    g_warn_if_reached();
  }

  /* do not repeat timeout */
  app_async_context->async_gsource = 0;
  return FALSE;
}

static void GDestroyNotify_async_source_destory(gpointer data) {
  GDialPlatAppAsyncContext *app_async_context = (GDialPlatAppAsyncContext *)data;
  app_async_context->async_gsource = 0;
  g_warn_if_fail(g_hash_table_remove(gdial_plat_app_async_contexts, app_async_context));
}


static void GDialPlatAppAsyncContext_destroy(gpointer data) {
  GDialPlatAppAsyncContext *app_async_context = (GDialPlatAppAsyncContext *)data;
  g_warn_if_fail(app_async_context->async_gsource == 0);
  g_print("GDialPlatAppAsyncContext_destroy(%s)\r\n", app_async_context->type_str);
  g_free(app_async_context->name);
  g_free(app_async_context->type_str);
  app_async_context->user_data = NULL;
  if (app_async_context->type == GDIAL_PLAT_APP_ASYNC_CONTEXT_TYPE_START) {
    GDialPlatAppStartContext *app_start_context = (GDialPlatAppStartContext *)app_async_context;
    g_free(app_start_context->payload);
    g_free(app_start_context->query);
    g_free(app_start_context->additional_data_url);
  }
  g_free(app_async_context);
}

static gboolean GSourceFunc_application_state_async_cb(gpointer user_data) {
  GDialPlatAppAsyncContext *app_async_context = (GDialPlatAppAsyncContext *)user_data;
  g_warn_if_fail(app_async_context->type == GDIAL_PLAT_APP_ASYNC_CONTEXT_TYPE_COMMON);
  GDialAppState state = GDIAL_APP_STATE_MAX;
  gdial_plat_application_state(app_async_context->name, app_async_context->instance_id, &state);
  gdial_app_state_cb_(app_async_context->instance_id, state, app_async_context->user_data);
  /* do not repeat timeout */
  app_async_context->async_gsource = 0;
  return FALSE;
}

static gboolean GSourceFunc_application_stop_async_cb(gpointer user_data) {
  GDialPlatAppAsyncContext *app_async_context = (GDialPlatAppAsyncContext *)user_data;
  g_warn_if_fail(app_async_context->type == GDIAL_PLAT_APP_ASYNC_CONTEXT_TYPE_COMMON);
  gdial_plat_application_stop(app_async_context->name, app_async_context->instance_id);
  gdial_plat_application_state_async(app_async_context->name, app_async_context->instance_id, app_async_context->user_data);
  /* do not repeat timeout */
  app_async_context->async_gsource = 0;
  return FALSE;
}

void gdail_plat_register_activation_cb(gdial_plat_activation_cb cb)
{
  g_activation_cb = cb;
  rtdail_register_activation_cb((rtdial_activation_cb)cb);
}

void gdial_plat_register_friendlyname_cb(gdial_plat_friendlyname_cb cb)
{
  rtdial_register_friendly_name_cb((rtdial_friendlyname_cb)cb);
}

gint gdial_plat_init(GMainContext *main_context) {
  g_return_val_if_fail(main_context != NULL, GDIAL_APP_ERROR_INTERNAL);
  g_return_val_if_fail((g_main_context_ == NULL || g_main_context_ == main_context), GDIAL_APP_ERROR_INTERNAL);
  g_main_context_ = g_main_context_ref(main_context);

  if(!rtdial_init(g_main_context_)) {
      g_print("rtdial_init failed !!!!!\n");
      return GDIAL_APP_ERROR_INTERNAL;
  }

  if (gdial_plat_app_async_contexts == NULL) {
    gdial_plat_app_async_contexts = g_hash_table_new_full(g_direct_hash, g_direct_equal, GDialPlatAppAsyncContext_destroy, /* key,value are same pointer*/NULL);
  }
  else {
    g_hash_table_ref(gdial_plat_app_async_contexts);
  }
  return GDIAL_APP_ERROR_NONE;
}

/*
 * upon return, the app must be in running state. An immediate 2nd invocation of this API
 * for singleton app should not cause a 2nd instance.
 */
GDialAppError gdial_plat_application_start(const gchar *app_name, const gchar *payload, const gchar *query, const gchar *additional_data_url, gint *instance_id) {
  g_return_val_if_fail(app_name != NULL, GDIAL_APP_ERROR_BAD_REQUEST);
  g_return_val_if_fail(instance_id != NULL, GDIAL_APP_ERROR_BAD_REQUEST);
  /*
   * Different app have different cmdline arguments and formats.
   */
  return gdial_os_application_start(app_name, payload, query, additional_data_url, instance_id);
}

void * gdial_plat_application_start_async(const gchar *app_name, const gchar *payload, const gchar *query, const gchar *additional_data_url, void *user_data) {
  g_return_val_if_fail(app_name != NULL && strlen(app_name), NULL);
  g_return_val_if_fail(gdial_plat_app_async_contexts != NULL, NULL);

  GDialPlatAppStartContext *app_async_context = (GDialPlatAppStartContext *)malloc(sizeof(*app_async_context));
  app_async_context->common.type = GDIAL_PLAT_APP_ASYNC_CONTEXT_TYPE_START;
  app_async_context->common.name = g_strdup(app_name);
  app_async_context->common.instance_id = GDIAL_APP_INSTANCE_NONE;
  app_async_context->common.user_data = user_data;
  app_async_context->common.type_str = g_strconcat(app_name, ":start_async", NULL);
  app_async_context->payload = g_strdup(payload);
  app_async_context->query = g_strdup(query);
  app_async_context->additional_data_url = g_strdup(additional_data_url);
  g_hash_table_insert(gdial_plat_app_async_contexts, app_async_context, app_async_context);
  app_async_context->common.async_gsource = g_timeout_add_full(G_PRIORITY_DEFAULT, 1/*milli*/, GSourceFunc_application_start_async_cb, app_async_context, GDestroyNotify_async_source_destory);
  return app_async_context;
}

GDialAppError gdial_plat_application_hide(const gchar *app_name, gint instance_id) {
  g_return_val_if_fail(app_name != NULL, GDIAL_APP_ERROR_BAD_REQUEST);
  g_return_val_if_fail(instance_id != GDIAL_APP_INSTANCE_NONE, GDIAL_APP_ERROR_BAD_REQUEST);

  return gdial_os_application_hide(app_name, instance_id);
}

GDialAppError gdial_plat_application_resume(const gchar *app_name, gint instance_id) {
  g_return_val_if_fail(app_name != NULL, GDIAL_APP_ERROR_BAD_REQUEST);
  g_return_val_if_fail(instance_id != GDIAL_APP_INSTANCE_NONE, GDIAL_APP_ERROR_BAD_REQUEST);

  return gdial_os_application_resume(app_name, instance_id);
}

GDialAppError gdial_plat_application_stop(const gchar *app_name, gint instance_id) {
  g_return_val_if_fail(app_name != NULL, GDIAL_APP_ERROR_BAD_REQUEST);
  g_return_val_if_fail(instance_id != GDIAL_APP_INSTANCE_NONE, GDIAL_APP_ERROR_BAD_REQUEST);

  return gdial_os_application_stop(app_name, instance_id);
}

void *gdial_plat_application_stop_async(const gchar *app_name, gint instance_id, void *user_data) {
  g_return_val_if_fail(app_name != NULL && strlen(app_name), NULL);
  g_return_val_if_fail(gdial_plat_app_async_contexts != NULL, NULL);

  GDialPlatAppAsyncContext *app_async_context = (GDialPlatAppAsyncContext *)malloc(sizeof(*app_async_context));
  app_async_context->type = GDIAL_PLAT_APP_ASYNC_CONTEXT_TYPE_COMMON;
  app_async_context->name = g_strdup(app_name);
  app_async_context->instance_id = instance_id;
  app_async_context->user_data = user_data;
  app_async_context->type_str = g_strconcat(app_name, ":stop_async", NULL);
  app_async_context->async_gsource = g_timeout_add_full(G_PRIORITY_DEFAULT, 1/*milli*/, GSourceFunc_application_stop_async_cb, app_async_context, GDestroyNotify_async_source_destory);
  g_hash_table_insert(gdial_plat_app_async_contexts, app_async_context, app_async_context);
  return app_async_context;
}

GDialAppError gdial_plat_application_state(const gchar *app_name, gint instance_id, GDialAppState *state) {
  g_print("GDIAL : Inside gdial_plat_application_state\n");
  g_return_val_if_fail(app_name != NULL, GDIAL_APP_ERROR_BAD_REQUEST);
  g_return_val_if_fail(state != NULL, GDIAL_APP_ERROR_BAD_REQUEST);
  g_return_val_if_fail(instance_id != GDIAL_APP_INSTANCE_NONE, GDIAL_APP_ERROR_BAD_REQUEST);

  return gdial_os_application_state(app_name, instance_id, state);
}

void * gdial_plat_application_state_async(const gchar *app_name, gint instance_id, void *user_data) {
  g_return_val_if_fail(app_name != NULL && strlen(app_name), NULL);
  g_return_val_if_fail(gdial_plat_app_async_contexts != NULL, NULL);
  GDialPlatAppAsyncContext *app_async_context = (GDialPlatAppAsyncContext *)malloc(sizeof(*app_async_context));
  app_async_context->type = GDIAL_PLAT_APP_ASYNC_CONTEXT_TYPE_COMMON;
  app_async_context->name = g_strdup(app_name);
  app_async_context->instance_id = instance_id;
  app_async_context->user_data = user_data;
  app_async_context->type_str = g_strconcat(app_name, ":state_async", NULL);
  app_async_context->async_gsource = g_timeout_add_full(G_PRIORITY_DEFAULT, 1/*milli*/, GSourceFunc_application_state_async_cb, app_async_context, GDestroyNotify_async_source_destory);
  g_hash_table_insert(gdial_plat_app_async_contexts, app_async_context, app_async_context);
  return app_async_context;
}

void gdial_plat_application_set_state_cb(gdial_plat_application_state_cb cb, gpointer user_data) {
  gdial_app_state_cb_ = cb;
  gdial_app_state_cb_user_data_ = user_data;
}

void gdial_plat_application_remove_async_source(void *async_source) {
  GDialPlatAppAsyncContext *app_async_context = (GDialPlatAppAsyncContext *)async_source;
  g_warn_if_fail((app_async_context->async_gsource == 0 && g_hash_table_lookup(gdial_plat_app_async_contexts, app_async_context) == NULL) ||
                 (app_async_context->async_gsource != 0 && g_hash_table_lookup(gdial_plat_app_async_contexts, app_async_context) != NULL));
  /*
   * when source is removed, the GDestoyNotifier will destroy the context.
   */
  guint async_gsource = app_async_context->async_gsource;
  g_source_remove(async_gsource);
  g_warn_if_fail(g_hash_table_remove(gdial_plat_app_async_contexts, app_async_context) == FALSE);
}

void gdial_plat_term() {
  rtdial_term();
  g_main_context_unref(g_main_context_);
  g_warn_if_fail(g_hash_table_size(gdial_plat_app_async_contexts) == 0);
  g_hash_table_unref(gdial_plat_app_async_contexts);
  return;
}

GDialAppError gdial_plat_system_app(GHashTable *query) {
  return gdial_os_system_app(query);
}
