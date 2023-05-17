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

#ifndef GDIAL_RESTSERVER_H_
#define GDIAL_RESTSERVER_H_

#include <glib.h>
#include <glib-object.h>
#include <libsoup/soup.h>

#include "gdial-config.h"
#include "gdial-app.h"
#include "gdial_app_registry.h"

G_BEGIN_DECLS

#define GDIAL_REST_SERVER_SOUP_INSTANCE "soup_instance"
#define GDIAL_LOCAL_REST_SERVER_SOUP_INSTANCE "local_soup_instance"
#define GDIAL_TYPE_REST_SERVER gdial_rest_server_get_type()
G_DECLARE_FINAL_TYPE(GDialRestServer, gdial_rest_server, GDIAL, REST_SERVER, GObject)

struct _GDialRestServer {
  GObject parent;
};

GDialRestServer *gdial_rest_server_new(SoupServer *rest_http_server,SoupServer *local_rest_http_server, gchar *random_uuid);
gboolean gdial_rest_server_register_app(GDialRestServer *self, const gchar *app_name, const GList *app_prefixes, GHashTable *properties, gboolean is_singleton, gboolean use_additional_data, const GList *allowed_origin);
gboolean gdial_rest_server_register_app_registry(GDialRestServer *self, GDialAppRegistry *app_registry);
gboolean gdial_rest_server_is_app_registered(GDialRestServer *self, const gchar *app_name);
gboolean gdial_rest_server_unregister_app(GDialRestServer *self, const gchar *app_name);
GDialApp *gdial_rest_server_find_app(GDialRestServer *self, const gchar *app_name);
gboolean gdial_rest_server_unregister_all_apps(GDialRestServer *self);

typedef struct _GDialAppRegistry GDialAppRegistry;

GDIAL_STATIC gboolean gdial_rest_server_is_allowed_origin(GDialRestServer *self, const gchar *header_origin, const gchar *app_name);
GDIAL_STATIC gchar *gdial_rest_server_new_additional_data_url(guint listening_port, const gchar *app_name, gboolean encode);
GDIAL_STATIC GDialAppRegistry *gdial_rest_server_find_app_registry(GDialRestServer *self, const gchar *app_name);

G_END_DECLS

#endif
