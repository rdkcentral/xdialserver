/*
 * If not stated otherwise in this file or this component's LICENSE file the
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

#ifndef GDIAL_SSDP_H_
#define GDIAL_SSDP_H_

#include <libsoup/soup.h>
#include "gdial-options.h"
#include <stdbool.h>

G_BEGIN_DECLS

int gdial_ssdp_new(SoupServer *server, GDialOptions *options, const gchar *random_uuid);
int gdial_ssdp_destroy();
int gdial_ssdp_set_available(bool activationStatus, const gchar *friendlyName);
int gdial_ssdp_set_friendlyname(const gchar *friendlyName);
int gdial_ssdp_set_manufacturername(const gchar *manufacturer_name);
int gdial_ssdp_set_modelname(const gchar *model_name);
G_END_DECLS

#endif
