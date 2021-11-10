/*
 * This file is part of vimix - video live mixer
 *
 * **Copyright** (C) 2020-2021 Bruno Herbelin <bruno.herbelin@gmail.com>
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program. If not, see <https://www.gnu.org/licenses/>.
**/

#include <algorithm>
#include <sstream>
#include <glm/gtc/matrix_transform.hpp>

#include <gst/pbutils/gstdiscoverer.h>
#include <gst/pbutils/pbutils.h>
#include <gst/gst.h>

#include "DeviceSource.h"

#include "defines.h"
#include "ImageShader.h"
#include "ImageProcessingShader.h"
#include "Resource.h"
#include "Decorations.h"
#include "Stream.h"
#include "Visitor.h"
#include "Log.h"

#ifndef NDEBUG
#define DEVICE_DEBUG
//#define GST_DEVICE_DEBUG
#endif



#if defined(APPLE)
std::string gst_plugin_device = "avfvideosrc";
std::string gst_plugin_vidcap = "avfvideosrc capture-screen=true";
#else
std::string gst_plugin_device = "v4l2src";
std::string gst_plugin_vidcap = "ximagesrc";
#endif

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

std::string pipelineForDevice(GstDevice *device, uint index)
{
    std::ostringstream pipe;
    const gchar *str = gst_structure_get_string(gst_device_get_properties(device), "device.api");

    if (str && gst_plugin_device.find(str) != std::string::npos)
    {
        pipe << gst_plugin_device;

#if defined(APPLE)
        pipe << " device-index=" << index;
#else
        str = gst_structure_get_string(gst_device_get_properties(device), "device.path");
        if (str)
            pipe << " device=" << str;
#endif
    }

    return pipe.str();
}

gboolean
Device::callback_device_monitor (GstBus *, GstMessage * message, gpointer )
{
   GstDevice *device;
   gchar *name;

   switch (GST_MESSAGE_TYPE (message)) {
     case GST_MESSAGE_DEVICE_ADDED: {
       gst_message_parse_device_added (message, &device);
       name = gst_device_get_display_name (device);

       // ignore if already in the list
       if ( std::find(manager().src_name_.begin(), manager().src_name_.end(), name) != manager().src_name_.end())
        break;

       manager().src_name_.push_back(name);
#ifdef GST_DEVICE_DEBUG
       gchar *stru = gst_structure_to_string( gst_device_get_properties(device) );
       g_print("\nDevice %s plugged : %s\n", name, stru);
       g_free (stru);
#endif
       g_free (name);

       std::string p = pipelineForDevice(device, manager().src_description_.size());
       manager().src_description_.push_back(p);

       DeviceConfigSet confs = getDeviceConfigs(p);
       manager().src_config_.push_back(confs);

       manager().list_uptodate_ = false;

       gst_object_unref (device);
   }
       break;
     case GST_MESSAGE_DEVICE_REMOVED: {
       gst_message_parse_device_removed (message, &device);
       name = gst_device_get_display_name (device);
       manager().remove(name);
#ifdef GST_DEVICE_DEBUG
       g_print("\nDevice %s unplugged\n", name);
#endif
       g_free (name);

       manager().list_uptodate_ = false;

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
    std::vector< std::string >::iterator nameit   = src_name_.begin();
    std::vector< std::string >::iterator descit   = src_description_.begin();
    std::vector< DeviceConfigSet >::iterator coit = src_config_.begin();
    while (nameit != src_name_.end()){

        if ( (*nameit).compare(device) == 0 )
        {
            src_name_.erase(nameit);
            src_description_.erase(descit);
            src_config_.erase(coit);
            break;
        }

        ++nameit;
        ++descit;
        ++coit;
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
        src_name_.push_back(name);
        g_free (name);

#ifdef GST_DEVICE_DEBUG
       gchar *stru = gst_structure_to_string( gst_device_get_properties(device) );
       g_print("\nDevice %s already plugged : %s", name, stru);
       g_free (stru);
#endif

        std::string p = pipelineForDevice(device, src_description_.size());
        src_description_.push_back(p);

        DeviceConfigSet confs = getDeviceConfigs(p);
        src_config_.push_back(confs);
    }
    g_list_free(devices);

    // Add config for plugged screen
    src_name_.push_back("Screen capture");
    src_description_.push_back(gst_plugin_vidcap);

    // Try to auto find resolution
    DeviceConfigSet confs = getDeviceConfigs(gst_plugin_vidcap);
    if (!confs.empty()) {
        // fix the framerate (otherwise at 1 FPS
        DeviceConfig best = *confs.rbegin();
        DeviceConfigSet confscreen;
        best.fps_numerator = 15;
        confscreen.insert(best);
        src_config_.push_back(confscreen);
    }

    // TODO Use lib glfw to get monitors
    // TODO Detect auto removal of monitors

    list_uptodate_ = true;
}


int Device::numDevices() const
{
    return src_name_.size();
}

bool Device::exists(const std::string &device) const
{
    std::vector< std::string >::const_iterator d = std::find(src_name_.begin(), src_name_.end(), device);
    return d != src_name_.end();
}

struct hasDevice: public std::unary_function<DeviceSource*, bool>
{
    inline bool operator()(const DeviceSource* elem) const {
       return (elem && elem->device() == _d);
    }
    explicit hasDevice(const std::string &d) : _d(d) { }
private:
    std::string _d;
};

Source *Device::createSource(const std::string &device) const
{
    Source *s = nullptr;

    // find if a DeviceSource with this device is already registered
    std::list< DeviceSource *>::const_iterator d = std::find_if(device_sources_.begin(), device_sources_.end(), hasDevice(device));

    // if already registered, clone the device source
    if ( d != device_sources_.end())  {
        CloneSource *cs = (*d)->clone();
        s = cs;
    }
    // otherwise, we are free to create a new device source
    else {
        DeviceSource *ds = new DeviceSource();
        ds->setDevice(device);
        s = ds;
    }

    return s;
}

bool Device::unplugged(const std::string &device) const
{
    if (list_uptodate_)
        return false;
    return !exists(device);
}

std::string Device::name(int index) const
{
    if (index > -1 && index < (int) src_name_.size())
        return src_name_[index];
    else
        return "";
}

std::string Device::description(int index) const
{
    if (index > -1 && index < (int) src_description_.size())
        return src_description_[index];
    else
        return "";
}

DeviceConfigSet Device::config(int index) const
{
    if (index > -1 && index < (int) src_config_.size())
        return src_config_[index];
    else
        return DeviceConfigSet();
}

int  Device::index(const std::string &device) const
{
    int i = -1;
    std::vector< std::string >::const_iterator p = std::find(src_name_.begin(), src_name_.end(), device);
    if (p != src_name_.end())
        i = std::distance(src_name_.begin(), p);

    return i;
}

DeviceSource::DeviceSource(uint64_t id) : StreamSource(id)
{
    // create stream
    stream_ = new Stream;

    // set symbol
    symbol_ = new Symbol(Symbol::CAMERA, glm::vec3(0.75f, 0.75f, 0.01f));
    symbol_->scale_.y = 1.5f;
}

DeviceSource::~DeviceSource()
{
    // unregister this device source
    Device::manager().device_sources_.remove(this);
}

void DeviceSource::setDevice(const std::string &devicename)
{
    device_ = devicename;
    Log::Notify("Creating Source with device '%s'", device_.c_str());

    int index = Device::manager().index(device_);
    if (index > -1) {

        // register this device source
        Device::manager().device_sources_.push_back(this);

        // start filling in the gstreamer pipeline
        std::ostringstream pipeline;
        pipeline << Device::manager().description(index);

        // test the device and get config
        DeviceConfigSet confs = Device::manager().config(index);        
#ifdef DEVICE_DEBUG
        Log::Info("Device %s supported configs:", devicename.c_str());
        for( DeviceConfigSet::iterator it = confs.begin(); it != confs.end(); ++it ){
            float fps = static_cast<float>((*it).fps_numerator) / static_cast<float>((*it).fps_denominator);
            Log::Info(" - %s %s %d x %d  %.1f fps", (*it).stream.c_str(), (*it).format.c_str(), (*it).width, (*it).height, fps);
        }
#endif
        DeviceConfig best = *confs.rbegin();
        float fps = static_cast<float>(best.fps_numerator) / static_cast<float>(best.fps_denominator);
        Log::Info("Device %s selected its optimal config: %s %s %dx%d@%.1ffps", device_.c_str(), best.stream.c_str(), best.format.c_str(), best.width, best.height, fps);

        pipeline << " ! " << best.stream;
        if (!best.format.empty())
            pipeline << ",format=" << best.format;
        pipeline << ",framerate=" << best.fps_numerator << "/" << best.fps_denominator;
        pipeline << ",width=" << best.width;
        pipeline << ",height=" << best.height;

        if ( best.stream.find("jpeg") != std::string::npos )
            pipeline << " ! jpegdec";

        if ( device_.find("Screen") != std::string::npos )
            pipeline << " ! videoconvert ! video/x-raw,format=RGB ! queue max-size-buffers=3";

        pipeline << " ! videoconvert";

        // resize render buffer
        if (renderbuffer_)
            renderbuffer_->resize(best.width, best.height);

        // open gstreamer
        stream_->open( pipeline.str(), best.width, best.height);
        stream_->play(true);

        // will be ready after init and one frame rendered
        ready_ = false;
    }
    else
        Log::Warning("No such device '%s'", device_.c_str());
}

void DeviceSource::accept(Visitor& v)
{
    Source::accept(v);
    if (!failed())
        v.visit(*this);
}

bool DeviceSource::failed() const
{
    return stream_->failed() || Device::manager().unplugged(device_);
}

DeviceConfigSet Device::getDeviceConfigs(const std::string &src_description)
{
    DeviceConfigSet configs;

    // create dummy pipeline to be tested
    std::string description = src_description;
    description += " name=devsrc ! fakesink name=sink";

    // parse pipeline descriptor
    GError *error = NULL;
    GstElement *pipeline_ = gst_parse_launch (description.c_str(), &error);
    if (error != NULL) {
        Log::Warning("DeviceSource Could not construct test pipeline %s:\n%s", description.c_str(), error->message);
        g_clear_error (&error);
        return configs;
    }

    // get the pipeline element named "devsrc" from the Device class
    GstElement *elem = gst_bin_get_by_name (GST_BIN (pipeline_), "devsrc");
    if (elem) {

        // initialize the pipeline
        GstStateChangeReturn ret = gst_element_set_state (pipeline_, GST_STATE_PAUSED);
        if (ret != GST_STATE_CHANGE_FAILURE) {

            // get the first pad and its content
            GstIterator *iter = gst_element_iterate_src_pads(elem);
            GValue vPad = G_VALUE_INIT;
            GstPad* pad_ret = NULL;
            if (gst_iterator_next(iter, &vPad) == GST_ITERATOR_OK)
            {
                pad_ret = GST_PAD(g_value_get_object(&vPad));
                GstCaps *device_caps = gst_pad_query_caps (pad_ret, NULL);

                // loop over all caps offered by the pad
                int C = gst_caps_get_size(device_caps);
                for (int c = 0; c < C; ++c) {
                    // get GST cap
                    GstStructure *decice_cap_struct = gst_caps_get_structure (device_caps, c);
#ifdef GST_DEVICE_DEBUG
                    gchar *capstext = gst_structure_to_string (decice_cap_struct);
                    g_print("\nDevice caps: %s", capstext);
                    g_free(capstext);
#endif

                    // fill our config
                    DeviceConfig config;

                    // not managing opengl texture-target types
                    // TODO: support input devices texture-target video/x-raw(memory:GLMemory) for improved pipeline
                    if ( gst_structure_has_field (decice_cap_struct, "texture-target"))
                        continue;

                    // NAME : typically video/x-raw or image/jpeg
                    config.stream = gst_structure_get_name (decice_cap_struct);

                    // FORMAT : typically BGRA or YUVY
                    if ( gst_structure_has_field (decice_cap_struct, "format")) {
                        // get generic value
                        const GValue *val = gst_structure_get_value(decice_cap_struct, "format");

                        // if its a list of format string
                        if ( GST_VALUE_HOLDS_LIST(val)) {
                            int N = gst_value_list_get_size(val);
                            for (int n = 0; n < N; n++ ){
                                std::string f = gst_value_serialize( gst_value_list_get_value(val, n) );

                                // preference order : 1) RGBx, 2) JPEG, 3) ALL OTHER
                                // select f if it contains R (e.g. for RGBx) and not already RGB in config
                                if ( (f.find("R") != std::string::npos) && (config.format.find("R") == std::string::npos ) ) {
                                    config.format = f;
                                    break;
                                }
                                // default, take at least one if nothing yet in config
                                else if ( config.format.empty() )
                                    config.format = f;
                            }

                        }
                        // single format
                        else {
                            config.format = gst_value_serialize(val);
                        }
                    }

                    // FRAMERATE : can be a fraction of a list of fractions
                    if ( gst_structure_has_field (decice_cap_struct, "framerate")) {

                        // get generic value
                        const GValue *val = gst_structure_get_value(decice_cap_struct, "framerate");
                        // if its a single fraction
                        if ( GST_VALUE_HOLDS_FRACTION(val)) {
                            config.fps_numerator = gst_value_get_fraction_numerator(val);
                            config.fps_denominator= gst_value_get_fraction_denominator(val);
                        }
                        // if its a range of fraction; take the max
                        else if ( GST_VALUE_HOLDS_FRACTION_RANGE(val)) {
                            config.fps_numerator = gst_value_get_fraction_numerator(gst_value_get_fraction_range_max(val));
                            config.fps_denominator= gst_value_get_fraction_denominator(gst_value_get_fraction_range_max(val));
                        }
                        // deal otherwise with a list of fractions; find the max
                        else if ( GST_VALUE_HOLDS_LIST(val)) {
                            gdouble fps_max = 1.0;
                            // loop over all fractions
                            int N = gst_value_list_get_size(val);
                            for (int i = 0; i < N; ++i ){
                                const GValue *frac = gst_value_list_get_value(val, i);
                                // read one fraction in the list
                                if ( GST_VALUE_HOLDS_FRACTION(frac)) {
                                    int n = gst_value_get_fraction_numerator(frac);
                                    int d = gst_value_get_fraction_denominator(frac);
                                    // keep only the higher FPS
                                    gdouble f = 1.0;
                                    gst_util_fraction_to_double( n, d, &f );
                                    if ( f > fps_max ) {
                                        config.fps_numerator = n;
                                        config.fps_denominator = d;
                                        fps_max = f;
                                    }
                                }
                            }
                        }
                    }

                    // WIDTH and HEIGHT
                    if ( gst_structure_has_field (decice_cap_struct, "width"))
                        gst_structure_get_int (decice_cap_struct, "width", &config.width);
                    if ( gst_structure_has_field (decice_cap_struct, "height"))
                        gst_structure_get_int (decice_cap_struct, "height", &config.height);


                    // add this config
                    configs.insert(config);
                }

            }
            gst_iterator_free(iter);

            // terminate pipeline
            gst_element_set_state (pipeline_, GST_STATE_NULL);
        }

        g_object_unref (elem);
    }

    gst_object_unref (pipeline_);

    return configs;
}


glm::ivec2 DeviceSource::icon() const
{
    if ( device_.find("Screen") != std::string::npos )
        return glm::ivec2(19, 1);
    else
        return glm::ivec2(2, 14);
}










