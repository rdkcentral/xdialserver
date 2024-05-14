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

#ifndef GDIAL_APP_H_
#define GDIAL_APP_H_

#include <glib.h>
#include <glib-object.h>

G_BEGIN_DECLS

#define GDIAL_TYPE_APP            (gdial_app_get_type ())
G_DECLARE_FINAL_TYPE (GDialApp, gdial_app, GDIAL, APP, GObject)

#define GDIAL_APP_NAME "name"
#define GDIAL_APP_STATE "state"
#define GDIAL_APP_INSTANCE_ID "instance-id"

#define GDIAL_APP_GET_STATE(app) ((app)->state)

#define GDIAL_APP_INSTANCE_NONE (G_MAXINT-1)

typedef enum {
  GDIAL_APP_STATE_STOPPED = 0,
  GDIAL_APP_STATE_HIDE,
  GDIAL_APP_STATE_RUNNING,
  GDIAL_APP_STATE_MAX
} GDialAppState;


typedef enum {
  GDIAL_APP_ERROR_NONE = 0,
  GDIAL_APP_ERROR_NOT_IMPLEMENTED = GDIAL_APP_STATE_MAX,
  GDIAL_APP_ERROR_FORBIDDEN,
  GDIAL_APP_ERROR_UNAUTH,

  GDIAL_APP_ERROR_IMPL_ = 0x1000,
  GDIAL_APP_ERROR_BAD_REQUEST,
  GDIAL_APP_ERROR_UNAVAILABLE,
  GDIAL_APP_ERROR_INVALID,
  GDIAL_APP_ERROR_INTERNAL,
  GDIAL_APP_ERROR_MAX,
} GDialAppError;

typedef gint gdial_instance_id_t;

struct _GDialApp {
  GObject parent;
  gchar *name;
  GDialAppState state;
  gint instance_id;
  const gchar *instance_sid;
};

GType gdial_app_get_type (void);

GDialApp* gdial_app_new(const char *app_name);

GDialAppError gdial_app_start(GDialApp *app, const gchar *payload, const gchar *query, const gchar *additional_data_url, gpointer state_cb_data);
GDialAppError gdial_app_hide(GDialApp *app);
GDialAppError gdial_app_resume(GDialApp *app);
GDialAppError gdial_app_stop(GDialApp *app);
GDialAppError gdial_app_state(GDialApp *app);
const gchar *gdial_app_state_to_string(GDialAppState state);

gint gdial_app_get_instance_id(GDialApp *app);

GDialApp *gdial_app_find_instance_by_name(const gchar *app_name);
GDialApp *gdial_app_find_instance_by_instance_id(gint instance_id);
gchar *gdial_app_get_additional_dial_data_by_key(GDialApp *app, const gchar *key);
void gdial_app_set_additional_dial_data(GDialApp *app, GHashTable *additional_dial_data);
GHashTable *gdial_app_get_additional_dial_data(GDialApp *app);
void gdial_app_refresh_additional_dial_data(GDialApp *app);
void gdial_app_clear_additional_dial_data(GDialApp *app);
gchar * gdial_app_state_response_new(GDialApp *app, const gchar *dial_ver, const gchar *client_dial_version, const gchar *xmlns, int *len);

GDIAL_STATIC gboolean gdial_app_write_additional_dial_data(const gchar *app_name, const gchar *data, size_t length);
GDIAL_STATIC gboolean gdial_app_read_additional_dial_data(const gchar *app_name, gchar **data, size_t *length);
GDIAL_STATIC gboolean gdial_app_remove_additional_dial_data_file(const gchar *app_name);

gchar *gdial_app_get_launch_payload(GDialApp *app);
void gdial_app_set_launch_payload(GDialApp *app, const gchar *payload);

void gdial_app_force_shutdown(GDialApp *app);

G_END_DECLS

#endif
