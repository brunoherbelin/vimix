#include <sstream>
#include <iomanip>
using namespace std;

#include "GstToolkit.h"

string GstToolkit::time_to_string(guint64 t, time_string_mode m)
{
    if (t == GST_CLOCK_TIME_NONE) {
        switch (m) {
        case TIME_STRING_FIXED:
            return "00:00:00.00";
        case TIME_STRING_MINIMAL:
            return "0.0";
        default:
            return "00.00";
        }
    }

    guint ms =  GST_TIME_AS_MSECONDS(t);
    guint s = ms / 1000;
    ostringstream oss;

    // MINIMAL: keep only the 2 higher values (most significant)
    if (m == TIME_STRING_MINIMAL) {
        int count = 0;
        if (s / 3600) {
            oss << s / 3600 << ':';
            count++;
        }
        if ((s % 3600) / 60) {
            oss << (s % 3600) / 60 << ':';
            count++;
        }
        if (count < 2) {
            oss << setw(count > 0 ? 2 : 1) << setfill('0') << (s % 3600) % 60;
            count++;
        }
        if (count < 2 )
            oss << '.'<< setw(1) << setfill('0') << (ms % 1000) / 10;
    }
    else {
        // TIME_STRING_FIXED : fixed length string (11 chars) HH:mm:ss.ii"
        // TIME_STRING_RIGHT : always show the right part (seconds), not the min or hours if none
        if (m == TIME_STRING_FIXED || (s / 3600) )
            oss << setw(2) << setfill('0') << s / 3600 << ':';
        if (m == TIME_STRING_FIXED || ((s % 3600) / 60) )
            oss << setw(2) << setfill('0') << (s % 3600) / 60 << ':';
        oss << setw(2) << setfill('0') << (s % 3600) % 60 << '.';
        oss << setw(2) << setfill('0') << (ms % 1000) / 10;
    }

    return oss.str();
}


list<string> GstToolkit::all_plugins()
{
    list<string> pluginlist;
    GList *l, *g;

    l = gst_registry_get_plugin_list (gst_registry_get ());

    for (g = l; g; g = g->next) {
        GstPlugin *plugin = GST_PLUGIN (g->data);
        pluginlist.push_front( string( gst_plugin_get_name (plugin) ) );
    }

    gst_plugin_list_free (l);

    return pluginlist;
}


list<string> GstToolkit::all_plugin_features(string pluginname)
{
    list<string> featurelist;
    GList *l, *g;

    l = gst_registry_get_feature_list_by_plugin (gst_registry_get (), pluginname.c_str());

    for (g = l; g; g = g->next) {
        GstPluginFeature *feature = GST_PLUGIN_FEATURE (g->data);
        featurelist.push_front( string( gst_plugin_feature_get_name (feature) ) );
    }

    gst_plugin_feature_list_free (l);

    return featurelist;
}

bool GstToolkit::enable_feature (string name, bool enable) {
    GstRegistry *registry = NULL;
    GstElementFactory *factory = NULL;

    registry = gst_registry_get();
    if (!registry) return false;

    factory = gst_element_factory_find (name.c_str());
    if (!factory) return false;

    if (enable) {
        gst_plugin_feature_set_rank (GST_PLUGIN_FEATURE (factory), GST_RANK_PRIMARY + 1);
    }
    else {
        gst_plugin_feature_set_rank (GST_PLUGIN_FEATURE (factory), GST_RANK_NONE);
    }

    gst_registry_add_feature (registry, GST_PLUGIN_FEATURE (factory));
    return true;
}


string GstToolkit::gst_version()
{
    std::ostringstream oss;
    oss << GST_VERSION_MAJOR << '.' << GST_VERSION_MINOR << '.';
    oss << std::setw(2) << setfill('0') << GST_VERSION_MICRO ;
    if (GST_VERSION_NANO > 0)
        oss << ( (GST_VERSION_NANO < 2 ) ? " - (CVS)" : " - (Prerelease)");

    return oss.str();
}

////EXAMPLE :
///
//v4l2deviceprovider, udev-probed=(boolean)true,
//device.bus_path=(string)pci-0000:00:14.0-usb-0:2:1.0,
//sysfs.path=(string)/sys/devices/pci0000:00/0000:00:14.0/usb1/1-2/1-2:1.0/video4linux/video0,
//device.bus=(string)usb,
//device.subsystem=(string)video4linux,
//device.vendor.id=(string)1bcf,
//device.vendor.name=(string)"Sunplus\\x20IT\\x20Co\\x20",
//device.product.id=(string)2286,
//device.product.name=(string)"AUSDOM\ FHD\ Camera:\ AUSDOM\ FHD\ C",
//device.serial=(string)Sunplus_IT_Co_AUSDOM_FHD_Camera,
//device.capabilities=(string):capture:,
//device.api=(string)v4l2,
//device.path=(string)/dev/video0,
//v4l2.device.driver=(string)uvcvideo,
//v4l2.device.card=(string)"AUSDOM\ FHD\ Camera:\ AUSDOM\ FHD\ C",
//v4l2.device.bus_info=(string)usb-0000:00:14.0-2,
//v4l2.device.version=(uint)328748,
//v4l2.device.capabilities=(uint)2225078273,
//v4l2.device.device_caps=(uint)69206017;
//Device added: AUSDOM FHD Camera: AUSDOM FHD C - v4l2src device=/dev/video0

//v4l2deviceprovider, udev-probed=(boolean)true,
//device.bus_path=(string)pci-0000:00:14.0-usb-0:4:1.0,
//sysfs.path=(string)/sys/devices/pci0000:00/0000:00:14.0/usb1/1-4/1-4:1.0/video4linux/video2,
//device.bus=(string)usb,
//device.subsystem=(string)video4linux,
//device.vendor.id=(string)046d,
//device.vendor.name=(string)046d,
//device.product.id=(string)080f,
//device.product.name=(string)"UVC\ Camera\ \(046d:080f\)",
//device.serial=(string)046d_080f_3EA77580,
//device.capabilities=(string):capture:,
//device.api=(string)v4l2,
//device.path=(string)/dev/video2,
//v4l2.device.driver=(string)uvcvideo,
//v4l2.device.card=(string)"UVC\ Camera\ \(046d:080f\)",
//v4l2.device.bus_info=(string)usb-0000:00:14.0-4,
//v4l2.device.version=(uint)328748,
//v4l2.device.capabilities=(uint)2225078273,
//v4l2.device.device_caps=(uint)69206017; // decimal of hexadecimal v4l code Device Caps      : 0x04200001
//Device added: UVC Camera (046d:080f) - v4l2src device=/dev/video2

static gboolean
my_bus_func (GstBus * bus, GstMessage * message, gpointer user_data)
{
   GstDevice *device;
   gchar *name;
   gchar *stru;

   switch (GST_MESSAGE_TYPE (message)) {
     case GST_MESSAGE_DEVICE_ADDED:
       gst_message_parse_device_added (message, &device);
       name = gst_device_get_display_name (device);

       stru = gst_structure_to_string( gst_device_get_properties(device) );
       g_print("%s \n", stru);
       g_print("Device added: %s - %ssrc device=%s\n", name,
               gst_structure_get_string(gst_device_get_properties(device), "device.api"),
               gst_structure_get_string(gst_device_get_properties(device), "device.path"));

       g_free (name);
       gst_object_unref (device);
       break;
     case GST_MESSAGE_DEVICE_REMOVED:
       gst_message_parse_device_removed (message, &device);
       name = gst_device_get_display_name (device);
       g_print("Device removed: %s\n", name);
       g_free (name);
       gst_object_unref (device);
       break;
     default:
       break;
   }

   return G_SOURCE_CONTINUE;
}

GstDeviceMonitor *GstToolkit::setup_raw_video_source_device_monitor()
{
   GstDeviceMonitor *monitor;
   GstBus *bus;
   GstCaps *caps;

   monitor = gst_device_monitor_new ();

   bus = gst_device_monitor_get_bus (monitor);
   gst_bus_add_watch (bus, my_bus_func, NULL);
   gst_object_unref (bus);

   caps = gst_caps_new_empty_simple ("video/x-raw");
   gst_device_monitor_add_filter (monitor, "Video/Source", caps);
   gst_caps_unref (caps);

   gst_device_monitor_set_show_all_devices(monitor, true);

   gst_device_monitor_start (monitor);

   GList *devices = gst_device_monitor_get_devices(monitor);

   GList *tmp;
   for (tmp = devices; tmp ; tmp = tmp->next ) {

       GstDevice *device = (GstDevice *) tmp->data;

       gchar *name = gst_device_get_display_name (device);
       g_print("Device already plugged: %s - %ssrc device=%s\n", name,
               gst_structure_get_string(gst_device_get_properties(device), "device.api"),
               gst_structure_get_string(gst_device_get_properties(device), "device.path"));

       g_free (name);
   }
   g_list_free(devices);

   return monitor;
}

