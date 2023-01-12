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
#include <gio/gio.h>
#include <stdlib.h>
#include <libxml/tree.h>

#include "gdial-config.h"
#include "gdial-util.h"
#include "gdial-plat-app.h"
#include "gdial-app.h"


typedef struct _GDialAppPrivate {
  gpointer state_cb_data;
  GHashTable *additional_dial_data;
  gchar *payload;
} GDialAppPrivate;

enum {
  PROP_0,
  PROP_NAME,
  PROP_STATE,
  PROP_INSTANCE_ID,
  N_PROPERTIES
};

enum {
  SIGNAL_STATE_CHANGED,
  N_SIGNALS
};

static  GList *application_instances_ = NULL;

static guint gdial_app_signals[N_SIGNALS] =  {0};

G_DEFINE_TYPE_WITH_PRIVATE(GDialApp, gdial_app, G_TYPE_OBJECT)

static gint GCompareFunc_match_instance_app_name(gconstpointer a, gconstpointer b) {
  GDialApp *app = (GDialApp *)a;
  return g_strcmp0(app->name, b);
}

static gint GCompareFunc_match_instance_instance_id(gconstpointer a, gconstpointer b) {
  GDialApp *app = (GDialApp *)a;
  return (app->instance_id == *((gint *)b)) ? 0 : 1;
}

static void gdial_app_dispose(GObject *gobject) {
  GDialApp *app = GDIAL_APP(gobject);
  GDialAppPrivate *priv = gdial_app_get_instance_private(GDIAL_APP(gobject));
  if (priv->payload) {
    g_free(priv->payload);
    priv->payload = NULL;
  }
  if (app->name) {
    g_free(app->name);
    app->name = NULL;
  }

  if (priv->additional_dial_data) {
    g_hash_table_destroy(priv->additional_dial_data);
    priv->additional_dial_data = NULL;
  }

  application_instances_ = g_list_remove(application_instances_, gobject);
  g_print("After dispose has %d app instances created\r\n", g_list_length(application_instances_));

  G_OBJECT_CLASS (gdial_app_parent_class)->dispose (gobject);
}

static void gdial_plat_app_state_cb(gint instance_id, GDialAppState state, void *user_data) {
  g_return_if_fail (instance_id != GDIAL_APP_INSTANCE_NONE);
  GDialApp *app = gdial_app_find_instance_by_instance_id(instance_id);
  g_return_if_fail (app != NULL);
  GDialAppPrivate *priv = gdial_app_get_instance_private(app);
  g_signal_emit(app, gdial_app_signals[SIGNAL_STATE_CHANGED], 0, app, priv->state_cb_data);
}

static void gdial_app_get_property (GObject *object, guint property_id, GValue *value, GParamSpec *pspec) {
  GDialApp *app = GDIAL_APP(object);

  switch (property_id) {
    case PROP_NAME:
      g_value_set_string(value, app->name);
      break;
    case PROP_STATE:
      g_value_set_uint(value, app->state);
      break;
    case PROP_INSTANCE_ID:
      g_value_set_int(value, app->instance_id);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
      break;
  }
}

static void gdial_app_set_property (GObject *object, guint property_id, const GValue *value, GParamSpec *pspec) {
  GDialApp *self = GDIAL_APP(object);

  switch (property_id) {
    case PROP_NAME:
      self->name = g_value_dup_string(value);
      break;
    case PROP_STATE:
      self->state = g_value_get_uint(value);
      break;
    case PROP_INSTANCE_ID:
      self->instance_id = g_value_get_int(value);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
      break;
  }
}

static void gdial_app_class_init (GDialAppClass *klass) {

  GObjectClass *gobject_class = (GObjectClass *)klass;

  gobject_class->dispose = gdial_app_dispose;
  gobject_class->get_property = gdial_app_get_property;
  gobject_class->set_property = gdial_app_set_property;

  g_object_class_install_property (gobject_class, PROP_NAME,
          g_param_spec_string("name",
                  NULL,
                  "The Applicaton's name",
                  "ABC", G_PARAM_READWRITE));


  g_object_class_install_property (gobject_class, PROP_STATE,
          g_param_spec_uint ("state",
                  NULL,
                  "The Applicaton's run/hide/stop state",
                  GDIAL_APP_STATE_STOPPED, GDIAL_APP_STATE_RUNNING,
                  GDIAL_APP_STATE_STOPPED, G_PARAM_READWRITE));

  g_object_class_install_property (gobject_class, PROP_INSTANCE_ID,
          g_param_spec_int ("instance-id",
                  NULL,
                  "The runtime id of the running application",
                  1, G_MAXINT-1,
                  GDIAL_APP_INSTANCE_NONE, G_PARAM_READABLE));

  gdial_app_signals[SIGNAL_STATE_CHANGED] =
    g_signal_new ("state-changed",
            G_OBJECT_CLASS_TYPE (gobject_class),
            0, /* flag */
            0, /* class offset */
            NULL, NULL, /* accumulator, accu_data */
            NULL, /* c_marshaller, use default */
            G_TYPE_NONE, 1, /* return type, arg num */
            G_TYPE_POINTER); /* arg types */

  gdial_plat_init(g_main_context_default());
  gdial_plat_application_set_state_cb(gdial_plat_app_state_cb, NULL);
}

static void gdial_app_init(GDialApp *self) {
  GDialAppPrivate *priv = gdial_app_get_instance_private(self);
  priv->payload = NULL;
  priv->state_cb_data = NULL;
  application_instances_ = g_list_prepend(application_instances_, self);
}

GDialApp *gdial_app_new(const gchar *app_name) {
  GDialApp *app = (GDialApp*)g_object_new(GDIAL_TYPE_APP, GDIAL_APP_NAME, app_name, NULL);
  g_print("After create has %d app %s instances created\r\n", g_list_length(application_instances_), app_name);
  gdial_app_refresh_additional_dial_data(app);
  return app;
};

GDialAppError gdial_app_start(GDialApp *app, const gchar *payload, const gchar *query, const gchar *additional_data_url, gpointer state_cb_data) {
  g_return_val_if_fail (GDIAL_IS_APP (app), GDIAL_APP_ERROR_BAD_REQUEST);

  GDialAppPrivate *priv = gdial_app_get_instance_private(app);
  priv->state_cb_data = state_cb_data;
  GDialAppError app_err = gdial_plat_application_start(app->name, payload, query, additional_data_url, &app->instance_id);
  if (app_err == GDIAL_APP_ERROR_NONE || (strcmp("system", app->name) != 0 && app->instance_id != GDIAL_APP_INSTANCE_NONE)) {
    gdial_plat_application_state_async(app->name, app->instance_id, app);
    app_err = gdial_plat_application_state(app->name, app->instance_id, &app->state);
    g_warn_if_fail(app->state == GDIAL_APP_STATE_RUNNING);
  }
  else {
    app->state = GDIAL_APP_STATE_STOPPED;
  }

  return app_err;
}

GDialAppError gdial_app_hide(GDialApp *app) {
  g_return_val_if_fail (GDIAL_IS_APP(app), GDIAL_APP_ERROR_BAD_REQUEST);
  g_return_val_if_fail (app->name != NULL, GDIAL_APP_ERROR_BAD_REQUEST);
  g_return_val_if_fail (app->instance_id != GDIAL_APP_INSTANCE_NONE, GDIAL_APP_ERROR_BAD_REQUEST);

  GDialAppError app_err =  gdial_plat_application_hide(app->name, app->instance_id);
  if (app_err == GDIAL_APP_ERROR_NONE) {
    app_err = gdial_plat_application_state(app->name, app->instance_id, &app->state);
  }
  else {
  }
  return app_err;
}

GDialAppError gdial_app_resume(GDialApp *app) {
  g_return_val_if_fail (GDIAL_IS_APP (app), GDIAL_APP_ERROR_BAD_REQUEST);
  g_return_val_if_fail (app->name != NULL, GDIAL_APP_ERROR_BAD_REQUEST);
  g_return_val_if_fail (app->instance_id != GDIAL_APP_INSTANCE_NONE, GDIAL_APP_ERROR_BAD_REQUEST);

  GDialAppError app_err =  gdial_plat_application_resume(app->name, app->instance_id);
  if (app_err == GDIAL_APP_ERROR_NONE) {
    app_err = gdial_plat_application_state(app->name, app->instance_id, &app->state);
  }
  else {
  }
  return app_err;
}

GDialAppError gdial_app_stop(GDialApp *app) {
  g_return_val_if_fail (GDIAL_IS_APP (app), GDIAL_APP_ERROR_INTERNAL);
  g_return_val_if_fail (app->name != NULL, GDIAL_APP_ERROR_INTERNAL);
  GDialAppError app_err =  gdial_plat_application_stop(app->name, app->instance_id);
  if (app_err == GDIAL_APP_ERROR_NONE) {
    app_err = gdial_plat_application_state(app->name, app->instance_id, &app->state);
  }
  else {
  }
  return app_err;
}

GDialAppError gdial_app_state(GDialApp *app) {
  g_return_val_if_fail (GDIAL_IS_APP (app), GDIAL_APP_ERROR_INTERNAL);
  g_return_val_if_fail (app->name != NULL, GDIAL_APP_ERROR_INTERNAL);

  if (app->instance_id == GDIAL_APP_INSTANCE_NONE) {
    app->state = GDIAL_APP_STATE_STOPPED;
    return GDIAL_APP_ERROR_NONE;
  }

  GDialAppState app_state = GDIAL_APP_STATE_MAX;
  GDialAppError app_err = gdial_plat_application_state(app->name, app->instance_id, &app_state);
  if (app_err == GDIAL_APP_ERROR_NONE) {
    app->state = app_state;
  }
  return app_err;
}

const gchar *gdial_app_state_to_string(GDialAppState state) {
  switch(state) {
    case GDIAL_APP_STATE_STOPPED: return "stopped";
    case GDIAL_APP_STATE_RUNNING: return "running";
    case GDIAL_APP_STATE_HIDE:    return "hidden";
    case GDIAL_APP_STATE_MAX:
    default:
      return NULL;
  }
}

void gdial_app_force_shutdown(GDialApp *app) {
  g_warn_if_reached();
}

gchar *gdial_app_get_launch_payload(GDialApp *app) {
  GDialAppPrivate *priv = gdial_app_get_instance_private(app);
  return priv->payload;
}

void gdial_app_set_launch_payload(GDialApp *app, const gchar *payload) {
  GDialAppPrivate *priv = gdial_app_get_instance_private(app);
  if (priv->payload) {
    g_free(priv->payload);
    priv->payload = NULL;
  }
  if (payload) {
    priv->payload = g_strdup(payload);
  }
}

gchar *gdial_app_get_additional_dial_data_by_key(GDialApp *app, const gchar *key) {
  g_return_val_if_fail(app && app->name && strlen(app->name) && key, NULL);
  GDialAppPrivate *priv = gdial_app_get_instance_private(app);
  if (priv->additional_dial_data) {
    return g_hash_table_lookup(priv->additional_dial_data, key);
  }
  else {
    return NULL;
  }
}

void gdial_app_set_additional_dial_data(GDialApp *app, GHashTable *additional_dial_data) {
  g_return_if_fail(app && app->name && strlen(app->name));

  GDialAppPrivate *priv = gdial_app_get_instance_private(app);
  if(priv->additional_dial_data) {
    g_hash_table_destroy(priv->additional_dial_data);
  }
  priv->additional_dial_data = gdial_util_str_str_hashtable_dup(additional_dial_data);
  /* cache the additional_dial_data */
  size_t length = 0;
  gchar *query_str = gdial_util_str_str_hashtable_to_string(additional_dial_data, NULL, TRUE, &length);
  if (query_str) {
    gdial_app_write_additional_dial_data(app->name, query_str, length);
  }
  g_free(query_str);
}

GHashTable *gdial_app_get_additional_dial_data(GDialApp *app) {
  g_return_val_if_fail(app && app->name && strlen(app->name), NULL);
  GDialAppPrivate *priv = gdial_app_get_instance_private(app);
  return g_hash_table_ref(priv->additional_dial_data);
}

void gdial_app_refresh_additional_dial_data(GDialApp *app) {
  g_return_if_fail(app && app->name && strlen(app->name));

  GDialAppPrivate *priv = gdial_app_get_instance_private(app);
  if(priv->additional_dial_data) {
    g_hash_table_destroy(priv->additional_dial_data);
  }
  priv->additional_dial_data = g_hash_table_new_full(g_str_hash, g_str_equal, g_free, g_free);
  gchar *data = NULL;
  size_t length = 0;
  if (gdial_app_read_additional_dial_data(app->name, &data, &length)) {
    if (data) {
      /* we are ready to convert to hashtable*/
      gdial_util_str_str_hashtable_from_string(data, length, priv->additional_dial_data);
      g_print("gdial_app_refresh_additional_dial_data [%s]\r\n", data);
      g_free(data);
    }
  }
}

void gdial_app_clear_additional_dial_data(GDialApp *app) {
  g_return_if_fail(app && app->name && strlen(app->name));

  GDialAppPrivate *priv = gdial_app_get_instance_private(app);
  if(priv->additional_dial_data) {
    g_hash_table_remove_all(priv->additional_dial_data);
  }
  gdial_app_remove_additional_dial_data_file(app->name);
}

GDialApp *gdial_app_find_instance_by_name(const gchar *app_name) {
  g_return_val_if_fail(app_name != NULL, NULL);
  GList *found = g_list_find_custom(application_instances_, app_name, GCompareFunc_match_instance_app_name);
  if (found) {
    return (GDialApp *)found->data;
  }
  return NULL;
}

GDialApp *gdial_app_find_instance_by_instance_id(gint instance_id) {
  g_return_val_if_fail(instance_id != GDIAL_APP_INSTANCE_NONE, NULL);
  GList *found = g_list_find_custom(application_instances_, &instance_id, GCompareFunc_match_instance_instance_id);
  if (found) {
    return (GDialApp *)found->data;
  }
  return NULL;
}

GDIAL_STATIC gboolean gdial_app_write_additional_dial_data(const gchar *app_name, const gchar *data, size_t length) {
  gboolean result = FALSE;
  GError *err = NULL;
  gchar *filename = g_build_filename(GDIAL_APP_DIAL_DATA_DIR, app_name, NULL);
  GFile *gfile = g_file_new_for_path(filename);
  if (!g_file_query_exists(gfile, NULL) || g_file_delete(gfile, NULL, &err) ) {
    GFileIOStream *gfile_ios = g_file_create_readwrite(gfile, G_FILE_CREATE_PRIVATE, NULL, &err);
    if (gfile_ios) {
      if (g_output_stream_write(g_io_stream_get_output_stream(G_IO_STREAM(gfile_ios)), data, length, NULL, &err) == length) {
        result = TRUE;
      }
      else {
        GDIAL_GERROR_CHECK_AND_FREE(err, "Unable to write file properly");
      }
      g_object_unref(gfile_ios);
    }
    else {
      g_warn_if_reached();
      GDIAL_GERROR_CHECK_AND_FREE(err, "Cannot create file");
    }
  }
  else {
    GDIAL_GERROR_CHECK_AND_FREE(err, "Cannot delete file");
  }
  g_object_unref(gfile);
  g_free(filename);

  return result;
}

GDIAL_STATIC gboolean gdial_app_read_additional_dial_data(const gchar *app_name, gchar **data, size_t *length) {
  gboolean result = FALSE;
  GError *err = NULL;

  *data = NULL; *length = 0;

  gchar *filename = g_build_filename(GDIAL_APP_DIAL_DATA_DIR, app_name, NULL);
  g_return_val_if_fail(filename && strlen(filename), FALSE);

  GFile *gfile = g_file_new_for_path(filename);
  if (g_file_query_exists(gfile, NULL)) {
    GFileInfo *gfile_info = g_file_query_info(gfile, G_FILE_ATTRIBUTE_STANDARD_SIZE, G_FILE_QUERY_INFO_NONE, NULL, &err);
    if (gfile_info && err == NULL) {
      goffset fsize = g_file_info_get_size(gfile_info);
      *data = malloc(fsize+1);
      GFileIOStream *gfile_ios = g_file_open_readwrite(gfile, NULL, &err);
      if (gfile_ios) {
      if (g_input_stream_read(g_io_stream_get_input_stream(G_IO_STREAM(gfile_ios)), *data, fsize, NULL, &err) == fsize) {
        (*data)[fsize] = 0;
        *length = fsize;
        result = TRUE;
      }
      g_object_unref(gfile_ios);
      }
      else {
        g_printerr("file %s file is not readable\n", filename);
      }
      g_object_unref(gfile_info);
    }
    else {
      g_warn_if_reached();
    }
  }
  else {
    g_printerr("file %s does not exist\n", filename);
  }

  g_object_unref(gfile);
  g_free(filename);

  return result;
}

GDIAL_STATIC gboolean gdial_app_remove_additional_dial_data_file(const gchar *app_name) {
  gboolean result = FALSE;
  gchar *filename = g_build_filename(GDIAL_APP_DIAL_DATA_DIR, app_name, NULL);
  GFile *gfile = g_file_new_for_path(filename);
  if (g_file_query_exists(gfile, NULL)) {
    GError *err = NULL;
    if (g_file_delete(gfile, NULL, &err)) {
      result = TRUE;
    }
    else {
      GDIAL_GERROR_CHECK_AND_FREE(err, "Cannot delete file");
    }
  }
  g_object_unref(gfile);
  g_free(filename);

  return result;
}

gchar * gdial_app_state_response_new(GDialApp *app, const gchar *dial_ver, const gchar *xmlns, int *len)
{
  g_return_val_if_fail(app && app->name && strlen(app->name), NULL);
  g_return_val_if_fail(dial_ver && xmlns && len, NULL);
  GDialAppPrivate *priv = gdial_app_get_instance_private(app);

  xmlDocPtr xdoc = NULL;
  xdoc = xmlNewDoc(BAD_CAST "1.0");
  xmlNodePtr nservice = xmlNewNode(NULL, BAD_CAST "service");
  xmlDocSetRootElement(xdoc, nservice); {
    xmlNewProp(nservice, BAD_CAST "xmlns", BAD_CAST xmlns);
    xmlNewProp(nservice, BAD_CAST "dialVer", BAD_CAST dial_ver);
  }
  xmlNewChild(nservice, NULL, BAD_CAST "name", BAD_CAST app->name);
  xmlNodePtr noptions = xmlNewChild(nservice, NULL, BAD_CAST "options", BAD_CAST NULL); {
    xmlNewProp(noptions, BAD_CAST "allowStop", BAD_CAST "true");
  }
  xmlNewChild(nservice, NULL, BAD_CAST "state", BAD_CAST gdial_app_state_to_string(app->state));
  if (app->state != GDIAL_APP_STATE_STOPPED) {
    xmlNodePtr nlink  = xmlNewChild(nservice, NULL, BAD_CAST "link", BAD_CAST NULL);
    xmlNewProp(nlink, BAD_CAST "rel", BAD_CAST "run");
    xmlNewProp(nlink, BAD_CAST "href", BAD_CAST "run");
  }
  xmlNodePtr naddtnl  = xmlNewChild(nservice, NULL, BAD_CAST "additionalData", BAD_CAST NULL); {
    GHashTableIter iter;
    gpointer key, value;
    g_hash_table_iter_init(&iter, priv->additional_dial_data);
    while (g_hash_table_iter_next(&iter, &key, &value)) {
      xmlNewChild(naddtnl, NULL, BAD_CAST (gchar*)key, BAD_CAST (gchar*)value);
    }
  }
  xmlChar *app_state_response = NULL;
  xmlDocDumpMemory(xdoc, &app_state_response, len);
  xmlFreeDoc(xdoc);
  return (gchar *)app_state_response;
}
