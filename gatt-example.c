/* An example of a GATT service that implements the same functionality as the BlueZ gatt-service
 * but does not use any GPL library code
 */

#include <errno.h>
#include <stdio.h>
#include <stdbool.h>
#include <execinfo.h>
#include <signal.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <gio/gio.h>

#include "bluez-gdbus.h"

#define BLUEZ_SERVICE			    "org.bluez"
#define BLUEZ_ADAPTER_PATH          "/org/bluez/hci0"
#define BLUEZ_MANAGER_PATH		    "/"
#define BLUEZ_GATT_MGR_INTERFACE	"org.bluez.GattManager1"
#define BLUEZ_ADAPTER_INTERFACE		"org.bluez.Adapter1"
#define BLUEZ_DEVICE_INTERFACE		"org.bluez.Device1"
#define GATT_SERVICE_IFACE		    "org.bluez.GattService1"
#define GATT_CHR_IFACE			    "org.bluez.GattCharacteristic1"
#define GATT_DESCRIPTOR_IFACE		"org.bluez.GattDescriptor1"
#define EXAMPLE_BUS_NAME            "com.example"

/* Immediate Alert Service UUID */
#define IAS_UUID			        "00001802-0000-1000-8000-00805f9b34fb"
#define ALERT_LEVEL_CHR_UUID		"00002a06-0000-1000-8000-00805f9b34fb"

/* Random UUID for testing purpose */
#define READ_WRITE_DESCRIPTOR_UUID	"8260c653-1a54-426b-9e36-e84c238bc669"

static GDBusConnection *g_connection;
static Adapter1 *adapter;

void handler(int sig) {
  void *array[10];
  size_t size;

  // get void*'s for all entries on the stack
  size = backtrace(array, 10);

  // print out all the frames to stderr
  fprintf(stderr, "Error: signal %d:\n", sig);
  backtrace_symbols_fd(array, size, STDERR_FILENO);
  exit(1);
}


static void
async_adapter_cb(GObject *source_object, GAsyncResult *res, gpointer user_data)
{
    GError *error = NULL;
    const gchar *alias;

    g_print("async_adapter_cb called\n");

    adapter = adapter1_proxy_new_finish(res, &error);

    if (!adapter) {
        g_printerr("Error getting adapter1: %s\n", error->message);
        return;
    }

    /* This block of code gets and sets various Adapter1 properties */
    alias = adapter1_get_alias(adapter);
    gboolean powered;
    powered = adapter1_get_powered(adapter);
    g_print("Adapter1 Alias value: %s\n", alias);
    g_print("Adapter1 Powered value: %d\n", powered);
    adapter1_set_alias(adapter, "New Alias Val");
    adapter1_set_powered(adapter, TRUE);
}

static void
async_reg_adv_cb(GObject *source_object, GAsyncResult *res, gpointer user_data)
{
    GError *error = NULL;

    if (!leadvertising_manager1_call_register_advertisement_finish(LEADVERTISING_MANAGER1(source_object),
                                                       res,
                                                       &error)) {
        g_printerr("Failed RegisterAdvertisement %s\n", error->message);
    } else {
        g_print("Sucessfully registered /advertisement1\n");
    }
}

static void
async_reg_app_cb(GObject *source_object, GAsyncResult *res, gpointer user_data)
{
    GError *error = NULL;


    if (!gatt_manager1_call_register_application_finish(GATT_MANAGER1(source_object), res, &error)) {
        g_printerr("RegisterApplication call error: %s\n",  error->message);
    } else {
        g_print("RegisterApplication call OK\n");
    }

    adapter1_proxy_new(g_connection,
                       G_DBUS_PROXY_FLAGS_DO_NOT_AUTO_START,
                       BLUEZ_SERVICE,
                       BLUEZ_ADAPTER_PATH,
                       NULL,
                       async_adapter_cb,
                       NULL);

}

static gboolean
on_chr_write (GattCharacteristic1 *characteristic,
                GDBusMethodInvocation *invocation,
                gchar *value,
                GVariant *options,
                gpointer user_data)
{
    return TRUE;
}

static void
on_bus_acquired (GDBusConnection *connection,
                 const gchar     *name,
                 gpointer         user_data)
{
    static GattService1 *svc;
    static GDBusObjectManagerServer *manager;
    static const char *ias_alert_level_flags[] = { "write-without-response", NULL };
    static const char *desc_flags[] = { "read", "write", NULL };
    GattCharacteristic1 *alert_chr;
    GattDescriptor1 *read_desc;
    ObjectSkeleton *object;

    g_print("Acquiring Bus Name: %s\n", name);

    manager = g_dbus_object_manager_server_new("/service");

    if (!manager) {
        g_printerr("Error setting up ObjectManager(s)\n");
        return;
    }

    /* Export manager on connection */
    g_dbus_object_manager_server_set_connection(manager, connection);

    svc = gatt_service1_skeleton_new();
    object = object_skeleton_new("/service/service1");
    object_skeleton_set_gatt_service1(object, svc);

    /* Set IAS Service  properties */
    gatt_service1_set_uuid(svc, IAS_UUID);
    gatt_service1_set_primary(svc, TRUE);

    g_dbus_object_manager_server_export(manager, G_DBUS_OBJECT_SKELETON(object));
    g_object_unref(object);

    /* Create Alert Level GATT characteristics */
    alert_chr = gatt_characteristic1_skeleton_new();
    object = object_skeleton_new("/service/service1/characteristic1");
    object_skeleton_set_gatt_characteristic1(object, alert_chr);
    gatt_characteristic1_set_service(alert_chr, "/service/service1");
    gatt_characteristic1_set_uuid(alert_chr, ALERT_LEVEL_CHR_UUID);
    gatt_characteristic1_set_flags(alert_chr, ias_alert_level_flags);

    g_dbus_object_manager_server_export(manager, G_DBUS_OBJECT_SKELETON(object));
    g_object_unref(object);

    /* Create Read/Write Descriptor */
    read_desc = gatt_descriptor1_skeleton_new();
    object = object_skeleton_new("/service/service1/descriptor1");
    object_skeleton_set_gatt_descriptor1(object, read_desc);
    gatt_characteristic1_set_service(alert_chr, "/service/service1");
    gatt_characteristic1_set_uuid(alert_chr, READ_WRITE_DESCRIPTOR_UUID);
    gatt_characteristic1_set_flags(alert_chr, desc_flags);

    g_dbus_object_manager_server_export(manager, G_DBUS_OBJECT_SKELETON(object));
    g_object_unref(object);

    /* Handle WriteValue() D-Bus method invocation for characteristic */
    g_signal_connect(alert_chr,
                    "handle-write-value",
                    G_CALLBACK(on_chr_write),
                    NULL);

    g_dbus_object_manager_server_export(manager, G_DBUS_OBJECT_SKELETON(object));
    g_object_unref(object);
}


static void
on_name_acquired (GDBusConnection *connection,
                  const gchar     *name,
                  gpointer         user_data)
{
    const char *svc_uuids[] = { IAS_UUID, NULL };
    static GattManager1 *gatt_mgr;
    static LEAdvertisingManager1 *adv_manager;
    static LEAdvertisement1 *adv;
    g_print ("%s bus acquired\n", name);
    GVariantBuilder *b;
    GVariant *dict;
    GError *error;

    /* Set a global connection reference */
    g_connection = connection;

    /* Get LEAdvertisingManager1 Proxy */
    adv_manager = leadvertising_manager1_proxy_new_sync(g_connection,
                                                       G_DBUS_PROXY_FLAGS_DO_NOT_AUTO_START,
                                                       BLUEZ_SERVICE,
                                                       BLUEZ_ADAPTER_PATH,
                                                       NULL,
                                                       &error);

    if (!adv_manager) {
        g_printerr("Unable to get LEAdvertisingManager1 proxy: %s\n", error->message);
        return;
    } else {
        g_print("Created LEAdvertisingManager1 proxy\n");
    }

    /* Create GATT Service Advertisement Object */
    adv = leadvertisement1_skeleton_new();

    const gchar *adv_type = "peripheral";
    leadvertisement1_set_type_(adv, adv_type);
    leadvertisement1_set_service_uuids(adv, svc_uuids);

    b = g_variant_builder_new(G_VARIANT_TYPE ("a{qv}"));
    g_variant_builder_add(b, "{qv}", 0xADDE, g_variant_new_bytestring("\xBE\xEF"));
    dict = g_variant_builder_end(b);

    leadvertisement1_set_manufacturer_data(adv, dict);
    g_variant_builder_unref(b);

    leadvertisement1_set_include_tx_power(adv, TRUE);

    /* Export Advertisement Object */
    g_dbus_interface_skeleton_export(G_DBUS_INTERFACE_SKELETON(adv), connection, "/advertisement1", &error);

    b = g_variant_builder_new(G_VARIANT_TYPE ("a{sv}"));
    dict = g_variant_builder_end(b);

    /* Enable advertising */
    leadvertising_manager1_call_register_advertisement(adv_manager,
                                                       "/advertisement1",
                                                       dict,
                                                       NULL,
                                                       async_reg_adv_cb,
                                                       NULL);

    g_dbus_interface_skeleton_flush(G_DBUS_INTERFACE_SKELETON(adv));

    gatt_mgr = gatt_manager1_proxy_new_sync(connection,
                       G_DBUS_PROXY_FLAGS_DO_NOT_AUTO_START,
                       BLUEZ_SERVICE,
                       BLUEZ_ADAPTER_PATH,
                       NULL,
                       &error);

    if (!gatt_mgr) {
        g_printerr("Failed to get GattManager1 instance: error: %s\n", error->message);
        return;
    }

    b = g_variant_builder_new(G_VARIANT_TYPE ("a{sv}"));
    dict = g_variant_builder_end(b);

    /* Call RegisterApplication method to register service */
    gatt_manager1_call_register_application(gatt_mgr, "/service", dict, NULL, async_reg_app_cb, user_data);
}

static void
on_name_lost (GDBusConnection *connection,
              const gchar     *name,
              gpointer         user_data)
{
    g_print ("Lost the name %s\n", name);
}

int main(int argc, char *argv[])
{
    GMainLoop *main_loop;
    guint id;

    signal(SIGSEGV, handler);

    main_loop = g_main_loop_new (NULL, FALSE);

    id = g_bus_own_name(G_BUS_TYPE_SYSTEM,
                       EXAMPLE_BUS_NAME,
                       G_BUS_NAME_OWNER_FLAGS_ALLOW_REPLACEMENT |
                       G_BUS_NAME_OWNER_FLAGS_REPLACE,
                       on_bus_acquired,
                       on_name_acquired,
                       on_name_lost,
                       main_loop,
                       NULL);

    g_main_loop_run(main_loop);

    g_main_loop_unref(main_loop);
    g_bus_unown_name(id);

	return 0;
}
