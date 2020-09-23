#include <algorithm>
#include <sstream>
#include <glm/gtc/matrix_transform.hpp>

#include "DeviceSource.h"

#include "defines.h"
#include "ImageShader.h"
#include "ImageProcessingShader.h"
#include "Resource.h"
#include "Primitives.h"
#include "Stream.h"
#include "Visitor.h"
#include "Log.h"


gboolean
Device::callback_device_monitor (GstBus * bus, GstMessage * message, gpointer user_data)
{
   GstDevice *device;
   gchar *name;
   gchar *stru;

   switch (GST_MESSAGE_TYPE (message)) {
     case GST_MESSAGE_DEVICE_ADDED: {
       gst_message_parse_device_added (message, &device);
       name = gst_device_get_display_name (device);
       manager().names_.push_back(name);
       g_free (name);

       std::ostringstream pipe;
       pipe << gst_structure_get_string(gst_device_get_properties(device), "device.api");
       pipe << "src device=";
       pipe << gst_structure_get_string(gst_device_get_properties(device), "device.path");
       manager().pipelines_.push_back(pipe.str());

       stru = gst_structure_to_string( gst_device_get_properties(device) );
       g_print("New device %s \n", stru);

       gst_object_unref (device);
   }
       break;
     case GST_MESSAGE_DEVICE_REMOVED: {
       gst_message_parse_device_removed (message, &device);
       name = gst_device_get_display_name (device);
       manager().remove(name);
       g_print("Device removed: %s\n", name);
       g_free (name);

       gst_object_unref (device);
   }
       break;
     default:
       break;
   }

   return G_SOURCE_CONTINUE;
}

void Device::remove(const char *device)
{
    std::vector< std::string >::iterator nameit = names_.begin();
    std::vector< std::string >::iterator pipeit = pipelines_.begin();
    while (nameit != names_.end()){

        if ( (*nameit).compare(device) == 0 )
        {
            names_.erase(nameit);
            pipelines_.erase(pipeit);
            break;
        }

        nameit++;
        pipeit++;
    }
}

Device::Device()
{
    GstBus *bus;
    GstCaps *caps;

    // create GStreamer device monitor to capture
    // when a device is plugged in or out
    monitor_ = gst_device_monitor_new ();

    bus = gst_device_monitor_get_bus (monitor_);
    gst_bus_add_watch (bus, callback_device_monitor, NULL);
    gst_object_unref (bus);

    caps = gst_caps_new_empty_simple ("video/x-raw");
    gst_device_monitor_add_filter (monitor_, "Video/Source", caps);
    gst_caps_unref (caps);

    gst_device_monitor_set_show_all_devices(monitor_, true);
    gst_device_monitor_start (monitor_);

    // initial fill of the list
    GList *devices = gst_device_monitor_get_devices(monitor_);
    GList *tmp;
    for (tmp = devices; tmp ; tmp = tmp->next ) {

        GstDevice *device = (GstDevice *) tmp->data;

        gchar *name = gst_device_get_display_name (device);
        names_.push_back(name);
        g_free (name);

        std::ostringstream pipe;
        pipe << gst_structure_get_string(gst_device_get_properties(device), "device.api");
        pipe << "src device=";
        pipe << gst_structure_get_string(gst_device_get_properties(device), "device.path");
        pipelines_.push_back(pipe.str());

    }
    g_list_free(devices);

}


int Device::numDevices() const
{
    return names_.size();
}

bool Device::exists(const std::string &device) const
{
    std::vector< std::string >::const_iterator d = std::find(names_.begin(), names_.end(), device);
    return d != names_.end();
}

std::string Device::name(int index) const
{
    if (index > -1 && index < names_.size())
        return names_[index];
    else
        return "";
}

std::string Device::pipeline(int index) const
{
    if (index > -1 && index < pipelines_.size())
        return pipelines_[index];
    else
        return "";
}

std::string Device::pipeline(const std::string &device) const
{
    std::string pip = "";
    std::vector< std::string >::const_iterator p = std::find(names_.begin(), names_.end(), device);
    if (p != names_.end())
    {
        int index = std::distance(names_.begin(), p);
        pip = pipelines_[index];
    }
    return pip;
}


////    std::string desc = "v4l2src ! video/x-raw,width=320,height=240,framerate=30/1 ! videoconvert";
////    std::string desc = "v4l2src ! jpegdec ! videoconvert";
//    std::string desc = "v4l2src ! image/jpeg,width=640,height=480,framerate=30/1 ! jpegdec ! videoscale ! videoconvert";

////        std::string desc = "v4l2src ! jpegdec ! videoscale ! videoconvert";

////    std::string desc = "videotestsrc pattern=snow is-live=true ";
////    std::string desc = "ximagesrc endx=800 endy=600 ! video/x-raw,framerate=15/1 ! videoscale ! videoconvert";


DeviceSource::DeviceSource() : StreamSource()
{
    // create stream
    stream_ = (Stream *) new Stream();

    // set icons TODO
    overlays_[View::MIXING]->attach( new Symbol(Symbol::EMPTY, glm::vec3(0.8f, 0.8f, 0.01f)) );
    overlays_[View::LAYER]->attach( new Symbol(Symbol::EMPTY, glm::vec3(0.8f, 0.8f, 0.01f)) );
}

void DeviceSource::setDevice(const std::string &devicename)
{
    device_ = devicename;
    Log::Notify("Creating Source with device '%s'", device_.c_str());

    std::string pipeline = Device::manager().pipeline(device_);
    pipeline += " ! jpegdec ! videoscale ! videoconvert";

    stream_->open( pipeline );
    stream_->play(true);
}

void DeviceSource::accept(Visitor& v)
{
    Source::accept(v);
    v.visit(*this);
}

bool DeviceSource::failed() const
{
    return stream_->failed() || !Device::manager().exists(device_);
}


