/*
 * If not stated otherwise in this file or this component's Licenses.txt file the
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

#include <gdial_app_registry.h>


#define GDIAL_STR_ENDS_WITH(s1, s2) ((s1 != NULL) && (s2 != NULL) && ((strlen(s2) == 0) || (g_str_has_suffix(s1, s2))))

void gdial_app_regstry_dispose (GDialAppRegistry *app_registry) {
  g_return_val_if_fail(app_registry != NULL, TRUE);
  printf ("freeing app name:%s\n", app_registry->name);
  g_free(app_registry->name);
  g_list_free_full(app_registry->allowed_origins, g_free);
  g_list_free_full(app_registry->app_prefixes, g_free);
  if (app_registry->properties) {
    g_hash_table_destroy (app_registry->properties);
    app_registry->properties = NULL;
  }
  free(app_registry);
  return;
}

gboolean gdial_app_registry_is_allowed_origin(GDialAppRegistry *app_registry, const gchar *header_origin) {
  gboolean is_allowed = FALSE;
  if (app_registry) {
    GList *allowed_origins = app_registry->allowed_origins;
    if(allowed_origins == NULL){
      is_allowed = TRUE;
    }
    else
    {
      while(allowed_origins) {
        gchar *origin = (gchar *)allowed_origins->data;
        if (GDIAL_STR_ENDS_WITH(header_origin, origin)) {
          is_allowed = TRUE;
          break;
        }
        allowed_origins = allowed_origins->next;
      }
    }
  }
  return is_allowed;
}

GDialAppRegistry* gdial_app_registry_new (const gchar *app_name, const GList *app_prefixes, const GHashTable *properties, gboolean is_singleton, gboolean use_additional_data, const GList *allowed_origins) {
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
      g_print("inside register app properties key:%s value:%s\n",key, value);
      g_hash_table_insert(app_registry->properties,g_strdup(key),g_strdup(value));
    }
  }

  while(allowed_origins) {
    app_registry->allowed_origins = g_list_prepend(app_registry->allowed_origins, g_strdup(allowed_origins->data));
    allowed_origins = allowed_origins->next;
  }
  return app_registry;
}
