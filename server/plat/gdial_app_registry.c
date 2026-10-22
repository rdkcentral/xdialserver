/*
 * If not stated otherwise in this file or this component's LICENSE file the
 * following copyright and licenses apply:
 *
 * Copyright 2019 RDK Management
 *
 * Author: Hong Li
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
#include <uuid/uuid.h>
#include <gdial_app_registry.h>
#include "gdialservicelogging.h"

#define UUID_FILE_TEMPLATE "/tmp/.dial_%s_uuid.txt"

#define GDIAL_STR_ENDS_WITH(s1, s2) ((s1 != NULL) && (s2 != NULL) && ((strlen(s2) == 0) || (g_str_has_suffix(s1, s2))))

void gdial_app_regstry_dispose (GDialAppRegistry *app_registry) {
  g_return_if_fail(app_registry != NULL);
  GDIAL_LOGINFO("freeing app name:%s", app_registry->name);
  g_free(app_registry->name);
  app_registry->name = NULL;
  g_list_free_full(app_registry->allowed_origins, g_free);
  g_list_free_full(app_registry->app_prefixes, g_free);
  if (app_registry->properties) {
    g_hash_table_destroy (app_registry->properties);
    app_registry->properties = NULL;
  }
  free(app_registry);
  return;
}

// Helper function to extract host from an origin string without libsoup
static gchar *util_get_host_from_origin(const gchar *origin) {
  if (!origin) return NULL;

  const gchar *host_start = NULL;

  if (g_str_has_prefix(origin, "https://")) {
    host_start = origin + strlen("https://");
  } else if (g_str_has_prefix(origin, "http://")) {
    host_start = origin + strlen("http://");
  } else {
    host_start = origin;
  }

  const gchar *delim_port = strchr(host_start, ':');
  const gchar *delim_path = strchr(host_start, '/');
  const gchar *end = NULL;

  if (delim_port && delim_path) {
    end = (delim_port < delim_path) ? delim_port : delim_path;
  } else if (delim_port) {
    end = delim_port;
  } else {
    end = delim_path;
  }

  if (end) {
    return g_strndup(host_start, end - host_start);
  }

  return g_strdup(host_start);
}

gboolean gdial_app_registry_is_allowed_origin(GDialAppRegistry *app_registry, const gchar *header_origin) {
  gboolean is_allowed = FALSE;

  if (!app_registry) {
    GDIAL_LOGINFO("gdial_app_registry_is_allowed_origin: app_registry=NULL header_origin[%s]", header_origin ? header_origin : "(null)");
    return FALSE;
  }
  if (header_origin == NULL) {
    GDIAL_LOGINFO("gdial_app_registry_is_allowed_origin: app[%s] header_origin=NULL", app_registry->name ? app_registry->name : "(null)");
    return FALSE;
  }

  GList *allowed_origins = app_registry->allowed_origins;
  GDIAL_LOGINFO("gdial_app_registry_is_allowed_origin: app[%s] incoming_origin[%s] configured_count[%d]",
                app_registry->name ? app_registry->name : "(null)",
                header_origin,
                g_list_length(allowed_origins));
  if (allowed_origins == NULL) {
    GDIAL_LOGINFO("gdial_app_registry_is_allowed_origin: app[%s] allowed list empty => allowing origin[%s]", app_registry->name ? app_registry->name : "(null)", header_origin);
    return TRUE;
  }

  while (allowed_origins) {
    gchar *origin = (gchar *)allowed_origins->data;
    GDIAL_LOGINFO("gdial_app_registry_is_allowed_origin: app[%s] checking configured_origin[%s] against incoming[%s]",
                  app_registry->name ? app_registry->name : "(null)",
                  origin ? origin : "(null)",
                  header_origin);
    if (origin == NULL) {
      allowed_origins = allowed_origins->next;
      continue;
    }

    if (g_strcmp0(origin, "*") == 0) {
      GDIAL_LOGINFO("gdial_app_registry_is_allowed_origin: app[%s] matched wildcard '*'", app_registry->name ? app_registry->name : "(null)");
      is_allowed = TRUE;
      break;
    }

    if (g_str_has_prefix(origin, "package:")) {
      if (g_strcmp0(header_origin, origin) == 0) {
        GDIAL_LOGINFO("gdial_app_registry_is_allowed_origin: app[%s] matched package origin exact match: configured[%s] incoming[%s]",
                      app_registry->name ? app_registry->name : "(null)",
                      origin,
                      header_origin);
        is_allowed = TRUE;
        break;
      }
      GDIAL_LOGINFO("gdial_app_registry_is_allowed_origin: app[%s] package mismatch: configured[%s] incoming[%s]",
                    app_registry->name ? app_registry->name : "(null)",
                    origin,
                    header_origin);
    }
    else if (g_str_has_prefix(origin, "https://*.")) {
      gchar *wildcard_host = origin + strlen("https://*.");
      gchar *header_host = util_get_host_from_origin(header_origin);
      GDIAL_LOGINFO("gdial_app_registry_is_allowed_origin: app[%s] wildcard check: configured[%s] wildcard_host[%s] incoming_host[%s]",
                    app_registry->name ? app_registry->name : "(null)",
                    origin,
                    wildcard_host,
                    header_host ? header_host : "(null)");
      if (header_host) {
        if (g_strcmp0(header_host, wildcard_host) == 0 || g_str_has_suffix(header_host, wildcard_host)) {
          size_t host_len = strlen(header_host);
          size_t wild_len = strlen(wildcard_host);
          if (host_len == wild_len || (host_len > wild_len && header_host[host_len - wild_len - 1] == '.')) {
            GDIAL_LOGINFO("gdial_app_registry_is_allowed_origin: app[%s] matched wildcard host rule", app_registry->name ? app_registry->name : "(null)");
            is_allowed = TRUE;
          }
        }
        g_free(header_host);
      }
      if (is_allowed) break;
    }
    else {
      gchar *configured_host = util_get_host_from_origin(origin);
      gchar *header_host = util_get_host_from_origin(header_origin);
      GDIAL_LOGINFO("gdial_app_registry_is_allowed_origin: app[%s] strict host check: configured_host[%s] header_host[%s] configured_scheme_secure[%d] incoming_scheme_secure[%d]",
                    app_registry->name ? app_registry->name : "(null)",
                    configured_host ? configured_host : "(null)",
                    header_host ? header_host : "(null)",
                    g_str_has_prefix(origin, "https://"),
                    g_str_has_prefix(header_origin, "https://"));
      if (configured_host && header_host) {
        if (g_strcmp0(header_host, configured_host) == 0 &&
            g_str_has_prefix(header_origin, "https://") == g_str_has_prefix(origin, "https://")) {
          GDIAL_LOGINFO("gdial_app_registry_is_allowed_origin: app[%s] matched strict host rule", app_registry->name ? app_registry->name : "(null)");
          is_allowed = TRUE;
        }
      }
      g_free(configured_host);
      g_free(header_host);

      if (is_allowed) break;
    }

    allowed_origins = allowed_origins->next;
  }

  GDIAL_LOGINFO("gdial_app_registry_is_allowed_origin: app[%s] final_result[%s] for incoming[%s]",
                app_registry->name ? app_registry->name : "(null)",
                is_allowed ? "ALLOW" : "DENY",
                header_origin);
  return is_allowed;
}

GDialAppRegistry* gdial_app_registry_new (const gchar *app_name, const GList *app_prefixes, GHashTable *properties, gboolean is_singleton, gboolean use_additional_data, const GList *allowed_origins) {
  g_return_val_if_fail(app_name != NULL, NULL);
  g_return_val_if_fail(is_singleton, NULL);

  GDialAppRegistry *app_registry = (GDialAppRegistry *)malloc(sizeof(*app_registry));
  memset(app_registry, 0, sizeof(*app_registry));
  app_registry->name = g_strdup(app_name);
  app_registry->is_singleton = is_singleton;
  app_registry->use_additional_data = use_additional_data;
  while (app_prefixes) {
    if (app_prefixes->data && (strlen(app_prefixes->data) > 0)) {
      app_registry->app_prefixes = g_list_prepend(app_registry->app_prefixes, g_strdup(app_prefixes->data));
    }
    app_prefixes = app_prefixes->next;
  }
  //add to registry
  if(properties) {
    GHashTableIter prop_iter;
    gpointer key,value;
    app_registry->properties = g_hash_table_new_full(g_str_hash, g_str_equal, g_free, g_free);
    g_hash_table_iter_init(&prop_iter, properties);
    while (g_hash_table_iter_next(&prop_iter, &key, &value)) {
      GDIAL_LOGINFO("inside register app properties key:%s value:%s",(gchar*)key, (gchar*)value);
      g_hash_table_insert(app_registry->properties,g_strdup(key),g_strdup(value));
    }
  }

  while(allowed_origins) {
    app_registry->allowed_origins = g_list_prepend(app_registry->allowed_origins, g_strdup(allowed_origins->data));
    allowed_origins = allowed_origins->next;
  }

  char app_uuid_file_path[64] = {0};
  char uuid_data[APP_MAX_UUID_SIZE] = {0};
  snprintf( app_uuid_file_path, sizeof(app_uuid_file_path), UUID_FILE_TEMPLATE , app_name);

  FILE *fuuid = fopen(app_uuid_file_path, "r");
  if (fuuid == NULL)
  {
    uuid_t random_uuid;
    uuid_generate_random(random_uuid);
    uuid_unparse(random_uuid, uuid_data);
    GDIAL_LOGINFO("generated uuid:[%s]", uuid_data);

    fuuid = fopen(app_uuid_file_path, "w");
    if (fuuid != NULL)
    {
        fprintf(fuuid, "%s", uuid_data);
        fclose(fuuid);
    }
  }
  else
  {
    fgets(uuid_data, sizeof(uuid_data), fuuid);
    fclose(fuuid);
  }

  snprintf( app_registry->app_uri, sizeof(app_registry->app_uri), "/%s" , uuid_data);
  GDIAL_LOGINFO("App:[%s] uuid:[%s] per Bootup", app_name, uuid_data);

  return app_registry;
}
