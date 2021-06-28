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

#ifndef GDIAL_OPTIONS_H_
#define GDIAL_OPTIONS_H_

#include <glib.h>

#define FRIENDLY_NAME_OPTION 'F'
#define FRIENDLY_NAME_OPTION_LONG "friendly-name"
#define FRIENDLY_NAME_DESCRIPTION "Device Friendly Name"

#define MODELNAME_OPTION 'M'
#define MODELNAME_OPTION_LONG "model-name"
#define MODELNAME_DESCRIPTION "Model name of the device"

#define MANUFACTURER_OPTION 'R'
#define MANUFACTURER_OPTION_LONG "manufacturer"
#define MANUFACTURER_DESCRIPTION "Manufacturer of the device"

#define UUID_OPTION 'U'
#define UUID_OPTION_LONG "uuid-name"
#define UUID_DESCRIPTION "UUID of the device"

#define IFNAME_OPTION 'I'
#define IFNAME_OPTION_LONG "network-interface"
#define IFNAME_DESCRIPTION "network interface to be used"

#define APP_LIST_OPTION 'A'
#define APP_LIST_OPTION_LONG "app-list"
#define APP_LIST_DESCRIPTION "A preset list of apps to support"

typedef struct {
  gchar *friendly_name;
  gchar *manufacturer;
  gchar *model_name;
  gchar *uuid;
  gchar *iface_name;
  gchar *app_list;
} GDialOptions;

#endif
