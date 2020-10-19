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

static void soup_message_weak_ref_callback(gpointer user_data, GObject *obj);
static void server_request_remove_callback (SoupMessage *msg) {
  DialShieldConnectionContext * conn_context = (DialShieldConnectionContext *) g_hash_table_lookup(active_conns_, msg);
  if (conn_context) {
    if (conn_context->read_timeout_source != 0) {
      g_print_with_timestamp("server_request_remove_callback tid=[%lx] msg=%p timeout source %d removed\r\n",
        pthread_self(), msg, conn_context->read_timeout_source);
      g_source_remove(conn_context->read_timeout_source);
    }
    g_hash_table_remove(active_conns_, msg);
    g_free(conn_context);
  }
}

static gboolean soup_message_read_timeout_callback(gpointer user_data) {
  SoupMessage *msg = (SoupMessage*)user_data;
  DialShieldConnectionContext * conn_context = (DialShieldConnectionContext *)g_hash_table_lookup(active_conns_, msg);
  g_print_with_timestamp("soup_message_read_timeout_callback tid=[%lx] msg=%p\r\n", pthread_self(), msg);
  if (conn_context) {
    conn_context->read_timeout_source = 0;
    g_socket_close(conn_context->read_gsocket, NULL);//this will trigger abort callback
  }
  return G_SOURCE_REMOVE;;
}


static void server_request_read_callback (SoupServer *server, SoupMessage *msg,
    SoupClientContext *context, gpointer data) {
  g_print_with_timestamp("server_request_read_callback tid=[%lx] msg=%p\r\n", pthread_self(), msg);
  g_object_weak_unref(G_OBJECT(msg), (GWeakNotify)soup_message_weak_ref_callback, msg);
  server_request_remove_callback(msg);
}

static void server_request_finished_callback (SoupServer *server, SoupMessage *msg,
    SoupClientContext *context, gpointer data) {
}

static void server_request_aborted_callback (SoupServer *server, SoupMessage *msg,
    SoupClientContext *context, gpointer data) {
  g_print_with_timestamp("server_request_aborted_callback tid=[%lx] msg=%p\r\n", pthread_self(), msg);
  g_object_weak_unref(G_OBJECT(msg), (GWeakNotify)soup_message_weak_ref_callback, msg);
  server_request_remove_callback(msg);
}

static void soup_message_weak_ref_callback(gpointer user_data, GObject *obj) {
  SoupMessage *msg0=(SoupMessage*)obj;
  SoupMessage *msg=(SoupMessage*)user_data;
  assert(msg0==msg);
  g_print_with_timestamp("soup_message_weak_ref_callback tid=[%lx] msg=%p\r\n", pthread_self(), msg);
  server_request_remove_callback(msg);
}

static void server_request_started_callback (SoupServer *server, SoupMessage *msg,
    SoupClientContext *context, gpointer data) {

  static const int throttle = GDIAL_THROTTLE_DELAY_US;
  DialShieldConnectionContext *conn_context = g_new(DialShieldConnectionContext, 1);
  guint read_timeout_source = g_timeout_add(2000, (GSourceFunc)soup_message_read_timeout_callback, msg);
  conn_context->read_gsocket = soup_client_context_get_gsocket(context);
  conn_context->read_timeout_source = read_timeout_source;
  g_print_with_timestamp("server_request_started_callback tid=[%lx] msg=%p timeout source %d added with socket fd = %d\r\n",
    pthread_self(), msg, read_timeout_source, g_socket_get_fd(conn_context->read_gsocket));
  g_hash_table_insert(active_conns_, msg, conn_context);
  g_object_weak_ref(G_OBJECT(msg), (GWeakNotify)soup_message_weak_ref_callback, msg);
  usleep(throttle);
}

void gdial_shield_init(void) {
  active_conns_ = g_hash_table_new(g_direct_hash, g_direct_equal);
}

void gdial_shield_server(SoupServer *server) {
  g_return_if_fail(server != NULL);
  g_signal_connect(server, "request_started", G_CALLBACK(server_request_started_callback),  NULL);
  g_signal_connect(server, "request_read",    G_CALLBACK(server_request_read_callback),     NULL);
  g_signal_connect(server, "request_finished",G_CALLBACK(server_request_finished_callback), NULL);
  g_signal_connect(server, "request_aborted", G_CALLBACK(server_request_aborted_callback),  NULL);
}

void gdial_shield_term(void) {
  GHashTableIter iter;
  gpointer key, value;
  printf("gdial_shield_term: hash_table_size start= %d\r\n", g_hash_table_size(active_conns_));
  g_hash_table_iter_init(&iter, (GHashTable *)active_conns_);
  while (g_hash_table_iter_next(&iter, &key, &value)) {
    SoupMessage *msg = (SoupMessage *)key;
    server_request_remove_callback(msg);
  }
  printf("gdial_shield_term: hash_table_size end= %d\r\n", g_hash_table_size(active_conns_));
  g_hash_table_unref(active_conns_);
  active_conns_ = NULL;
}
