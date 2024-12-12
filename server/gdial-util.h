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

#ifndef GDIAL_UTIL_H_
#define GDIAL_UTIL_H_

#include <glib.h>
G_BEGIN_DECLS

gchar *gdial_util_str_str_hashtable_to_string(const GHashTable *ht, const gchar *delimiter, gboolean newline, gsize *length);
gboolean gdial_util_str_str_hashtable_from_string(const gchar *ht_str, gsize length, GHashTable *ht);
gchar *gdial_util_str_str_hashtable_to_xml_string(const GHashTable *ht, gsize *length);
GHashTable *gdial_util_str_str_hashtable_merge(GHashTable *ht_dst, const GHashTable *ht_src);
gboolean gdial_util_str_str_hashtable_equal(const GHashTable *ht1, const GHashTable *ht2);
GHashTable * gdial_util_str_str_hashtable_dup(const GHashTable *src);
gboolean gdial_util_is_ascii_printable(const gchar *data, gsize length);

G_END_DECLS
#endif
