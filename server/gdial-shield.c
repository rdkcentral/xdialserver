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
#include <glib.h>
#include <assert.h>
#include <libsoup/soup.h>

#include "gdial-config.h"
#include "gdial-debug.h"

typedef struct DialShieldConnectionContext {
  GSocket *read_gsocket;
  guint read_timeout_source;
} DialShieldConnectionContext;

static GHashTable *active_conns_ = NULL;
static pthread_mutex_t shield_mutex = PTHREAD_MUTEX_INITIALIZER;

static void soup_message_weak_ref_callback(gpointer user_data, GObject *obj);
static void server_request_remove_callback (SoupMessage *msg) {
  GDIAL_LOGTRACE("Entering ...");
  // FIX(Coverity): Mutex protection for hash table access
  // Reason: Prevent race conditions in multi-threaded environment
  // Impact: Thread-safe hash table operations. Public API unchanged.
  pthread_mutex_lock(&shield_mutex);
  DialShieldConnectionContext * conn_context = (DialShieldConnectionContext *) g_hash_table_lookup(active_conns_, msg);
  if (conn_context) {
    if (conn_context->read_timeout_source != 0) {
      g_print_with_timestamp("server_request_remove_callback tid=[%lx] msg=%p timeout source %d removed",
        pthread_self(), msg, conn_context->read_timeout_source);
      g_source_remove(conn_context->read_timeout_source);
    }
    // FIX(Coverity): Correct order - remove from hash table before freeing
    // Reason: Prevent use-after-free and ensure cleanup atomicity
    // Impact: Proper resource cleanup order. Public API unchanged.
    g_hash_table_remove(active_conns_, msg);
    g_free(conn_context);
    conn_context = NULL;
  }
  pthread_mutex_unlock(&shield_mutex);
  GDIAL_LOGTRACE("Exiting ...");
}

static gboolean soup_message_read_timeout_callback(gpointer user_data) {
  SoupMessage *msg = (SoupMessage*)user_data;
  GDIAL_LOGTRACE("Entering ...");
  // FIX(Coverity): Add mutex protection and explicit NULL check
  // Reason: Prevent NULL pointer dereference in concurrent environment
  // Impact: Thread-safe access with defensive NULL check. Public API unchanged.
  pthread_mutex_lock(&shield_mutex);
  DialShieldConnectionContext * conn_context = (DialShieldConnectionContext *)g_hash_table_lookup(active_conns_, msg);
  g_print_with_timestamp("soup_message_read_timeout_callback tid=[%lx] msg=%p", pthread_self(), msg);
  if (conn_context) {
    conn_context->read_timeout_source = 0;
    g_socket_close(conn_context->read_gsocket, NULL);//this will trigger abort callback
  }
  pthread_mutex_unlock(&shield_mutex);
  GDIAL_LOGTRACE("Exiting ...");
  return G_SOURCE_REMOVE;;
}


static void server_request_read_callback (SoupServer *server, SoupMessage *msg,
    SoupClientContext *context, gpointer data) {
  GDIAL_LOGTRACE("Entering ...");
  g_print_with_timestamp("server_request_read_callback tid=[%lx] msg=%p", pthread_self(), msg);
  g_object_weak_unref(G_OBJECT(msg), (GWeakNotify)soup_message_weak_ref_callback, msg);
  server_request_remove_callback(msg);
  GDIAL_LOGTRACE("Exiting ...");
}

static void server_request_finished_callback (SoupServer *server, SoupMessage *msg,
    SoupClientContext *context, gpointer data) {
    GDIAL_LOGTRACE("!!!");
}

static void server_request_aborted_callback (SoupServer *server, SoupMessage *msg,
    SoupClientContext *context, gpointer data) {
  GDIAL_LOGTRACE("Entering ...");
  g_print_with_timestamp("server_request_aborted_callback tid=[%lx] msg=%p", pthread_self(), msg);

  g_object_weak_unref(G_OBJECT(msg), (GWeakNotify)soup_message_weak_ref_callback, msg);
  server_request_remove_callback(msg);
  GDIAL_LOGTRACE("Exiting ...");
}

static void soup_message_weak_ref_callback(gpointer user_data, GObject *obj) {
  SoupMessage *msg0=(SoupMessage*)obj;
  SoupMessage *msg=(SoupMessage*)user_data;
  GDIAL_LOGTRACE("Entering ...");
  assert(msg0==msg);
  g_print_with_timestamp("soup_message_weak_ref_callback tid=[%lx] msg=%p", pthread_self(), msg);
  server_request_remove_callback(msg);
  GDIAL_LOGTRACE("Exiting ...");
}

static void server_request_started_callback (SoupServer *server, SoupMessage *msg,
    SoupClientContext *context, gpointer data) {

  static const int throttle = GDIAL_THROTTLE_DELAY_US;
  GDIAL_LOGTRACE("Entering ...");
  // FIX(Coverity): Use g_try_new to check for allocation failure
  // Reason: g_new aborts on failure; g_try_new returns NULL for error handling
  // Impact: Proper error handling for memory allocation. Public API unchanged.
  DialShieldConnectionContext *conn_context = g_try_new(DialShieldConnectionContext, 1);
  if (!conn_context) {
    GDIAL_LOGERROR("Failed to allocate connection context");
    GDIAL_LOGTRACE("Exiting ...");
    return;
  }
  guint read_timeout_source = g_timeout_add(2000, (GSourceFunc)soup_message_read_timeout_callback, msg);
  conn_context->read_gsocket = soup_client_context_get_gsocket(context);
  conn_context->read_timeout_source = read_timeout_source;
  g_print_with_timestamp("server_request_started_callback tid=[%lx] msg=%p timeout source %d added with socket fd = %d",
    pthread_self(), msg, read_timeout_source, g_socket_get_fd(conn_context->read_gsocket));
  // FIX(Coverity): Add mutex protection for hash table insert
  // Reason: Prevent race condition with concurrent hash table access
  // Impact: Thread-safe insertion. Public API unchanged.
  pthread_mutex_lock(&shield_mutex);
  g_hash_table_insert(active_conns_, msg, conn_context);
  pthread_mutex_unlock(&shield_mutex);
  g_object_weak_ref(G_OBJECT(msg), (GWeakNotify)soup_message_weak_ref_callback, msg);
  usleep(throttle);
  GDIAL_LOGTRACE("Exiting ...");
}

void gdial_shield_init(void) {
  GDIAL_LOGTRACE("Entering ...");
  active_conns_ = g_hash_table_new(g_direct_hash, g_direct_equal);
  pthread_mutex_init(&shield_mutex, NULL);
  GDIAL_LOGTRACE("Exiting ...");
}

void gdial_shield_server(SoupServer *server) {
  g_return_if_fail(server != NULL);
  GDIAL_LOGTRACE("Entering ...");
  g_signal_connect(server, "request_started", G_CALLBACK(server_request_started_callback),  NULL);
  g_signal_connect(server, "request_read",    G_CALLBACK(server_request_read_callback),     NULL);
  g_signal_connect(server, "request_finished",G_CALLBACK(server_request_finished_callback), NULL);
  g_signal_connect(server, "request_aborted", G_CALLBACK(server_request_aborted_callback),  NULL);
  GDIAL_LOGTRACE("Exiting ...");
}

void gdial_shield_term(void) {
  GHashTableIter iter;
  gpointer key, value;
  GDIAL_LOGTRACE("Entering ...");
  g_return_if_fail(active_conns_ != NULL);
  GDIAL_LOGINFO("gdial_shield_term: hash_table_size start= %d", g_hash_table_size(active_conns_));
  pthread_mutex_lock(&shield_mutex);
  g_hash_table_iter_init(&iter, (GHashTable *)active_conns_);
  while (g_hash_table_iter_next(&iter, &key, &value)) {
    SoupMessage *msg = (SoupMessage *)key;
    DialShieldConnectionContext *conn_context = (DialShieldConnectionContext *)value;
    if (conn_context && conn_context->read_timeout_source != 0) {
      g_source_remove(conn_context->read_timeout_source);
    }
    g_free(conn_context);
  }
  g_hash_table_remove_all(active_conns_);
  GDIAL_LOGINFO("gdial_shield_term: hash_table_size end= %d", g_hash_table_size(active_conns_));
  g_hash_table_unref(active_conns_);
  active_conns_ = NULL;
  pthread_mutex_unlock(&shield_mutex);
  pthread_mutex_destroy(&shield_mutex);
  GDIAL_LOGTRACE("Exiting ...");
}
