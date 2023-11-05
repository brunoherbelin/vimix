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

#include <thread>
#include <algorithm>

#include <gst/pbutils/gstdiscoverer.h>
#include <gst/pbutils/pbutils.h>
#include <gst/gst.h>

#include "Settings.h"
#include "Log.h"

#include "Audio.h"

#ifndef NDEBUG
//#define AUDIO_DEBUG
#endif

Audio::Audio(): monitor_(nullptr), monitor_initialized_(false)
{
    std::thread(launchMonitoring, this).detach();
}

void Audio::launchMonitoring(Audio *d)
{
    // gstreamer monitoring of devices
    d->monitor_ = gst_device_monitor_new ();
    gst_device_monitor_set_show_all_devices(d->monitor_, true);

    // watching all video stream sources
    GstCaps *caps = gst_caps_new_empty_simple ("audio/x-raw");
    gst_device_monitor_add_filter (d->monitor_, "Audio/Source", caps);
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
    d->monitor_initialized_ = true;
    d->monitor_initialization_.notify_all();

    // create a local g_main_context to launch monitoring in this thread
    GMainContext *_gcontext_device = g_main_context_new();
    g_main_context_acquire(_gcontext_device);

    // temporarily push as default the default g_main_context for add_watch
    g_main_context_push_thread_default(_gcontext_device);

    // add a bus watch on the device monitoring using the main loop we created
    GstBus *bus = gst_device_monitor_get_bus (d->monitor_);
    gst_bus_add_watch (bus, callback_audio_monitor, NULL);
    gst_object_unref (bus);

    //    gst_device_monitor_set_show_all_devices(d->monitor_, false);
    if ( !gst_device_monitor_start (d->monitor_) )
        Log::Info("Audio discovery failed.");

    // restore g_main_context
    g_main_context_pop_thread_default(_gcontext_device);

    // start main loop for this context (blocking infinitely)
    g_main_loop_run( g_main_loop_new (_gcontext_device, true) );
}

void Audio::initialize()
{
    // instanciate and wait for monitor initialization if not already initialized
    std::mutex mtx;
    std::unique_lock<std::mutex> lck(mtx);
    Audio::manager().monitor_initialization_.wait(lck, Audio::_initialized);
}

bool Audio::_initialized()
{
    return Audio::manager().monitor_initialized_;
}

gboolean
Audio::callback_audio_monitor (GstBus *, GstMessage * message, gpointer )
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

int Audio::numDevices()
{
    access_.lock();
    int ret = handles_.size();
    access_.unlock();

    return ret;
}

struct hasAudioName
{
    inline bool operator()(const AudioHandle &elem) const {
        return (elem.name.compare(_name) == 0);
    }
    explicit hasAudioName(const std::string &name) : _name(name) { }
private:
    std::string _name;
};

bool Audio::exists(const std::string &device)
{
    access_.lock();
    auto h = std::find_if(handles_.cbegin(), handles_.cend(), hasAudioName(device));
    bool ret = (h != handles_.cend());
    access_.unlock();

    return ret;
}

int  Audio::index(const std::string &device)
{
    int i = -1;

    access_.lock();
    auto h = std::find_if(handles_.cbegin(), handles_.cend(), hasAudioName(device));
    if (h != handles_.cend())
        i = std::distance(handles_.cbegin(), h);
    access_.unlock();

    return i;
}

std::string Audio::name(int index)
{
    std::string ret = "";

    access_.lock();
    if (index > -1 && index < (int) handles_.size())
        ret = handles_[index].name;
    access_.unlock();

    return ret;
}

bool Audio::is_monitor(int index)
{
    bool ret = false;

    access_.lock();
    if (index > -1 && index < (int) handles_.size())
        ret = handles_[index].is_monitor;
    access_.unlock();

    return ret;
}

std::string Audio::pipeline(int index)
{
    std::string ret = "";

    access_.lock();
    if (index > -1 && index < (int) handles_.size())
        ret = handles_[index].pipeline;
    access_.unlock();

    return ret;
}


// copied from gst-inspect-1.0. perfect for identifying devices
// https://github.com/freedesktop/gstreamer-gst-plugins-base/blob/master/tools/gst-device-monitor.c
static gchar *get_launch_line(::GstDevice *device)
{
    static const char *const ignored_propnames[] = { "name", "parent", "direction", "template", "caps", nullptr };
    GString *                launch_line;
    GstElement *             element;
    GstElement *             pureelement;
    GParamSpec **            properties, *property;
    GValue                   value  = G_VALUE_INIT;
    GValue                   pvalue = G_VALUE_INIT;
    guint                    i, number_of_properties;
    GstElementFactory *      factory;

    element = gst_device_create_element(device, nullptr);

    if (!element)
        return nullptr;

    factory = gst_element_get_factory(element);
    if (!factory) {
        gst_object_unref(element);
        return nullptr;
    }

    if (!gst_plugin_feature_get_name(factory)) {
        gst_object_unref(element);
        return nullptr;
    }

    launch_line = g_string_new(gst_plugin_feature_get_name(factory));

    pureelement = gst_element_factory_create(factory, nullptr);

    /* get paramspecs and show non-default properties */
    properties = g_object_class_list_properties(G_OBJECT_GET_CLASS(element), &number_of_properties);
    if (properties) {
        for (i = 0; i < number_of_properties; i++) {
            gint     j;
            gboolean ignore = FALSE;
            property        = properties[i];

            /* skip some properties */
            if ((property->flags & G_PARAM_READWRITE) != G_PARAM_READWRITE)
                continue;

            for (j = 0; ignored_propnames[j]; j++)
                if (!g_strcmp0(ignored_propnames[j], property->name))
                    ignore = TRUE;

            if (ignore)
                continue;

            /* Can't use _param_value_defaults () because sub-classes modify the
             * values already.
             */

            g_value_init(&value, property->value_type);
            g_value_init(&pvalue, property->value_type);
            g_object_get_property(G_OBJECT(element), property->name, &value);
            g_object_get_property(G_OBJECT(pureelement), property->name, &pvalue);
            if (gst_value_compare(&value, &pvalue) != GST_VALUE_EQUAL) {
                gchar *valuestr = gst_value_serialize(&value);

                if (!valuestr) {
                    GST_WARNING("Could not serialize property %s:%s", GST_OBJECT_NAME(element), property->name);
                    g_free(valuestr);
                    goto next;
                }

                g_string_append_printf(launch_line, " %s=%s", property->name, valuestr);
                g_free(valuestr);
            }

        next:
            g_value_unset(&value);
            g_value_unset(&pvalue);
        }
        g_free(properties);
    }

    gst_object_unref(element);
    gst_object_unref(pureelement);

    return g_string_free(launch_line, FALSE);
}


void Audio::add(GstDevice *device)
{
    if (device==nullptr)
        return;

    gchar *device_name = gst_device_get_display_name (device);

    // lock before change
    access_.lock();

    // if device with this name is not already listed
    auto handle = std::find_if(handles_.cbegin(), handles_.cend(), hasAudioName(device_name) );
    if ( handle == handles_.cend() ) {

        gchar *ll = get_launch_line(device);

        // add if valid pipeline
        if (ll)
        {
            AudioHandle dev;
            dev.name = device_name;
            dev.pipeline = std::string(ll);
            dev.is_monitor = dev.pipeline.compare( dev.pipeline.size()-7, 7, "monitor") == 0;

#ifdef AUDIO_DEBUG
            GstStructure *stru = gst_device_get_properties(device);
            g_print("\nName: '%s'\nProperties: %s", device_name, gst_structure_to_string(stru) );
            g_print("\nPipeline: %s\n", dev.pipeline.c_str());
#endif
            handles_.push_back(dev);
            Log::Info("Audio device '%s' is plugged-in.", device_name);
        }

    }

    // unlock access
    access_.unlock();

    g_free (device_name);
}

void Audio::remove(GstDevice *device)
{
    if (device==nullptr)
        return;

    std::string devicename = gst_device_get_display_name (device);

    // lock before change
    access_.lock();

    // if a device with this name is listed
    auto handle = std::find_if(handles_.cbegin(), handles_.cend(), hasAudioName(devicename) );
    if ( handle != handles_.cend() ) {

        // inform the user
        Log::Info("Audio device '%s' unplugged.", devicename.c_str());

        // Warning if the audio device used for recording is unplugged
        if (Settings::application.record.audio_device.compare(devicename) == 0) {
            Log::Warning("Audio device for recording was unplugged.");
            // reset settings
            Settings::application.record.audio_device = "";
        }

        // remove the handle
        handles_.erase(handle);
    }

    // unlock access
    access_.unlock();
}
