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

#ifndef GDIAL_APPREGISTRY_H_
#define GDIAL_APPREGISTRY_H_

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <netinet/in.h>
#include <sys/socket.h>

#include <glib.h>

G_BEGIN_DECLS

typedef struct _GDialAppRegistry {
  gchar *name;
  gboolean use_additional_data;
  gboolean is_singleton;
  GList *allowed_origins;
  GList *app_prefixes;
  GHashTable *properties;
} GDialAppRegistry;

void gdial_app_regstry_dispose (GDialAppRegistry *app_registry);
gboolean gdial_app_registry_is_allowed_origin(GDialAppRegistry *app_registry, const gchar *header_origin);
GDialAppRegistry* gdial_app_registry_new (const gchar *app_name, const GList *app_prefixes, GHashTable *properties, gboolean is_singleton, gboolean use_additional_data, const GList *allowed_origins);

G_END_DECLS

#endif
