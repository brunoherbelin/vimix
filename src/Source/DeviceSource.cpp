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
#include "Scene/Decorations.h"
#include "Stream.h"
#include "Visitor/Visitor.h"

#include "DeviceSource.h"

#ifndef NDEBUG
#define DEVICE_DEBUG
#endif



#if defined(APPLE)
std::string gst_plugin_device = "avfvideosrc";
#else
std::string gst_plugin_device = "v4l2src";
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
        gst_structure_free(stru);
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

std::string get_device_properties(GstDevice *device)
{
    std::ostringstream oss;

    GstStructure *stru = gst_device_get_properties(device);
    gint n =  gst_structure_n_fields (stru);
    oss << "- " << gst_structure_get_name(stru) << " -" << std::endl;
    for (gint i = 0; i < n; ++i) {
        std::string name =  gst_structure_nth_field_name (stru, i);
        if (name.find("device.path") !=std::string::npos || name.find("object.path") !=std::string::npos)
            oss << "Path : " << gst_structure_get_string(stru, name.c_str()) << std::endl;
        if (name.find("device.api") !=std::string::npos)
            oss << "Api : " << gst_structure_get_string(stru, name.c_str()) << std::endl;
        if (name.find("device.card") !=std::string::npos || name.find("cap.card") !=std::string::npos)
            oss << "Card : " << gst_structure_get_string(stru, name.c_str()) << std::endl;
        if (name.find("device.driver") !=std::string::npos || name.find("cap.driver") !=std::string::npos)
            oss << "Driver : " << gst_structure_get_string(stru, name.c_str()) << std::endl;
    }
    gst_structure_free (stru);

    return oss.str();
}

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

        // add if not in the list and valid
        std::string p = pipelineForDevice(device, handles_.size());
        if (!p.empty()) {

            GstToolkit::PipelineConfigSet confs = GstToolkit::getPipelineConfigs(p);
            if (!confs.empty()) {
                DeviceHandle dev;
                dev.name = device_name;
                dev.pipeline = p;
                dev.configs = confs;
                dev.properties = get_device_properties (device);
#ifdef DEVICE_DEBUG
                GstStructure *stru = gst_device_get_properties(device);
                g_print("\n%s: %s\n", device_name, gst_structure_to_string(stru) );
                gst_structure_free(stru);
#endif
                handles_.push_back(dev);
                Log::Info("Device '%s' is plugged-in.", device_name);
            }
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

    std::string devicename = gst_device_get_display_name (device);

    // lock before change
    access_.lock();

    // if a device with this name is listed
    auto handle = std::find_if(handles_.cbegin(), handles_.cend(), hasDeviceName(devicename) );
    if ( handle != handles_.cend() ) {

        // just inform the user if there is no source connected
        if (handle->connected_sources.empty()) {
            Log::Info("Device '%s' unplugged.", devicename.c_str());
        }
        else {
            // otherwise unplug all sources and close their streams
            bool first = true;
            for (auto sit = handle->connected_sources.begin(); sit != handle->connected_sources.end(); ++sit) {
                // for the first connected source in the list
                if (first) {
                    // close the stream
                    (*sit)->stream()->close();
                    first = false;
                } else {
                    // all others set the stream to null, so it is not deleted in destructor of StreamSource
                    (*sit)->stream_ = nullptr;
                }
                // all connected sources are set to unplugged
                (*sit)->unplug();
                // and finally inform user
                Log::Warning("Device '%s' unplugged: sources %s disconnected.",
                             devicename.c_str(), (*sit)->name().c_str());
            }
        }

        // remove the handle
        handles_.erase(handle);
    }

    // unlock access
    access_.unlock();
}

Device::Device(): monitor_(nullptr), monitor_initialized_(false)
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

    // monitor is initialized
    d->monitor_initialized_.store(true);
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
    return Device::manager().monitor_initialized_.load();
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
    if (!initialized())
        return 0;
    
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

std::string Device::properties(int index)
{
    std::string ret = "";

    access_.lock();
    if (index > -1 && index < (int) handles_.size())
        ret = handles_[index].properties;
    access_.unlock();

    return ret;
}

GstToolkit::PipelineConfigSet Device::config(int index)
{
    GstToolkit::PipelineConfigSet ret;

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
    // lock before accessing handles_
    Device::manager().access_.lock();

    // unregister this device source from a Device handler
    auto h = std::find_if(Device::manager().handles_.begin(), Device::manager().handles_.end(), hasConnectedSource(this));
    if (h != Device::manager().handles_.end())
    {
        // remove this pointer to the list of connected sources
        h->connected_sources.remove(this);
        // if this is the last source connected to the device handler
        // the stream will be removed by the ~StreamSource destructor
        // and the device handler should not keep reference to it
        if (h->connected_sources.empty())
            // cancel the reference to the stream
            h->stream = nullptr;
        else
        // else this means another DeviceSource is using this stream
        // and we should avoid to delete the stream in the ~StreamSource destructor
            stream_ = nullptr;
    }

    // unlock before changing device name
    Device::manager().access_.unlock();

    device_ = "";
}

void DeviceSource::reconnect()
{
    // remember name
    std::string d = device_;
    // disconnect
    unsetDevice();
    // connect
    setDevice(d);
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

    // if the stream referenced in this source remains after unsetDevice
    if (stream_) {
        delete stream_;
        stream_ = nullptr;
    }

    // set new device name
    device_ = devicename;

    // lock before accessing handles_
    Device::manager().access_.lock();

    // check existence of a device handle with that name
    auto h = std::find_if(Device::manager().handles_.begin(), Device::manager().handles_.end(), hasDeviceName(device_));

    // found a device handle
    if ( h != Device::manager().handles_.end()) {

        // find if a DeviceHandle with this device name already has a stream that is open
        if ( h->stream != nullptr ) {            
            // just use it !
            stream_ = h->stream;
            // reinit to adapt to new stream
            init();
        }
        else {
            // start filling in the gstreamer pipeline
            std::ostringstream pipeline;
            pipeline << h->pipeline;

            // test the device and get config
            GstToolkit::PipelineConfigSet confs = h->configs;
#ifdef DEVICE_DEBUG
            Log::Info("Device %s supported configs:", devicename.c_str());
            for( GstToolkit::PipelineConfigSet::iterator it = confs.begin(); it != confs.end(); ++it ){
                float fps = static_cast<float>((*it).fps_numerator) / static_cast<float>((*it).fps_denominator);
                Log::Info(" - %s %s %d x %d  %.1f fps", (*it).stream.c_str(), (*it).format.c_str(), (*it).width, (*it).height, fps);
            }
#endif
            if (!confs.empty()) {
                GstToolkit::PipelineConfig best = *confs.rbegin();
                float fps = static_cast<float>(best.fps_numerator) / static_cast<float>(best.fps_denominator);
                Log::Info("Device %s selected its optimal config: %s %s %dx%d@%.1ffps", device_.c_str(), best.stream.c_str(), best.format.c_str(), best.width, best.height, fps);

                pipeline << " ! " << best.stream;
                // if (!best.format.empty())
                //     pipeline << ",format=" << best.format; // disabled after problem with OSX avfvideosrc
                pipeline << ",framerate=" << best.fps_numerator << "/" << best.fps_denominator;
                pipeline << ",width=" << best.width;
                pipeline << ",height=" << best.height;

                // decode jpeg if needed
                if ( best.stream.find("jpeg") != std::string::npos )
                    pipeline << " ! jpegdec";

                // always convert
                pipeline << " ! queue ! videoconvert";

                // delete and reset render buffer to enforce re-init of StreamSource
                if (renderbuffer_)
                    delete renderbuffer_;
                renderbuffer_ = nullptr;

                // new stream
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
        Log::Warning("No device named '%s'", device_.c_str());
    }

    // unlock after accessing handles_
    Device::manager().access_.unlock();
}

void DeviceSource::setActive (bool on)
{
    bool was_active = active_;

    // try to activate (may fail if source is cloned)
    Source::setActive(on);

    if (stream_) {
        // change status of stream (only if status changed)
        if (active_ != was_active) {

            // lock before accessing handles_
            Device::manager().access_.lock();

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

            // unlock after accessing handles_
            Device::manager().access_.unlock();
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

glm::ivec2 DeviceSource::icon() const
{
    return glm::ivec2(ICON_SOURCE_DEVICE);
}

std::string DeviceSource::info() const
{
    return "Device";
}








