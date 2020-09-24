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
       pipe << "src name=devsrc device=";
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
        pipe << "src name=devsrc device=";
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

    std::ostringstream pipeline;
    pipeline << Device::manager().pipeline(device_);

    DeviceInfoSet confs = getDeviceConfigs(pipeline.str());
    DeviceInfoSet::reverse_iterator best = confs.rbegin();

    pipeline << " ! " << (*best).format;
    pipeline << ",framerate=" << (*best).fps_numerator << "/" << (*best).fps_denominator;
    pipeline << ",width=" << (*best).width;
    pipeline << ",height=" << (*best).height;

    if ( (*best).format.find("jpeg") != std::string::npos )
        pipeline << " ! jpegdec";

//    for( DeviceInfoSet::iterator it = confs.begin(); it != confs.end(); it++ ){
//        Log::Info("config possible : %s %dx%d @ %d fps", (*it).format.c_str(), (*it).width, (*it).height, (*it).fps_numerator);
//    }

    pipeline << " ! videoconvert";

    stream_->open( pipeline.str(), (*best).width, (*best).height);
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


DeviceInfoSet DeviceSource::getDeviceConfigs(const std::string &pipeline)
{
    DeviceInfoSet configs;

    // create dummy pipeline to be tested
    std::string description = pipeline;
    description += " ! fakesink name=sink";

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
            GstPad* ret = NULL;
            if (gst_iterator_next(iter, &vPad) == GST_ITERATOR_OK)
            {
                ret = GST_PAD(g_value_get_object(&vPad));
                GstCaps *device_caps = gst_pad_query_caps (ret, NULL);

                // loop over all caps offered by the pad
                int C = gst_caps_get_size(device_caps);
                for (int c = 0; c < C; ++c) {
                    GstStructure *decice_cap_struct = gst_caps_get_structure (device_caps, c);
                    DeviceInfo config;

                    // NAME : typically video/x-raw or image/jpeg
                    config.format = gst_structure_get_name (decice_cap_struct);

                    // FRAMERATE : can be a fraction of a list of fractions
                    if ( gst_structure_has_field (decice_cap_struct, "framerate")) {

                        // get generic value
                        const GValue *val = gst_structure_get_value(decice_cap_struct, "framerate");
                        // if its a single fraction
                        if ( GST_VALUE_HOLDS_FRACTION(val)) {
                            config.fps_numerator = gst_value_get_fraction_numerator(val);
                            config.fps_denominator= gst_value_get_fraction_denominator(val);
                        }
                        // deal otherwise with a list of fractions
                        else {
                            gdouble fps_max = 1.0;
                            // loop over all fractions
                            int N = gst_value_list_get_size(val);
                            for (int n = 0; n < N; n++ ){
                                const GValue *frac = gst_value_list_get_value(val, n);
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
                    if ( gst_structure_has_field (decice_cap_struct, "width"))
                        gst_structure_get_int (decice_cap_struct, "width", &config.width);
                    if ( gst_structure_has_field (decice_cap_struct, "height"))
                        gst_structure_get_int (decice_cap_struct, "height", &config.height);

//                    gchar *capstext = gst_structure_to_string (decice_cap_struct);
//                    Log::Info("DeviceSource found cap struct %s", capstext);
//                    g_free(capstext);
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












