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

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <netinet/in.h>
#include <sys/socket.h>

#include <glib.h>
#include <libsoup/soup.h>
#include <libgssdp/gssdp.h>

#include "gdial-rest.h"
#include "gdial-util.h"
#include "gdial-debug.h"

#include "gdial-plat-util.h"
#include "gdial-plat-dev.h"
#include "gdial-plat-app.h"
#include "gdial-rest-builder.h"

typedef struct _GDialRestServerPrivate {
  GList *registered_apps;
  SoupServer *soup_instance;
  SoupServer *local_soup_instance;
} GDialRestServerPrivate;

enum {
  PROP_0,
  PROP_SOUP_INSTANCE,
  PROP_LOCAL_SOUP_INSTANCE,
  PROP_ENABLE,
  N_PROPERTIES
};

enum {
  SIGNAL_INVALID_URI,
  SIGNAL_GMAINLOOP_QUIT,
  SIGNAL_REST_ENABLE,
  N_SIGNALS
};

#define GDIAL_MERGE_URL_AND_BODY_QUERY 0

static guint gdial_rest_server_signals[N_SIGNALS] =  {0};

G_DEFINE_TYPE_WITH_PRIVATE(GDialRestServer, gdial_rest_server, G_TYPE_OBJECT)

#define GDIAL_SOUP_MESSAGE_PATH_HAS_ROOT

static gboolean gdial_soup_message_security_check(SoupMessage *msg) {
  g_return_val_if_fail(msg != NULL, FALSE);
  SoupURI *uri = soup_message_get_uri(msg);
  g_return_val_if_fail(uri != NULL && SOUP_URI_VALID_FOR_HTTP(uri), FALSE);

  return TRUE;
}

static void gdial_soup_message_set_http_error(SoupMessage *msg, guint state_code) {
  g_printerr("%s::uri=%s::state_code=%d\r\n", __FUNCTION__, soup_uri_get_path(soup_message_get_uri(msg)), state_code);
  soup_message_headers_replace(msg->response_headers, "Connection", "close");
  soup_message_set_status(msg, state_code);
  return;
}

#define gdial_rest_server_http_print_and_return_if_fail(expr, msg, state, fmt, merr) \
{\
  if (!(expr)) {\
    g_warn_msg_if_fail(expr, fmt, merr);\
    gdial_soup_message_set_http_error(msg, state);\
    return;\
  }\
}

#define gdial_rest_server_http_return_if_fail(expr, msg, state) \
{\
  if (!(expr)) {\
    g_warn_if_fail(expr);\
    gdial_soup_message_set_http_error(msg, state);\
    return;\
  }\
}

#define gdial_rest_server_http_return_if(expr, msg, state) \
{\
  if ((expr)) {\
    g_warn_if_fail(!(expr));\
    gdial_soup_message_set_http_error(msg, state);\
    return;\
  }\
}

#define gdial_soup_message_headers_set_Allow_Origin(msg, allowed) \
{\
  const gchar *header_origin = soup_message_headers_get_one(msg->request_headers, "Origin");\
  if (allowed && header_origin && strlen(header_origin)) {\
    soup_message_headers_replace(msg->response_headers, "Access-Control-Allow-Origin", header_origin);\
  }\
  else {\
    soup_message_headers_remove(msg->response_headers, "Access-Control-Allow-Origin");\
  }\
}

#define gdial_soup_message_headers_replace_va(hdrs, name, format, ...) \
{\
  gchar *value = g_strdup_printf(format, __VA_ARGS__); \
  soup_message_headers_replace(hdrs, name, value); \
  g_free(value);\
}

#define gdial_soup_message_headers_replace_va2(hdrs, name, format, ...) \
{\
  GString *value_buf = g_string_new("");\
  g_string_printf(value_buf, format, __VA_ARGS__);\
  gchar *value = g_string_free(value_buf, FALSE); \
  soup_message_headers_replace(hdrs, name, value); \
  g_free(value);\
}

#define gdial_soup_message_set_response_va(msg, ctype, format, ...) \
{\
  GString *value_buf = g_string_new("");\
  g_string_printf(value_buf, format, __VA_ARGS__);\
  soup_message_set_response(msg, ctype, SOUP_MEMORY_TAKE, value_buf->str, value_buf->len); \
  g_string_free(value_buf, FALSE); \
}

static GList *gdial_rest_server_registered_apps_clear(GList *registered_apps, GList *found) {
  GDialAppRegistry *app_registry = (GDialAppRegistry *)found->data;
  registered_apps = g_list_remove_link(registered_apps, found);
  gdial_app_regstry_dispose (app_registry);
  g_list_free(found);
  return registered_apps;
}

GDIAL_STATIC gboolean gdial_rest_server_should_relaunch_app(GDialApp *app, const gchar *payload) {
  /*
   *@TODO: connect state callback to plat-app. Here add workaround
   * by checking the state first. If app is stopped, then relaunch.
   */
  g_return_val_if_fail(app != NULL && app->name != NULL, TRUE);
  if (gdial_app_state(app) != GDIAL_APP_ERROR_NONE || GDIAL_APP_GET_STATE(app) == GDIAL_APP_STATE_STOPPED) {
    g_print("app [%s] state is stopped, relaunch required\r\n", app->name);
    return TRUE;
  }

  gchar *cached_payload = gdial_app_get_launch_payload(app);
  if ((cached_payload == NULL) && (payload == NULL)) {
    return FALSE;
  }

  if ((cached_payload != NULL) && (payload != NULL)) {
    int changed = g_strcmp0(cached_payload, payload);
    if (changed) {
      g_print("relaunch requred due to payload change [%s] vs [%s]\r\n", cached_payload, payload);
    }
    return changed;
  }

  return TRUE;
}

static gint GCompareFunc_match_registry_app_name(gconstpointer a, gconstpointer b) {
  GDialAppRegistry *app_registry = (GDialAppRegistry *)a;
  int is_matched = 0;
  /* match by prefix first */
  GList *app_prefixes = app_registry->app_prefixes;
  while (app_prefixes) {
    gchar *app_prefix = (gchar *)app_prefixes->data;
    if (GDIAL_STR_STARTS_WITH(b, app_prefix)) {
      is_matched = 1;
      break;
    }
    app_prefixes = app_prefixes->next;
  }
  /* match by exact name */
  if (!is_matched) is_matched = (g_strcmp0(app_registry->name, b) == 0);
  return is_matched ? 0 : 1;
}

GDIAL_STATIC GDialAppRegistry *gdial_rest_server_find_app_registry(GDialRestServer *self, const gchar *app_name) {
  g_return_val_if_fail(self != NULL && app_name != NULL, FALSE);
  GDialRestServerPrivate *priv = gdial_rest_server_get_instance_private(self);
  GList *found = g_list_find_custom(priv->registered_apps, app_name, GCompareFunc_match_registry_app_name);
  if (found) {
    return (GDialAppRegistry *)found->data;
  }
  return NULL;
}

GDIAL_STATIC gboolean gdial_rest_server_is_allowed_youtube_origin(GDialRestServer *self, const gchar *header_origin, const gchar *app_name) {
  if (self == NULL) return FALSE;

  gboolean is_allowed = FALSE;
  /* YouTube DIAL requirements overwrite the standard DIAL requirements for cors validation
   * 7.10 The target device MUST support CORS as outlined in Section 6.6 of the DIAL 2.2.1 protocol specification,
   * with the additional requirement that the target device MUST reject all requests to the DIAL server where the
   * ORIGIN header is not present.
   * 7.10.1 The ORIGIN header MUST match https://${ANY}.youtube.com or package
   */
  SoupURI *origin_uri = soup_uri_new(header_origin);
  const gchar *uri_scheme = origin_uri ? soup_uri_get_scheme(origin_uri) : NULL;

  if (origin_uri && uri_scheme &&
    ( uri_scheme == SOUP_URI_SCHEME_HTTPS )) {
    GDialAppRegistry *app_registry = gdial_rest_server_find_app_registry(self, app_name);
    if (app_registry) {
      is_allowed = gdial_app_registry_is_allowed_origin (app_registry, header_origin);
    }
    else {
    }
  }
  else {
    if(!g_strcmp0(uri_scheme,"package")) {
      is_allowed = TRUE;
    }
    else {
      is_allowed = FALSE;
    }
  }
  if (origin_uri) soup_uri_free(origin_uri);

  return is_allowed;
}

GDIAL_STATIC gboolean gdial_rest_server_is_allowed_origin(GDialRestServer *self, const gchar *header_origin, const gchar *app_name) {
  if (self == NULL) return FALSE;
  if (g_str_has_prefix(app_name,"YouTube")) return gdial_rest_server_is_allowed_youtube_origin(self,header_origin,app_name);
  if (header_origin == NULL) return TRUE;
  if (!g_strcmp0(header_origin, "")) return TRUE;

  gboolean is_allowed = FALSE;

  SoupURI *origin_uri = soup_uri_new(header_origin);
  const gchar *uri_scheme = origin_uri ? soup_uri_get_scheme(origin_uri) : NULL;

  if (origin_uri && uri_scheme &&
    (uri_scheme == SOUP_URI_SCHEME_HTTP || uri_scheme == SOUP_URI_SCHEME_HTTPS || uri_scheme == SOUP_URI_SCHEME_FILE)) {
    GDialAppRegistry *app_registry = gdial_rest_server_find_app_registry(self, app_name);
    if (app_registry) {
      is_allowed = gdial_app_registry_is_allowed_origin (app_registry, header_origin);
    }
    else {
    }
  }
  else {
    is_allowed = TRUE;
  }
  if (origin_uri) soup_uri_free(origin_uri);

  return is_allowed;
}

GDIAL_STATIC gchar *gdial_rest_server_new_additional_data_url(guint listening_port, const gchar *app_name, gboolean encode) {
  /*
   * The specifciation of additionalDataUrl in form of /apps/<app_name>/dial_data
   * thus the instance data must be included in the query or payload, not the path.
   *
   * [SPEC]
   * [[The additionalDataUrl value MUST be URL-encoded using the rules defined in [5]
   * for MIME type application/x-www-form-urlencoded.]]
   */
  GString *url_buf = g_string_new("");
  g_string_printf(url_buf, "http://%s:%d%s/%s%s", "localhost", listening_port, GDIAL_REST_HTTP_APPS_URI, app_name, GDIAL_REST_HTTP_DIAL_DATA_URI);
  gchar *unencoded = g_string_free(url_buf, FALSE);
  if (encode) {
    gchar *encoded = soup_uri_encode(unencoded, NULL);
    g_free(unencoded);
    return encoded;
  }
  else {
    return unencoded;
  }
}

static void gdial_rest_app_state_changed_cb(GDialApp *app, gpointer signal_param_user_data, gpointer user_data) {
  GDialRestServer *gdial_rest_server = (GDIAL_REST_SERVER(user_data));
  g_return_if_fail(gdial_rest_server_is_app_registered(gdial_rest_server, app->name));
  g_print("gdial_rest_app_state_changed_cb : [%s].state = %d\r\n", app->name, app->state);
}

static void gdial_rest_server_handle_OPTIONS(SoupMessage *msg, const gchar *allow_methods) {
  soup_message_headers_replace(msg->response_headers, "Access-Control-Allow-Methods", allow_methods);
  soup_message_headers_replace(msg->response_headers, "Access-Control-Max-Age", "86400");
  gdial_soup_message_headers_set_Allow_Origin(msg, TRUE);
  soup_message_set_status(msg, SOUP_STATUS_NO_CONTENT);
}

static gboolean gdial_rest_server_is_bad_payload(const gchar *data, goffset length) {
  return (gdial_util_is_ascii_printable(data, length) == FALSE);
}

static GDialApp *gdial_rest_server_check_instance(GDialApp *app, const gchar *instance) {
  /*
   * instance in URL should be "run" or an actual instance_id (e.g. pid_t)
   * @TODO: match exact instance sent in LOCATION URL.
   */
  g_return_val_if_fail(app && instance, NULL);

  GDialApp *app_by_instance = NULL;
  if (app && g_strcmp0(instance, (const char*)&(GDIAL_REST_HTTP_RUN_URI[1])) != 0) {
    gchar *endptr = NULL;
    gint instance_id = (gint)g_ascii_strtoll(instance, &endptr, 10);
    if (strlen(endptr) == 0) {
      app_by_instance = gdial_app_find_instance_by_instance_id(instance_id);
      g_return_val_if_fail(app == app_by_instance, app);
    }
    else {
      g_printerr("invalid instance %s %d\r\n", instance, instance_id);
    }
  }
  else {
    app_by_instance = app;
  }

  return app_by_instance;
}

static void gdial_rest_server_handle_POST_hide(SoupMessage *msg, GDialApp *app) {
  gdial_rest_server_http_return_if_fail((gdial_app_state(app) == GDIAL_APP_ERROR_NONE), msg, SOUP_STATUS_NOT_FOUND);
  gdial_rest_server_http_return_if_fail((GDIAL_APP_GET_STATE(app) == GDIAL_APP_STATE_RUNNING) || (GDIAL_APP_GET_STATE(app) == GDIAL_APP_STATE_HIDE), msg, SOUP_STATUS_NOT_FOUND);

  GDialAppError app_error = GDIAL_APP_ERROR_NONE;
  GDialAppState current_state = GDIAL_APP_GET_STATE(app);

  if ( current_state == GDIAL_APP_STATE_HIDE) {
    // Do not call gdial_app_hide if current app state is GDIAL_APP_STATE_HIDE
  }
  else if ( (app_error = gdial_app_hide(app)) == GDIAL_APP_ERROR_NONE) {
     g_warn_if_fail(gdial_app_state(app) == GDIAL_APP_ERROR_NONE && GDIAL_APP_GET_STATE(app) == GDIAL_APP_STATE_HIDE);
  }
  else if (app_error == GDIAL_APP_ERROR_NOT_IMPLEMENTED) {
    gdial_rest_server_http_return_if_fail(FALSE, msg, SOUP_STATUS_NOT_IMPLEMENTED);
  }
  else {
    g_printerr("gdial_app_hide(%s) failed\r\n", app->name);
    gdial_rest_server_http_return_if_fail(FALSE, msg, SOUP_STATUS_INTERNAL_SERVER_ERROR);
  }
  if(current_state == GDIAL_APP_STATE_STOPPED) {
     soup_message_set_status(msg, SOUP_STATUS_CREATED);
  }
  else {
     soup_message_set_status(msg, SOUP_STATUS_OK);
  }
  soup_message_headers_replace(msg->response_headers, "Content-Type", "text/plain; charset=utf-8");
  gdial_soup_message_headers_set_Allow_Origin(msg, TRUE);
}

static void gdial_rest_server_handle_DELETE(SoupMessage *msg, GHashTable *query, GDialApp *app) {
  gdial_rest_server_http_return_if_fail(g_strcmp0(app->name, "system") != 0, msg, SOUP_STATUS_FORBIDDEN);
  gdial_rest_server_http_return_if_fail((gdial_app_state(app) == GDIAL_APP_ERROR_NONE), msg, SOUP_STATUS_NOT_FOUND);
  gdial_rest_server_http_return_if_fail((GDIAL_APP_GET_STATE(app) == GDIAL_APP_STATE_RUNNING) || (GDIAL_APP_GET_STATE(app) == GDIAL_APP_STATE_HIDE), msg, SOUP_STATUS_NOT_FOUND);

  if (gdial_app_stop(app) == GDIAL_APP_ERROR_NONE) {
    g_warn_if_fail(gdial_app_state(app) == GDIAL_APP_ERROR_NONE && GDIAL_APP_GET_STATE(app) == GDIAL_APP_STATE_STOPPED);
  }
  else {
    g_printerr("gdial_app_stop(%s) failed, force shutdown\r\n", app->name);
    gdial_app_force_shutdown(app);
  }

  soup_message_headers_replace(msg->response_headers, "Content-Type", "text/plain; charset=utf-8");
  gdial_soup_message_headers_set_Allow_Origin(msg, TRUE);
  soup_message_set_status(msg, SOUP_STATUS_OK);
  g_object_unref(app);
}

static void gdial_rest_server_handle_POST(GDialRestServer *gdial_rest_server, SoupMessage* msg, GHashTable *query, const gchar *app_name) {
  GDialAppRegistry *app_registry = gdial_rest_server_find_app_registry(gdial_rest_server, app_name);
  gdial_rest_server_http_return_if_fail(app_registry, msg, SOUP_STATUS_NOT_FOUND);
  if (msg->request_body && msg->request_body->data && msg->request_body->length) {
    gdial_rest_server_http_return_if_fail(msg->request_body->length <= GDIAL_REST_HTTP_MAX_PAYLOAD, msg, SOUP_STATUS_REQUEST_ENTITY_TOO_LARGE);
    gdial_rest_server_http_return_if_fail(!gdial_rest_server_is_bad_payload(msg->request_body->data, msg->request_body->length), msg, SOUP_STATUS_BAD_REQUEST);
  }
  guint listening_port = soup_address_get_port(soup_message_get_address(msg));
  gdial_rest_server_http_return_if_fail(listening_port != 0, msg, SOUP_STATUS_INTERNAL_SERVER_ERROR);

  g_printerr("Starting the app with payload %.*s\n", (int)msg->request_body->length, msg->request_body->data);
  GDialApp *app = gdial_app_find_instance_by_name(app_registry->name);
  gboolean new_app_instance = FALSE;
  gboolean first_instance_created = FALSE;
  GDialAppState current_state = GDIAL_APP_STATE_STOPPED;

  if (app != NULL && app_registry->is_singleton) {
    /*
     * Reuse app instance as is, but do not update refcnt
     * per DIAL 2.1 recommendation, push relaunch decision to application platform,
     */
    g_printerr("POST request received for running app [%s]\r\n", app->name);
    new_app_instance = TRUE;
    first_instance_created = FALSE;
    current_state = GDIAL_APP_GET_STATE(app);
  }
  else {
    app = gdial_app_new(app_registry->name);
    new_app_instance = TRUE;
    first_instance_created = TRUE;
  }

  GDialAppError start_error = GDIAL_APP_ERROR_NONE;

  if (new_app_instance) {
    gchar *additional_data_url = NULL;
    if (app_registry->use_additional_data) {
      additional_data_url = gdial_rest_server_new_additional_data_url(listening_port, app_registry->name, FALSE);
    }
    gchar *additional_data_url_safe = soup_uri_encode(additional_data_url, NULL);
    g_print("additionalDataUrl = %s, %s\r\n", additional_data_url, additional_data_url_safe);
    g_signal_connect_object(app, "state-changed", G_CALLBACK(gdial_rest_app_state_changed_cb), gdial_rest_server, 0);
    const gchar *query_str = soup_uri_get_query(soup_message_get_uri(msg));
    gchar *query_str_safe = NULL;
    const int use_query_directly_from_soup = 1;
    if(query_str && strlen(query_str)) {
    g_print("query = %s\r\n", query_str);
      if (!use_query_directly_from_soup) {
        query_str_safe = soup_uri_encode(query_str, NULL);
      }
      else {
        query_str_safe = g_strdup(query_str);
      }
    }
    const gchar *payload = msg->request_body->data;
    gchar *payload_safe = NULL;
    if (payload && strlen(payload)) {
      if (g_str_has_prefix(app->name, "YouTube")) {
        /* temporary disabling encoding payload for YouTube till cloud side changed*/
        payload_safe = g_strdup(payload);
      }
      else {
        payload_safe = soup_uri_encode(payload, "=&");
      }
    }
    start_error = gdial_app_start(app, payload_safe, query_str_safe, additional_data_url_safe, gdial_rest_server);
    if (query_str_safe) g_free(query_str_safe);
    if (payload_safe) g_free(payload_safe);
    g_free(additional_data_url_safe);
    g_free(additional_data_url);
  }
  else {
    /*
     * start_error = NONE;
     * app exist, and could be in hidden state, so resume;
     */
    start_error = gdial_app_start(app, NULL, NULL, NULL, gdial_rest_server);
  }

  /*
   * The app start could be asyn, thus app->state may not have changed
   * to RUNNING yet. so started == TRUE, there will be followed by a
   * RUNNING callback. if started == FAUSE, then the instance is not
   * created;
   */
  if (start_error == GDIAL_APP_ERROR_NONE) {
    soup_message_headers_replace(msg->response_headers, "Content-Type", "text/plain; charset=utf-8");
    gdial_soup_message_headers_replace_va(msg->response_headers, "Location", "http://%s:%d%s/%s/run",
      soup_uri_get_host(soup_message_get_uri(msg)), listening_port, GDIAL_REST_HTTP_APPS_URI, app->name);
    gdial_soup_message_headers_set_Allow_Origin(msg, TRUE);
    if (new_app_instance) {
      if (g_strcmp0(app->name, "system") != 0 && (first_instance_created || current_state == GDIAL_APP_STATE_HIDE)) {
        soup_message_set_status(msg, SOUP_STATUS_CREATED);
      }
      else {
        soup_message_set_status(msg, SOUP_STATUS_OK);
      }
      /*
       *@TODO msg->request_body may not need to be cached app->payload as it is
       * only used by shouldRelaunch(), which is not used and we don't support
       * relaunch anyway;
       *
       * If relaunch is needed it is better to leave it to the app.
       */
      if(msg->request_body && msg->request_body->data) {
        g_print("POST request payload = [%s]\r\n", msg->request_body->data);
        gdial_app_set_launch_payload(app, msg->request_body->data);
      }
    }
    else {
      soup_message_set_status(msg, SOUP_STATUS_OK);
    }
  }
  else {
    g_object_unref(app);
    gdial_rest_server_http_return_if(start_error == GDIAL_APP_ERROR_FORBIDDEN, msg, SOUP_STATUS_FORBIDDEN);
    gdial_rest_server_http_return_if(start_error == GDIAL_APP_ERROR_UNAUTH, msg, SOUP_STATUS_UNAUTHORIZED);
    gdial_rest_server_http_return_if(TRUE, msg, SOUP_STATUS_SERVICE_UNAVAILABLE);
  }
}

static void gdial_rest_server_handle_GET_app(GDialRestServer *gdial_rest_server, SoupMessage *msg, GHashTable *query, const gchar *app_name, gint instance_id) {
  gdouble client_dial_version = 0.;
  if (query) {
    gchar *client_dial_version_str = g_hash_table_lookup(query, "clientDialVer");
    if (client_dial_version_str) {
      client_dial_version = g_ascii_strtod(client_dial_version_str, NULL);
      g_print("clientDialVer = %s = %f\r\n",client_dial_version_str, client_dial_version);
    }
  }

  GDialAppRegistry *app_registry = gdial_rest_server_find_app_registry(gdial_rest_server, app_name);
  gdial_rest_server_http_return_if_fail(app_registry, msg, SOUP_STATUS_NOT_FOUND);

  GDialApp *app = gdial_app_find_instance_by_name(app_name);
  GDialAppState app_state = GDIAL_APP_STATE_MAX;

  if (app != NULL) {
    /*
     * Get the app state
     */
    gdial_app_state(app);
    app_state = app->state;
    /*
     * if app has stopped, destroy the app instance
     */
  }
  else {
    /*
     * There is no app instance, but app may have started through
     * other means. ask platform for app state
     */
    app = gdial_app_new(app_name);
    gdial_app_state(app);
    app_state = app->state;
    if (app_state != GDIAL_APP_STATE_STOPPED) {
      g_signal_connect_object(app, "state-changed", G_CALLBACK(gdial_rest_app_state_changed_cb), gdial_rest_server, 0);
      g_print("creating app instance from state %d \r\n", app_state);
    }
  }

  gdial_soup_message_headers_set_Allow_Origin(msg, TRUE);
  soup_message_set_status(msg, SOUP_STATUS_OK);
  #if 0
  void *builder = GET_APP_response_builder_new(app_name);
  GET_APP_response_builder_set_option(builder, "allowStop", "true");
  GET_APP_response_builder_set_state(builder, app_state);
  GET_APP_response_builder_set_additionalData(builder, " ");
  gsize response_len = 0;
  gchar *response_str =  GET_APP_response_builder_build(builder, &response_len);
  //g_printf("############ response_str ##########\r\n%s\r\n", response_str);
  soup_message_set_response(msg, "text/xml", SOUP_MEMORY_TAKE, response_str, response_len);
  GET_APP_response_builder_destroy(builder);
  #else
  int response_len = 0;
  gpointer allow_stop = g_hash_table_lookup(app_registry->properties,"allowStop");
  if(allow_stop == NULL) {
    allow_stop = "false";
  }
  g_print("server_register_application allowStop:%s\n",allow_stop);
  gchar *response_str = gdial_app_state_response_new(app, GDIAL_PROTOCOL_VERSION_STR, GDIAL_PROTOCOL_XMLNS_SCHEMA, &response_len);
  #endif
  soup_message_set_response(msg, "text/xml; charset=utf-8", SOUP_MEMORY_TAKE, response_str, response_len);
  if (app_state == GDIAL_APP_STATE_STOPPED) {
    g_print("deleting app instance from state %d \r\n", app_state);
    g_object_unref(app);
  }
}

static void gdial_rest_server_handle_POST_dial_data(GDialRestServer *gdial_rest_server, SoupMessage* msg, GHashTable *query, const gchar *app_name) {
  /*
   * All instances of same app shares same additonalDataUrl
   */
  if(msg->request_body && msg->request_body->data && msg->request_body->length) {
    gdial_rest_server_http_return_if_fail(msg->request_body->length < GDIAL_APP_DIAL_DATA_MAX_LEN, msg, SOUP_STATUS_REQUEST_ENTITY_TOO_LARGE);
    gdial_rest_server_http_return_if_fail(!gdial_rest_server_is_bad_payload(msg->request_body->data, msg->request_body->length), msg, SOUP_STATUS_BAD_REQUEST);
  }
  /*
   * Cache dial_data so as to use on future queries.
   */
  GDialApp *app = gdial_app_find_instance_by_name(app_name);
  if(app == NULL)
  {
    g_print("gdial_rest_server_handle_POST_dial_data creating app instance \n");
    app = gdial_app_new(app_name);
  }
  gdial_rest_server_http_return_if_fail(app, msg, SOUP_STATUS_NOT_FOUND);
  /*
   * Give priority to body (body overrites query
   */
  if (GDIAL_MERGE_URL_AND_BODY_QUERY && query && !msg->request_body) {
    gdial_app_set_additional_dial_data(app, query);
  }
  else if ((msg->request_body && msg->request_body->data && msg->request_body->length)) {
    /* according to SoupMessage doc, msg->request_body c string, with the nul byte at data[length] */
    gdial_rest_server_http_return_if_fail(msg->request_body->data[msg->request_body->length] == '\0', msg, SOUP_STATUS_BAD_REQUEST);
    GHashTable *body_query = soup_form_decode(msg->request_body->data);
    if (body_query) {
      #if GDIAL_MERGE_URL_AND_BODY_QUERY
    /*append url query to body_query */
      if (query) {
        GHashTable *dupQuery = gdial_util_str_str_hashtable_dup(query);
        body_query = query ? gdial_util_str_str_hashtable_merge(body_query, dupQuery) : body_query;
        g_hash_table_destory(dupQuery);
      }
    #endif
    gdial_app_set_additional_dial_data(app, body_query);
    g_hash_table_destroy(body_query);
    }
  }
  else {
    printf("clear [%s] dial_data\r\n", app_name);
    GHashTable* empty = g_hash_table_new(NULL, NULL);
    gdial_app_set_additional_dial_data(app, empty);
    g_hash_table_destroy(empty);
  }

  gdial_soup_message_headers_set_Allow_Origin(msg, TRUE);
  soup_message_set_status(msg, SOUP_STATUS_OK);
}

static void gdial_rest_http_server_system_callback(SoupServer *server,
            SoupMessage *msg, const gchar *path, GHashTable *query,
            SoupClientContext  *client, gpointer user_data) {

  GDialRestServer *gdial_rest_server = (GDIAL_REST_SERVER(user_data));

  if (msg->method == SOUP_METHOD_DELETE) {
    /*
     * Stop Server
     */
    g_signal_emit(gdial_rest_server, gdial_rest_server_signals[SIGNAL_GMAINLOOP_QUIT], 0, "stop rest http gmainloop");
  }
  else if (msg->method == SOUP_METHOD_PUT) {
     gchar *value = g_hash_table_lookup(query,"rest_enable");
     g_print_with_timestamp("gdial_rest_http_server_system_callback emit SIGNAL_REST_ENABLE value:%s \r\n",(gchar *)value);
     g_signal_emit(gdial_rest_server, gdial_rest_server_signals[SIGNAL_REST_ENABLE], 0,value);
  }
  soup_message_set_status(msg, SOUP_STATUS_OK);
}

static void gdial_local_rest_http_server_callback(SoupServer *server,
            SoupMessage *msg, const gchar *path, GHashTable *query,
            SoupClientContext  *client, gpointer user_data) {
  gchar *remote_address_str = g_inet_address_to_string(g_inet_socket_address_get_address(G_INET_SOCKET_ADDRESS(soup_client_context_get_remote_address(client))));
  g_print_with_timestamp("gdial_local_rest_http_server_callback() %s path=%s recv from [%s], in thread %lx\r\n", msg->method, path, remote_address_str, pthread_self());
  g_free(remote_address_str);
  GDialRestServer *gdial_rest_server = (GDIAL_REST_SERVER(user_data));
  gchar **elements = g_strsplit(&path[1], "/", 4);
  gdial_rest_server_http_return_if_fail(elements != NULL, msg, SOUP_STATUS_NOT_IMPLEMENTED);
  gchar base[GDIAL_REST_HTTP_PATH_COMPONENT_MAX_LEN] = {0};
  gchar app_name[GDIAL_REST_HTTP_PATH_COMPONENT_MAX_LEN] = {0};
  gchar instance[GDIAL_REST_HTTP_PATH_COMPONENT_MAX_LEN] = {0};
  gchar last_elem[GDIAL_REST_HTTP_PATH_COMPONENT_MAX_LEN] = {0};
  int i = 0;
  int j = 0;
  for (i = 0; elements[i] != NULL; i++) {
    /* do not allow any element to be empty, stop on first one */
    if ((strlen(elements[i])) == 0) {
      g_printerr("Warn: empty elements in URI path\r\n");
      continue;
    }
    if (j == 0) g_strlcpy(base, elements[i], sizeof(base));
    else if (j == 1) g_strlcpy(app_name, elements[i], sizeof(app_name));
    else if (j == 2) g_strlcpy(instance, elements[i], sizeof(instance));
    g_strlcpy(last_elem, elements[i], sizeof(last_elem));
    j++;
  }
  g_strfreev(elements);
  const int element_num = j;
  printf("there are %d non-empty elems\r\n", element_num);
  if(element_num == 3 && g_strcmp0(instance,"dial_data") == 0)
  {
    GDialAppRegistry *app_registry = gdial_rest_server_find_app_registry(gdial_rest_server, app_name);
    gdial_rest_server_http_return_if_fail(app_registry, msg, SOUP_STATUS_NOT_FOUND);
    if (msg->method == SOUP_METHOD_POST) {
       gdial_rest_server_handle_POST_dial_data(gdial_rest_server, msg, query, app_name);
    }
    else if (msg->method == SOUP_METHOD_GET) {
     gdial_rest_server_handle_GET_app(gdial_rest_server, msg, query, app_name, GDIAL_APP_INSTANCE_NULL);
    }
    else {
      gdial_rest_server_http_return_if_fail(msg->method == SOUP_METHOD_POST, msg, SOUP_STATUS_NOT_IMPLEMENTED);
    }
  }
  else {
    gdial_soup_message_set_http_error(msg,SOUP_STATUS_NOT_IMPLEMENTED);
  }
}

static void gdial_rest_http_server_apps_callback(SoupServer *server,
            SoupMessage *msg, const gchar *path, GHashTable *query,
            SoupClientContext  *client, gpointer user_data) {
  gchar *remote_address_str = g_inet_address_to_string(g_inet_socket_address_get_address(G_INET_SOCKET_ADDRESS(soup_client_context_get_remote_address(client))));
  g_print_with_timestamp("gdial_rest_http_server_apps_callback() %s path=%s recv from [%s], in thread %lx\r\n", msg->method, path, remote_address_str, pthread_self());
  g_free(remote_address_str);

  gdial_rest_server_http_return_if_fail(server && msg && path && client && user_data, msg, SOUP_STATUS_INTERNAL_SERVER_ERROR);
  GDialRestServer *gdial_rest_server = (GDIAL_REST_SERVER(user_data));

  /*
   * Valid http paths (DIAL 2.1)
   *
   * Minimum URI must start with "/apps" -- ensured by soup
   * Minimum URI must be larger than "/apps/"
   * URI must not end with '/'
   *
   * Default <instance> is "run", but this is not guarnteed
   *
   * POST http://<ip>:<port>/apps/Netflix -- launch app
   * GET  http://<ip>:<port>/apps/Netflix -- get app state, and instance URL
   * GET  http://<ip>:<port>/apps/Netflix/<instance> -- get instance state
   * DELETE http://<ip>:<port>/apps/Netflix/<instance> -- stop instance
   * POST http://<ip>:<port>/apps/Netflix/<instance>/hide -- hide instance
   * POST http://<ip>:<port>/apps/Netflix/dial_data
   */
  gdial_rest_server_http_return_if_fail(
    g_socket_address_get_family(soup_client_context_get_remote_address(client)) == G_SOCKET_FAMILY_IPV4, msg, SOUP_STATUS_NOT_IMPLEMENTED);
  gdial_rest_server_http_return_if_fail(gdial_soup_message_security_check(msg), msg, SOUP_STATUS_INTERNAL_SERVER_ERROR);
  gdial_rest_server_http_return_if_fail(path != NULL, msg, SOUP_STATUS_INTERNAL_SERVER_ERROR);

  size_t path_len = strlen(path);
  gdial_rest_server_http_return_if_fail(path_len < GDIAL_REST_HTTP_MAX_URI_LEN, msg, SOUP_STATUS_INTERNAL_SERVER_ERROR);
  gdial_rest_server_http_return_if_fail(path_len > (GDIAL_STR_SIZEOF(GDIAL_REST_HTTP_APPS_URI) + GDIAL_STR_SIZEOF("/")), msg, SOUP_STATUS_NOT_IMPLEMENTED);
  gdial_rest_server_http_return_if_fail(strncmp(path, GDIAL_REST_HTTP_APPS_URI, GDIAL_STR_SIZEOF(GDIAL_REST_HTTP_APPS_URI)) == 0, msg, SOUP_STATUS_NOT_IMPLEMENTED);

  const gchar *header_host = soup_message_headers_get_one(msg->request_headers, "Host");
  gdial_rest_server_http_return_if_fail(header_host, msg, SOUP_STATUS_FORBIDDEN);

  /*
   * @TODO remote consecutive slashes.
   */
  gchar **elements = g_strsplit(&path[1], "/", 4);
  gdial_rest_server_http_return_if_fail(elements != NULL, msg, SOUP_STATUS_NOT_IMPLEMENTED);

  gchar base[GDIAL_REST_HTTP_PATH_COMPONENT_MAX_LEN] = {0};
  gchar app_name[GDIAL_REST_HTTP_PATH_COMPONENT_MAX_LEN] = {0};
  gchar instance[GDIAL_REST_HTTP_PATH_COMPONENT_MAX_LEN] = {0};
  gchar last_elem[GDIAL_REST_HTTP_PATH_COMPONENT_MAX_LEN] = {0};

  gboolean invalid_uri = FALSE;

  int i = 0;
  int j = 0;
  for (i = 0; elements[i] != NULL; i++) {
    /* do not allow any element to be empty, stop on first one */
    if ((strlen(elements[i])) == 0) {
      g_printerr("Warn: empty elements in URI path\r\n");
      //invalid_uri = invalid_uri || TRUE;
      //break;
      continue;
    }
    if (j == 0) g_strlcpy(base, elements[i], sizeof(base));
    else if (j == 1) g_strlcpy(app_name, elements[i], sizeof(app_name));
    else if (j == 2) g_strlcpy(instance, elements[i], sizeof(instance));
    g_strlcpy(last_elem, elements[i], sizeof(last_elem));
    j++;
  }

  const int element_num = j;
  printf("there are %d non-empty elems\r\n", element_num);
  /*
   * Make sure path remains same as given
   */
  const gchar *copied_str[] = {base, app_name, instance, last_elem};
  i = 0; j = 0;
  while (i < element_num && i < sizeof(copied_str)/sizeof(copied_str[0])) {
    if (strlen(elements[j]) == 0) {
      j++;
      continue;
    }
    invalid_uri = invalid_uri || g_strcmp0(copied_str[i], elements[j]);
    gdial_rest_server_http_return_if_fail(!invalid_uri, msg, SOUP_STATUS_NOT_IMPLEMENTED);
    j++;i++;
  }

  g_strfreev(elements);

  invalid_uri = invalid_uri || (i > 4 || i < 2);
  invalid_uri = invalid_uri || (g_strcmp0(base, &GDIAL_REST_HTTP_APPS_URI[1]) != 0);
  invalid_uri = invalid_uri || (strlen(app_name) == 0);

  gdial_rest_server_http_return_if_fail(!invalid_uri, msg, SOUP_STATUS_NOT_IMPLEMENTED);

  if(!gdial_rest_server_is_app_registered(gdial_rest_server, app_name)) {
    /*
     * Any request only respond to app name that is among registered apps
     */
    g_signal_emit(gdial_rest_server, gdial_rest_server_signals[SIGNAL_INVALID_URI], 0, "URI containes unregistered app name");
    gdial_rest_server_http_return_if_fail(FALSE, msg, SOUP_STATUS_NOT_FOUND);
  }

  const gchar *header_origin = soup_message_headers_get_one(msg->request_headers, "Origin");
  g_printerr("Origin %s, Host: %s, Method: %s\r\n", header_origin, header_host, msg->method);
  if (!gdial_rest_server_is_allowed_origin(gdial_rest_server, header_origin, app_name)) {
    gdial_rest_server_http_print_and_return_if_fail(FALSE, msg, SOUP_STATUS_FORBIDDEN, "origin %s is not allowed\r\n", header_origin);
  }
  /*
   * element_num == 2:
   *   apps/Netflix
   *
   * element_num == 3:
   *   apps/Netflix/run
   *   apps/Netflix/12345
   *   apps/Netflix/dial_data
   *
   * element_num == 4:
   *   apps/Netflix/run/hide
   *   apps/Netflix/12345/hide
   */
  if (element_num == 2) {
    // URL ends with app name
    g_print("app_name is %s\r\n", app_name);
    if (!header_host || !gdial_rest_server_is_allowed_origin(gdial_rest_server, header_origin, app_name)) {
      gdial_rest_server_http_return_if_fail(FALSE, msg, SOUP_STATUS_FORBIDDEN);
    }
    else if (msg->method == SOUP_METHOD_OPTIONS) {
      gdial_rest_server_handle_OPTIONS(msg, "GET, POST, OPTIONS");
    }
    else if (msg->method == SOUP_METHOD_POST) {
      gdial_rest_server_handle_POST(gdial_rest_server, msg, query, app_name);
    }
    else if (msg->method == SOUP_METHOD_GET) {
      /*
       * GET_app will get app state...there is no instance_id in URL
       */
      gdial_rest_server_handle_GET_app(gdial_rest_server, msg, query, app_name, GDIAL_APP_INSTANCE_NULL);
    }
    else {
      gdial_rest_server_http_return_if_fail(FALSE, msg, SOUP_STATUS_NOT_IMPLEMENTED);
    }
  }
  else if (element_num == 3) {
    if (g_strcmp0(last_elem, &GDIAL_REST_HTTP_DIAL_DATA_URI[1]) ==0) {
      // URL ends with dial_data, only accepted when originating from localhost
      g_print("for [%s] app_name is %s\r\n", last_elem, app_name);
      GSocketAddress *remote_address = soup_client_context_get_remote_address(client);
      GError *error = NULL;
      struct sockaddr_in saddr;
      gdial_rest_server_http_return_if_fail(remote_address && g_socket_address_to_native(remote_address, &saddr, sizeof(saddr), &error) && !error, msg, SOUP_STATUS_INTERNAL_SERVER_ERROR);
      gdial_rest_server_http_return_if_fail(saddr.sin_addr.s_addr == htonl(INADDR_LOOPBACK), msg, SOUP_STATUS_INTERNAL_SERVER_ERROR);

      if (msg->method == SOUP_METHOD_OPTIONS) {
        gdial_rest_server_handle_OPTIONS(msg, "POST, OPTIONS");
      }
      else {
        gdial_rest_server_http_return_if_fail(msg->method == SOUP_METHOD_POST, msg, SOUP_STATUS_NOT_IMPLEMENTED);
      }
    }
    else {
      // URL ends with .../run or some app specific instance Id .../<instance_id>
      g_print("for instance [%s] app_name is %s\r\n", last_elem, app_name);
      if (!header_host || !gdial_rest_server_is_allowed_origin(gdial_rest_server, header_origin, app_name)) {
        gdial_rest_server_http_return_if_fail(FALSE, msg, SOUP_STATUS_FORBIDDEN);
      }
      else if (msg->method == SOUP_METHOD_OPTIONS) {
        gdial_rest_server_handle_OPTIONS(msg, "DELETE, OPTIONS");
      }
      else if (msg->method == SOUP_METHOD_DELETE) {
        GDialApp *app = gdial_app_find_instance_by_name(app_name);
        GDialApp *app_by_instance = gdial_rest_server_check_instance(app, instance);
        if (app_by_instance) {
          gdial_rest_server_handle_DELETE(msg, query, app);
        }
        else {
          g_printerr("app to delete is not found\r\n");
          gdial_soup_message_set_http_error(msg, SOUP_STATUS_NOT_FOUND);
        }
      }
      else if (msg->method == SOUP_METHOD_POST) {
          gdial_soup_message_set_http_error(msg, SOUP_STATUS_NOT_FOUND);
      }
      else {
        gdial_rest_server_http_return_if_fail(FALSE, msg, SOUP_STATUS_NOT_IMPLEMENTED);
      }
    }
  }
  else if (element_num == 4) {
    if (g_strcmp0(last_elem, &GDIAL_REST_HTTP_HIDE_URI[1]) == 0) {
      // URL ends with hide
      g_print("for [%s] app_name is %s, instance is %s\r\n", last_elem, app_name, instance);
      if (msg->method == SOUP_METHOD_OPTIONS) {
        gdial_rest_server_handle_OPTIONS(msg, "POST, OPTIONS");
      }
      else if (msg->method == SOUP_METHOD_POST) {

        GDialApp *app = gdial_app_find_instance_by_name(app_name);
        GDialApp *app_by_instance = gdial_rest_server_check_instance(app, instance);
        if (app_by_instance) {
          gdial_rest_server_handle_POST_hide(msg, app);
        }
        else {
          g_printerr("app to hide is not found\r\n");
          gdial_rest_server_http_return_if_fail(FALSE, msg, SOUP_STATUS_NOT_FOUND);
        }
      }
      else if (msg->method == SOUP_METHOD_DELETE) {
        gdial_rest_server_http_return_if_fail(msg->method == SOUP_METHOD_POST, msg, SOUP_STATUS_NOT_FOUND);
      }
      else {
        gdial_rest_server_http_return_if_fail(msg->method == SOUP_METHOD_POST, msg, SOUP_STATUS_NOT_IMPLEMENTED);
      }
    }
    else {
      invalid_uri = TRUE;
    }
  }

  gdial_rest_server_http_return_if_fail(!invalid_uri, msg, SOUP_STATUS_NOT_IMPLEMENTED);
}

static void gdial_rest_server_dispose(GObject *object) {
  GDialRestServerPrivate *priv = gdial_rest_server_get_instance_private(GDIAL_REST_SERVER(object));
  soup_server_remove_handler(priv->soup_instance, GDIAL_REST_HTTP_APPS_URI);
  g_object_unref(priv->soup_instance);
  g_object_unref(priv->local_soup_instance);
  while (priv->registered_apps) {
    priv->registered_apps = gdial_rest_server_registered_apps_clear(priv->registered_apps, priv->registered_apps);
  }
  G_OBJECT_CLASS (gdial_rest_server_parent_class)->dispose (object);
}

static void gdial_rest_server_get_property (GObject *object, guint property_id, GValue *value, GParamSpec *pspec) {
  GDialRestServerPrivate *priv = gdial_rest_server_get_instance_private(GDIAL_REST_SERVER(object));

  switch (property_id) {
    case PROP_SOUP_INSTANCE:
      g_value_set_object(value, priv->soup_instance);
      break;
    case PROP_LOCAL_SOUP_INSTANCE:
      g_value_set_object(value, priv->local_soup_instance);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
      break;
  }
}

static void gdial_rest_server_set_property (GObject *object, guint property_id, const GValue *value, GParamSpec *pspec) {
  GDialRestServer *self = GDIAL_REST_SERVER(object);
  GDialRestServerPrivate *priv = gdial_rest_server_get_instance_private(self);

  switch (property_id) {
    case PROP_SOUP_INSTANCE:
      priv->soup_instance = g_value_get_object(value);
      break;
    case PROP_LOCAL_SOUP_INSTANCE:
      priv->local_soup_instance = g_value_get_object(value);
      break;
    case PROP_ENABLE:
      if(priv->soup_instance)
      {
          if(g_value_get_boolean(value))
          {
              g_print("gdial_rest_server_set_property add handler\n");
              soup_server_add_handler(priv->soup_instance, GDIAL_REST_HTTP_APPS_URI, gdial_rest_http_server_apps_callback, object, NULL);
          }
          else
          {
              g_print("gdial_rest_server_set_property remove handler\n");
              soup_server_remove_handler(priv->soup_instance, GDIAL_REST_HTTP_APPS_URI);
          }
      }
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
      break;
  }
}

static void gdial_rest_server_class_init(GDialRestServerClass *klass) {

  GObjectClass *gobject_class = (GObjectClass *)klass;

  gobject_class->dispose = gdial_rest_server_dispose;
  gobject_class->get_property = gdial_rest_server_get_property;
  gobject_class->set_property = gdial_rest_server_set_property;

  g_object_class_install_property (gobject_class, PROP_SOUP_INSTANCE,
          g_param_spec_object("soup_instance", NULL, "Http Server for DIAL Rest Service",
                  SOUP_TYPE_SERVER,
                  G_PARAM_READWRITE));
  g_object_class_install_property (gobject_class, PROP_LOCAL_SOUP_INSTANCE,
          g_param_spec_object("local_soup_instance", NULL, "Local Http Server for DIAL Rest Service",
                  SOUP_TYPE_SERVER,
                  G_PARAM_READWRITE));
  g_object_class_install_property (gobject_class, PROP_ENABLE,
          g_param_spec_boolean("enable", NULL, "Enable REST Server",
                  FALSE,
                  G_PARAM_WRITABLE));

  gdial_rest_server_signals[SIGNAL_INVALID_URI] =
    g_signal_new ("invalid-uri",
            G_OBJECT_CLASS_TYPE (gobject_class),
            0, /* flag */
            0, /* class offset */
            NULL, NULL, /* accumulator, accu_data */
            NULL, /* c_marshaller, use default */
            G_TYPE_NONE, 1, /* return type, arg num */
            G_TYPE_STRING); /* arg types */

  gdial_rest_server_signals[SIGNAL_GMAINLOOP_QUIT] =
    g_signal_new ("gmainloop-quit",
            G_OBJECT_CLASS_TYPE (gobject_class),
            0, /* flag */
            0, /* class offset */
            NULL, NULL, /* accumulator, accu_data */
            NULL, /* c_marshaller, use default */
            G_TYPE_NONE, 1, /* return type, arg num */
            G_TYPE_STRING); /* arg types */

  gdial_rest_server_signals[SIGNAL_REST_ENABLE] =
    g_signal_new ("rest-enable",
            G_OBJECT_CLASS_TYPE (gobject_class),
            0, /* flag */
            0, /* class offset */
            NULL, NULL, /* accumulator, accu_data */
            NULL, /* c_marshaller, use default */
            G_TYPE_NONE, 1, /* return type, arg num */
            G_TYPE_STRING); /* arg types */
}

static void gdial_rest_server_init(GDialRestServer *self) {
  GDialRestServerPrivate *priv = gdial_rest_server_get_instance_private(self);
  priv->registered_apps = NULL;
}

GDialRestServer *gdial_rest_server_new(SoupServer *rest_http_server,SoupServer * local_rest_http_server) {
  g_return_val_if_fail(rest_http_server != NULL, NULL);
  g_return_val_if_fail(local_rest_http_server != NULL, NULL);
  g_object_ref(local_rest_http_server);
  g_object_ref(rest_http_server);
  gpointer object = g_object_new(GDIAL_TYPE_REST_SERVER, GDIAL_REST_SERVER_SOUP_INSTANCE, rest_http_server,GDIAL_LOCAL_REST_SERVER_SOUP_INSTANCE,local_rest_http_server, NULL);
#ifdef GDIAL_BUILD_TEST
  /*
   * For Testing Purpose only
   */
  soup_server_add_handler(rest_http_server, "/apps/system", gdial_rest_http_server_system_callback, object, NULL);
#endif
  g_print("gdial_local_rest_http_server_callback add handler\n");

  soup_server_add_handler(local_rest_http_server, GDIAL_REST_HTTP_APPS_URI, gdial_local_rest_http_server_callback, object, NULL);
  return object;
}

gboolean gdial_rest_server_register_app(GDialRestServer *self, const gchar *app_name, const GList *app_prefixes, const GHashTable *properties, gboolean is_singleton, gboolean use_additional_data, const GList *allowed_origins) {

  g_return_val_if_fail(self != NULL && app_name != NULL, FALSE);
  /*
   *@TODO: support multiple app instances.
   */
  g_return_val_if_fail(is_singleton, FALSE);

  GDialRestServerPrivate *priv = gdial_rest_server_get_instance_private(self);
  if (g_list_find_custom(priv->registered_apps, app_name, GCompareFunc_match_registry_app_name) != NULL) {
   /*
    * Do not support duplicate registration with different param
    *
    *@TODO: check params. If identical to previous registraiton, return TRUE;
    */
    return FALSE;
  }

  GDialAppRegistry *app_registry = gdial_app_registry_new (g_strdup(app_name),
                                                           app_prefixes,
                                                           properties,
                                                           is_singleton,
                                                           use_additional_data,
                                                           allowed_origins);
  priv->registered_apps = g_list_prepend(priv->registered_apps, app_registry);

  /*
   * when an app is registered, we also check if it is already running
   * @TODO
   */

  if(strcmp(app_name,"system") == 0){
	  gdial_app_new(app_registry->name);
  }
  g_return_val_if_fail(priv->registered_apps != NULL, FALSE);
  g_return_val_if_fail(gdial_rest_server_is_app_registered(self, app_name), FALSE);
  return TRUE;
}

gboolean gdial_rest_server_register_app_registry(GDialRestServer *self, GDialAppRegistry *app_registry) {

  g_return_val_if_fail(self != NULL && app_registry != NULL, FALSE);

  GDialRestServerPrivate *priv = gdial_rest_server_get_instance_private(self);
  if (g_list_find_custom(priv->registered_apps, app_registry->name, GCompareFunc_match_registry_app_name) != NULL) {
   /*
    * Do not support duplicate registration with different param
    *
    *@TODO: check params. If identical to previous registraiton, return TRUE;
    */
    return FALSE;
  }

  priv->registered_apps = g_list_prepend(priv->registered_apps, app_registry);

  /*
   * when an app is registered, we also check if it is already running
   * @TODO
   */

  g_return_val_if_fail(priv->registered_apps != NULL, FALSE);
  g_return_val_if_fail(gdial_rest_server_is_app_registered(self, app_registry->name), FALSE);
  return TRUE;
}

gboolean gdial_rest_server_unregister_all_apps(GDialRestServer *self) {
  g_return_val_if_fail(self != NULL, FALSE);

  g_print("Inside gdial_rest_server_unregister_all_apps\n");
  GDialRestServerPrivate *priv = gdial_rest_server_get_instance_private(self);
  GList *registered_apps_head = priv->registered_apps;
  /*Stopping all registread Apps*/
  while (priv->registered_apps) {
    GDialAppRegistry *app_registry = priv->registered_apps->data;
    GDialApp *app = gdial_app_find_instance_by_name(app_registry->name);
    if (app) {
      if (gdial_app_stop(app) == GDIAL_APP_ERROR_NONE) {
        g_warn_if_fail(gdial_app_state(app) == GDIAL_APP_ERROR_NONE && GDIAL_APP_GET_STATE(app) == GDIAL_APP_STATE_STOPPED);
      }
      else {
        g_printerr("gdial_app_stop(%s) failed, force shutdown\r\n", app->name);
        gdial_app_force_shutdown(app);
      }
      g_object_unref(app);
    }
    priv->registered_apps = priv->registered_apps->next;
  }
  priv->registered_apps = registered_apps_head;
  /*Remove all registered apps before*/
  while (priv->registered_apps) {
    priv->registered_apps = gdial_rest_server_registered_apps_clear(priv->registered_apps, priv->registered_apps);
  }
  return TRUE;
}

gboolean gdial_rest_server_is_app_registered(GDialRestServer *self, const gchar *app_name) {
  return gdial_rest_server_find_app_registry(self, app_name) != NULL;
}

gboolean gdial_rest_server_unregister_app(GDialRestServer *self, const gchar *app_name) {
  g_return_val_if_fail(self != NULL && app_name != NULL, FALSE);
  GDialRestServerPrivate *priv = gdial_rest_server_get_instance_private(self);
  GList *found = g_list_find_custom(priv->registered_apps, app_name, GCompareFunc_match_registry_app_name);
  if (found == NULL) return FALSE;
  priv->registered_apps = gdial_rest_server_registered_apps_clear(priv->registered_apps, found);
  return TRUE;
}

typedef struct  {
  gchar *app_name;
  gchar *dialVer;
  GHashTable *options;
  GDialAppState state;
  gchar *installable;
  gchar *link_href;
  gchar *additionalData;
} GDialServerResponseBuilderGetApp;


GDIAL_STATIC_INLINE void *GET_APP_response_builder_new(const gchar *app_name) {
  GDialServerResponseBuilderGetApp *rbuilder = (GDialServerResponseBuilderGetApp *) malloc(sizeof(*rbuilder));
  memset(rbuilder, 0, sizeof(*rbuilder));
  memset(rbuilder, 0, sizeof(*rbuilder));
  rbuilder->app_name = g_strdup(app_name);
  rbuilder->options = g_hash_table_new_full(g_str_hash, g_str_equal, g_free, g_free);
  rbuilder->dialVer = g_strdup(GDIAL_PROTOCOL_VERSION_STR);
  rbuilder->link_href = g_strdup("run");
  return rbuilder;
}

/*
 * options:
 * [@allowStop, ...]
 */
GDIAL_STATIC_INLINE void *GET_APP_response_builder_set_option(void *builder, const gchar *option_name, const gchar *option_value) {
  GDialServerResponseBuilderGetApp * rbuilder = (GDialServerResponseBuilderGetApp *)builder;
  /*
   * Simple check only...
   */
  if (option_name && option_value) {
    g_hash_table_insert(rbuilder->options, g_strdup(option_name), g_strdup(option_value));
  }
  return builder;
}

GDIAL_STATIC_INLINE void *GET_APP_response_builder_set_state(void *builder, GDialAppState state) {
  ((GDialServerResponseBuilderGetApp *)builder)->state= state;
  return builder;
}

GDIAL_STATIC_INLINE void *GET_APP_response_builder_set_installable(void *builder, const gchar *encoded_url) {
  ((GDialServerResponseBuilderGetApp *)builder)->installable= soup_uri_encode(encoded_url, NULL);
  return builder;
}

GDIAL_STATIC_INLINE void *GET_APP_response_builder_set_link_href(void *builder, const gchar *encoded_href) {
  GDialServerResponseBuilderGetApp * rbuilder = (GDialServerResponseBuilderGetApp *)builder;
  g_free(rbuilder->link_href);
  if (encoded_href) {
    rbuilder->link_href= soup_uri_encode(encoded_href, NULL);
  }
  else {
    rbuilder->link_href = g_strdup("run");
  }
  return builder;
}

GDIAL_STATIC_INLINE void *GET_APP_response_builder_set_additionalData(void *builder, const gchar *additionalData) {
  if (additionalData) {
    ((GDialServerResponseBuilderGetApp *)builder)->additionalData = g_strdup(additionalData);
  }
  return builder;
}

GDIAL_STATIC_INLINE gchar *GET_APP_response_builder_build(void *builder, gsize *length) {
  GDialServerResponseBuilderGetApp * rbuilder = (GDialServerResponseBuilderGetApp *)builder;
  GString *rbuf = g_string_new_len('\0', 128);
  gsize options_length = 0;
  gchar *options_str = gdial_util_str_str_hashtable_to_xml_string(rbuilder->options, &options_length);

  g_string_append_printf(rbuf, "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\r\n");
  g_string_append_printf(rbuf, "<service xmlns=\"%s\" dialVer=\"%s\">\r\n", GDIAL_PROTOCOL_XMLNS_SCHEMA, rbuilder->dialVer);
  g_string_append_printf(rbuf, "  <name>%s</name>\r\n", rbuilder->app_name);
  if (options_str) {
  g_string_append_printf(rbuf, "  <options %s/>\r\n", options_str);
  g_free(options_str);
  }
  g_string_append_printf(rbuf, "  <state>%s</state>\r\n", gdial_app_state_to_string(rbuilder->state));
  if (rbuilder->state != GDIAL_APP_STATE_STOPPED) {
  g_string_append_printf(rbuf, "  <link rel=\"run\" href=\"%s\"/>\r\n", rbuilder->link_href);
  }
  if (rbuilder->additionalData) {
  g_string_append_printf(rbuf, "  <additionalData>%s</additionalData>\r\n", "");
  }
  g_string_append_printf(rbuf, "</service>\r\n");

  if (length) {
    *length = rbuf->len;
  }

  return g_string_free(rbuf, FALSE);
}

GDIAL_STATIC_INLINE void GET_APP_response_builder_destroy(void *builder) {
  GDialServerResponseBuilderGetApp * rbuilder = (GDialServerResponseBuilderGetApp *)builder;
  g_free(rbuilder->app_name);
  g_free(rbuilder->dialVer);
  g_hash_table_destroy(rbuilder->options);
  g_free(rbuilder->link_href);
  g_free(rbuilder->installable);
  g_free(rbuilder->additionalData);

  g_free(builder);
}
