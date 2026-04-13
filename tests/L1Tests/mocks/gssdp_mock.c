/*
 * Minimal GSSDP mocks for L1 tests.
 * These avoid runtime dependency on host network/GSSDP behavior while
 * allowing gdial-ssdp.c code paths to execute for coverage.
 */

#include <glib-object.h>
#include <libgssdp/gssdp.h>

#ifndef HAVE_GSSDP_VERSION_1_2_OR_NEWER
GSSDPClient *gssdp_client_new(GMainContext *main_context, const char *iface, GError **error)
{
    (void)main_context;
    (void)iface;
    if (error) {
        *error = NULL;
    }
    return (GSSDPClient *)g_object_new(G_TYPE_OBJECT, NULL);
}
#else
GSSDPClient *gssdp_client_new(const char *iface, GError **error)
{
    (void)iface;
    if (error) {
        *error = NULL;
    }
    return (GSSDPClient *)g_object_new(G_TYPE_OBJECT, NULL);
}
#endif

void gssdp_client_append_header(GSSDPClient *client, const char *name, const char *value)
{
    (void)client;
    (void)name;
    (void)value;
}

void gssdp_client_remove_header(GSSDPClient *client, const char *name)
{
    (void)client;
    (void)name;
}

void gssdp_client_clear_headers(GSSDPClient *client)
{
    (void)client;
}

GSSDPResourceGroup *gssdp_resource_group_new(GSSDPClient *client)
{
    (void)client;
    return (GSSDPResourceGroup *)g_object_new(G_TYPE_OBJECT, NULL);
}

guint gssdp_resource_group_add_resource_simple(GSSDPResourceGroup *resource_group,
                                               const char *target,
                                               const char *usn,
                                               const char *location)
{
    (void)resource_group;
    (void)target;
    (void)usn;
    (void)location;
    return 1;
}

void gssdp_resource_group_set_available(GSSDPResourceGroup *resource_group, gboolean available)
{
    (void)resource_group;
    (void)available;
}

void gssdp_resource_group_remove_resource(GSSDPResourceGroup *resource_group, guint resource_id)
{
    (void)resource_group;
    (void)resource_id;
}
