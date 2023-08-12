/*
 * This file is part of vimix - video live mixer
 *
 * **Copyright** (C) 2019-2023 Bruno Herbelin <bruno.herbelin@gmail.com>
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


#include "Log.h"
#include "Decorations.h"
#include "Stream.h"
#include "Visitor.h"

#include "DeviceSource.h"

#ifndef NDEBUG
#define DEVICE_DEBUG
#define GST_DEVICE_DEBUG
#endif



#if defined(APPLE)
std::string gst_plugin_device = "avfvideosrc";
std::string gst_plugin_vidcap = "avfvideosrc capture-screen=true";
#else
std::string gst_plugin_device = "v4l2src";
std::string gst_plugin_vidcap = "ximagesrc";
#endif

std::string pipelineForDevice(GstDevice *device, uint index)
{
    std::ostringstream pipe;
    GstStructure *stru = gst_device_get_properties(device);

    if (stru) {
        const gchar *api = gst_structure_get_string(stru, "device.api");

        // check that the api is supported by vimix
        if (api && gst_plugin_device.find(api) != std::string::npos)
        {
            pipe << gst_plugin_device;

#if defined(APPLE)
            pipe << " device-index=" << index;
#else
            (void) index; // prevent compilatin warning unused argument

            // get the path: either "device.path" or "api.v4l2.path"
            // (GStreamer has different ways depending on version)
            const gchar *path;
            if ( gst_structure_has_field (stru, "device.path"))
                path = gst_structure_get_string(stru, "device.path");
            else
                path = gst_structure_get_string(stru, "api.v4l2.path");
            pipe << " device=" << path;
#endif
        }
    }

    return pipe.str();
}

gboolean
Device::callback_device_monitor (GstBus *, GstMessage * message, gpointer )
{
   GstDevice *device;

   switch (GST_MESSAGE_TYPE (message)) {
   case GST_MESSAGE_DEVICE_ADDED:
   {
       gst_message_parse_device_added (message, &device);
       manager().add(device);
       gst_object_unref (device);

   }
       break;
   case GST_MESSAGE_DEVICE_REMOVED:
   {
       gst_message_parse_device_removed (message, &device);
       manager().remove(device);
       gst_object_unref (device);
   }
       break;
   default:
       break;
   }

   return G_SOURCE_CONTINUE;
}

struct hasDeviceName
{
    inline bool operator()(const DeviceHandle &elem) const {
       return (elem.name.compare(_name) == 0);
    }
    explicit hasDeviceName(const std::string &name) : _name(name) { }
private:
    std::string _name;
};


struct hasConnectedSource
{
    inline bool operator()(const DeviceHandle &elem) const {
        auto sit = std::find(elem.connected_sources.begin(), elem.connected_sources.end(), s_);
        return sit != elem.connected_sources.end();
    }
    explicit hasConnectedSource(Source *s) : s_(s) { }
private:
    Source *s_;
};

void Device::add(GstDevice *device)
{
    if (device==nullptr)
        return;

    gchar *device_name = gst_device_get_display_name (device);

    // lock before change
    access_.lock();

    // if device with this name is not already listed
    auto handle = std::find_if(handles_.cbegin(), handles_.cend(), hasDeviceName(device_name) );
    if ( handle == handles_.cend() ) {

        std::string p = pipelineForDevice(device, handles_.size());
        DeviceConfigSet confs = getDeviceConfigs(p);

        // add if not in the list and valid
        if (!p.empty() && !confs.empty()) {
            DeviceHandle dev;
            dev.name = device_name;
            dev.pipeline = p;
            dev.configs = confs;
            handles_.push_back(dev);
#ifdef GST_DEVICE_DEBUG
            gchar *stru = gst_structure_to_string( gst_device_get_properties(device) );
            g_print("\nDevice %s plugged : %s\n", device_name, stru);
            g_free (stru);
#endif
        }

    }
    // unlock access
    access_.unlock();

    g_free (device_name);
}

void Device::remove(GstDevice *device)
{    
    if (device==nullptr)
        return;

    gchar *device_name = gst_device_get_display_name (device);

    // lock before change
    access_.lock();

    // if a device with this name is listed
    auto handle = std::find_if(handles_.cbegin(), handles_.cend(), hasDeviceName(device_name) );
    if ( handle != handles_.cend() ) {

        // remove the handle if there is no source connected
        if (handle->connected_sources.empty())
            handles_.erase(handle);
        else {
            // otherwise unplug all sources
            for (auto sit = handle->connected_sources.begin(); sit != handle->connected_sources.end(); ++sit)
                (*sit)->unplug();
            // and inform user
            Log::Warning("Device %s unplugged.", device_name);
            // NB; the handle will be removed at the destruction of the last DeviceSource
        }
    }

    // unlock access
    access_.unlock();


    g_free (device_name);
}

Device::Device(): monitor_(nullptr), monitor_initialized_(false), monitor_unplug_event_(false)
{
    std::thread(launchMonitoring, this).detach();
}

void Device::launchMonitoring(Device *d)
{
    // gstreamer monitoring of devices
    d->monitor_ = gst_device_monitor_new ();
    gst_device_monitor_set_show_all_devices(d->monitor_, true);

    // watching all video stream sources
    GstCaps *caps = gst_caps_new_empty_simple ("video/x-raw");
    gst_device_monitor_add_filter (d->monitor_, "Video/Source", caps);
    gst_caps_unref (caps);

    // Add configs for already plugged devices
    GList *devices = gst_device_monitor_get_devices(d->monitor_);
    GList *tmp;
    for (tmp = devices; tmp ; tmp = tmp->next ) {
        GstDevice *device = (GstDevice *) tmp->data;
        d->add(device);
        gst_object_unref (device);
    }
    g_list_free(devices);

    // Try to get captrure screen
    DeviceConfigSet confs = getDeviceConfigs(gst_plugin_vidcap);
    if (!confs.empty()) {
        DeviceConfig best = *confs.rbegin();
        DeviceConfigSet confscreen;
        // limit framerate to 30fps
        best.fps_numerator = MIN( best.fps_numerator, 30);
        best.fps_denominator = 1;
        confscreen.insert(best);
        // add to config list
        d->access_.lock();

        DeviceHandle dev;
        dev.name = "Screen capture";
        dev.pipeline = gst_plugin_vidcap;
        dev.configs = confscreen;
        d->handles_.push_back(dev);
        d->access_.unlock();
    }

    // monitor is initialized
    d->monitor_initialized_ = true;
    d->monitor_initialization_.notify_all();

    // create a local g_main_context to launch monitoring in this thread
    GMainContext *_gcontext_device = g_main_context_new();
    g_main_context_acquire(_gcontext_device);

    // temporarily push as default the default g_main_context for add_watch
    g_main_context_push_thread_default(_gcontext_device);

    // add a bus watch on the device monitoring using the main loop we created
    GstBus *bus = gst_device_monitor_get_bus (d->monitor_);
    gst_bus_add_watch (bus, callback_device_monitor, NULL);
    gst_object_unref (bus);

    //    gst_device_monitor_set_show_all_devices(d->monitor_, false);
    if ( !gst_device_monitor_start (d->monitor_) )
        Log::Info("Device discovery failed.");

    // restore g_main_context
    g_main_context_pop_thread_default(_gcontext_device);

    // start main loop for this context (blocking infinitely)
    g_main_loop_run( g_main_loop_new (_gcontext_device, true) );
}

bool Device::initialized()
{
    return Device::manager().monitor_initialized_;
}

void Device::reload()
{
    if (monitor_ != nullptr) {
        gst_device_monitor_stop(monitor_);
        if ( !gst_device_monitor_start (monitor_) )
            Log::Info("Device discovery start failed.");
    }
}

int Device::numDevices()
{
    access_.lock();
    int ret = handles_.size();
    access_.unlock();

    return ret;
}

bool Device::exists(const std::string &device)
{
    access_.lock();
    auto h = std::find_if(handles_.cbegin(), handles_.cend(), hasDeviceName(device));
    bool ret = (h != handles_.cend());
    access_.unlock();

    return ret;
}

struct hasDevice
{
    inline bool operator()(const DeviceSource* elem) const {
       return (elem && elem->device() == _d);
    }
    explicit hasDevice(const std::string &d) : _d(d) { }
private:
    std::string _d;
};

std::string Device::name(int index)
{
    std::string ret = "";

    access_.lock();
    if (index > -1 && index < (int) handles_.size())
        ret = handles_[index].name;
    access_.unlock();

    return ret;
}

std::string Device::description(int index)
{
    std::string ret = "";

    access_.lock();
    if (index > -1 && index < (int) handles_.size())
        ret = handles_[index].pipeline;
    access_.unlock();

    return ret;
}

DeviceConfigSet Device::config(int index)
{
    DeviceConfigSet ret;

    access_.lock();
    if (index > -1 && index < (int) handles_.size())
        ret = handles_[index].configs;
    access_.unlock();

    return ret;
}

int  Device::index(const std::string &device)
{
    int i = -1;

    access_.lock();
    auto h = std::find_if(handles_.cbegin(), handles_.cend(), hasDeviceName(device));
    if (h != handles_.cend())
        i = std::distance(handles_.cbegin(), h);
    access_.unlock();

    return i;
}

DeviceSource::DeviceSource(uint64_t id) : StreamSource(id), unplugged_(false)
{
    // set symbol
    symbol_ = new Symbol(Symbol::CAMERA, glm::vec3(0.75f, 0.75f, 0.01f));
    symbol_->scale_.y = 1.5f;
}

DeviceSource::~DeviceSource()
{
    unsetDevice();
}

void DeviceSource::unsetDevice()
{
    // unregister this device source from a Device handler
    auto h = std::find_if(Device::manager().handles_.begin(), Device::manager().handles_.end(), hasConnectedSource(this));
    if (h != Device::manager().handles_.end())
    {
        // remove this pointer to the list of connected sources
        h->connected_sources.remove(this);
        // if this is the last source connected to the device handler
        // the stream will be removed by the ~StreamSource destructor
        // and the device handler should not keep reference to it
        if (h->connected_sources.empty()) {
            // if the cause of deletion is the unplugging of the device
            if (unplugged_)
                // remove the handle entirely
                Device::manager().handles_.erase(h);
            else
                // otherwise just cancel the reference to the stream
                h->stream = nullptr;
        }
        // else this means another DeviceSource is using this stream
        // and we should avoid to delete the stream in the ~StreamSource destructor
        else
            stream_ = nullptr;
    }
}


void DeviceSource::setDevice(const std::string &devicename)
{
    if (device_.compare(devicename) == 0)
        return;

    // instanciate and wait for monitor initialization if not already initialized
    std::mutex mtx;
    std::unique_lock<std::mutex> lck(mtx);
    Device::manager().monitor_initialization_.wait(lck, Device::initialized);

    // if changing device
    if (!device_.empty())
        unsetDevice();

    // remember device name
    device_ = devicename;

    // check existence of a device handle with that name
    auto h = std::find_if(Device::manager().handles_.begin(), Device::manager().handles_.end(), hasDeviceName(device_));
    if ( h != Device::manager().handles_.end()) {

        // find if a DeviceHandle with this device name already has a stream
        if ( h->stream != nullptr) {
            // just use it !
            stream_ = h->stream;
        }
        else {
            // start filling in the gstreamer pipeline
            std::ostringstream pipeline;
            pipeline << h->pipeline;

            // test the device and get config
            DeviceConfigSet confs = h->configs;
#ifdef DEVICE_DEBUG
            Log::Info("Device %s supported configs:", devicename.c_str());
            for( DeviceConfigSet::iterator it = confs.begin(); it != confs.end(); ++it ){
                float fps = static_cast<float>((*it).fps_numerator) / static_cast<float>((*it).fps_denominator);
                Log::Info(" - %s %s %d x %d  %.1f fps", (*it).stream.c_str(), (*it).format.c_str(), (*it).width, (*it).height, fps);
            }
#endif
            if (!confs.empty()) {
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

                // delete and reset render buffer to enforce re-init of StreamSource
                if (renderbuffer_)
                    delete renderbuffer_;
                renderbuffer_ = nullptr;

                // new stream
                if (stream_)
                    delete stream_;
                stream_ = h->stream = new Stream;

                // open gstreamer
                h->stream->open( pipeline.str(), best.width, best.height);
                h->stream->play(true);
            }
        }

        // reference this source in the handle
        h->connected_sources.push_back(this);

        // will be ready after init and one frame rendered
        ready_ = false;
    }
    else {
        unplugged_ = true;
        Log::Warning("No such device '%s'", device_.c_str());
    }
}

void DeviceSource::setActive (bool on)
{
    bool was_active = active_;

    // try to activate (may fail if source is cloned)
    Source::setActive(on);

    if (stream_) {
        // change status of stream (only if status changed)
        if (active_ != was_active) {

            // activate a source if any of the handled device source is active
            auto h = std::find_if(Device::manager().handles_.begin(), Device::manager().handles_.end(), hasConnectedSource(this));
            if (h != Device::manager().handles_.end()) {
                bool streamactive = false;
                for (auto sit = h->connected_sources.begin(); sit != h->connected_sources.end(); ++sit) {
                    if ( (*sit)->active_)
                        streamactive = true;
                }
                stream_->enable(streamactive);
            }

        }

    }

}

void DeviceSource::accept(Visitor& v)
{
    StreamSource::accept(v);
    v.visit(*this);
}

Source::Failure DeviceSource::failed() const
{
    return (unplugged_ || StreamSource::failed()) ? FAIL_CRITICAL : FAIL_NONE;
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
                    else {
                        // default
                        config.fps_numerator = 30;
                        config.fps_denominator = 1;
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
        return glm::ivec2(ICON_SOURCE_DEVICE_SCREEN);
    else
        return glm::ivec2(ICON_SOURCE_DEVICE);
}

std::string DeviceSource::info() const
{
    if ( device_.find("Screen") != std::string::npos )
        return "Screen capture";
    else
        return "Device";
}










