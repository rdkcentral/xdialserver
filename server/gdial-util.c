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

#include <stdio.h>
#include <string.h>
#include <libsoup/soup.h>
#include "gdial-config.h"
#include "gdial-util.h"
#include "gdialservicelogging.h"

gchar *gdial_util_str_str_hashtable_to_string(const GHashTable *ht, const gchar *delimiter, gboolean newline, gsize *length) {

  g_return_val_if_fail(ht != NULL && length != NULL, NULL);
  if (delimiter == NULL) delimiter = " ";

  GHashTableIter iter;
  gpointer key, value;
  g_hash_table_iter_init(&iter, (GHashTable *)ht);
  GString *value_buf = g_string_new("");
  while (g_hash_table_iter_next(&iter, &key, &value)) {
    g_string_append_printf(value_buf, "%s%s%s%s", (gchar *)key, (gchar *)delimiter, (gchar *)value, newline ? "\r\n" : "");
  }
  *length = value_buf->len;
  return g_string_free(value_buf, FALSE);
}

gchar *gdial_util_str_str_hashtable_to_xml_string(const GHashTable *ht, gsize *length) {

  g_return_val_if_fail(ht != NULL, NULL);

  GHashTableIter iter;
  gpointer key, value;
  g_hash_table_iter_init(&iter, (GHashTable *)ht);
  GString *value_buf = g_string_new("");
  while (g_hash_table_iter_next(&iter, &key, &value)) {
    g_string_append_printf(value_buf, "%s=\"%s\"", (gchar *)key, (gchar *)value);
  }
  if (length) *length = value_buf->len;
  return g_string_free(value_buf, FALSE);
}

gboolean gdial_util_str_str_hashtable_from_string(const gchar *ht_str, gsize length, GHashTable *ht) {
  g_return_val_if_fail(ht_str != NULL && ht != NULL, FALSE);
  char key[GDIAL_APP_DIAL_DATA_MAX_KV_LEN+1] = {0};
  char value[GDIAL_APP_DIAL_DATA_MAX_KV_LEN+1] = {0};
  gsize used = 0;
  int ret = 0;

  while( used < length && (ret = sscanf(&ht_str[used], "%"GDIAL_APP_DIAL_DATA_MAX_KV_LEN_STR"s %"GDIAL_APP_DIAL_DATA_MAX_KV_LEN_STR"s""\r\n", key, value)) > 0) {
    g_hash_table_insert(ht, g_strdup(key), g_strdup(value));
    used += strlen(key) + strlen(value) + strlen(" \r\n");
  }
  return TRUE;
}

GHashTable * gdial_util_str_str_hashtable_merge(GHashTable *dst, const GHashTable *src) {
  if (dst && src) {
    GHashTableIter iter;
    gpointer key, value;
    g_hash_table_iter_init (&iter, (GHashTable *)src);
    while (g_hash_table_iter_next (&iter, &key, &value)) {
      g_hash_table_replace(dst, key, value);
    }
  }

  return dst;
}

gboolean gdial_util_is_ascii_printable(const gchar *data, gsize length) {
  g_return_val_if_fail(data != NULL && length, FALSE);

  while (length--) {
    if (g_ascii_isgraph(data[length]) || g_ascii_isspace(data[length])) {
    }
    else {
      return FALSE;
    }
  }
  return TRUE;
}

/*
 * A hashtable copy function that only works with string hash tables
 */
GHashTable * gdial_util_str_str_hashtable_dup(const GHashTable *src) {
  g_return_val_if_fail(src, NULL);

  GHashTable *dst = g_hash_table_new_full(g_str_hash, g_str_equal, g_free, g_free);
  g_return_val_if_fail(dst, NULL);

  GHashTableIter iter;
  gpointer key, value;
  g_hash_table_iter_init (&iter, (GHashTable *)src);
  while (g_hash_table_iter_next(&iter, &key, &value)) {
    /* a NULL value results a dup NULL entry */
    g_hash_table_insert(dst, g_strdup(key), g_strdup(value));
  }

  return dst;
}

gboolean gdial_util_str_str_hashtable_equal(const GHashTable *ht1, const GHashTable *ht2) {

  if (ht1 == ht2) return TRUE;
  if (!ht1 || !ht2) return FALSE;
  if (g_hash_table_size((GHashTable *)ht1) != g_hash_table_size((GHashTable *)ht2)) return FALSE;

  GHashTableIter iter;
  gpointer key, value1, value2;
  g_hash_table_iter_init(&iter, (GHashTable *)ht1);
  while (g_hash_table_iter_next(&iter, &key, &value1)) {
    value2 = g_hash_table_lookup((GHashTable *)ht2, (GHashTable *)key);
    if ((value1 == value2) || (value1 && value2 && g_strcmp0(value2, value1) == 0)) {
    }
    else {
      GDIAL_LOGERROR("breaking equal at key %s", (char *)key);
      return FALSE;
    }
  }

  return TRUE;
}
