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
//#define GST_DEVICE_DEBUG

#include <sstream>
#include <iomanip>
using namespace std;

#include <gst/gl/gl.h>

#include "GstToolkit.h"

string GstToolkit::time_to_string(guint64 t, time_string_mode m)
{
    if (t == GST_CLOCK_TIME_NONE) {
        switch (m) {
        case TIME_STRING_FIXED:
            return "00:00:00.00";
        case TIME_STRING_MINIMAL:
            return "0.0";
        case TIME_STRING_READABLE:
            return "0 second";
        default:
            return "00.00";
        }
    }

    guint ms =  GST_TIME_AS_MSECONDS(t);
    guint s = ms / 1000;
    ostringstream oss;

    // READABLE : long format
    if (m == TIME_STRING_READABLE) {
        int count = 0;
        if (s / 3600) {
            oss << s / 3600 << " h ";
            count++;
        }
        if ((s % 3600) / 60) {
            oss << (s % 3600) / 60 << " min ";
            count++;
        }
        if (count < 2) {
            oss << setw(count > 0 ? 2 : 1) << setfill('0') << (s % 3600) % 60;
            count++;

            if (count < 2 )
                oss << '.'<< setw(1) << setfill('0') << (ms % 1000) / 100 << " sec";
            else
                oss << " s";
        }
    }
    // MINIMAL: keep only the 2 higher values (most significant)
    else if (m == TIME_STRING_MINIMAL) {
        int count = 0;
        // hours
        if (s / 3600) {
            oss << s / 3600 << ':';
            count++;
        }
        // minutes
        if (count > 0) {
            oss << setw(2) << setfill('0') << (s % 3600) / 60 << ':';
            count++;
        }
        else if ((s % 3600) / 60)
        {
            oss << (s % 3600) / 60 << ':';
            count++;
        }
        // seconds
        {
            oss << setw(count > 0 ? 2 : 1) << setfill('0') << (s % 3600) % 60;
            count++;
        }
        if (count < 2)
            oss << '.'<< setw((ms % 1000) / 100 ? 2 : 1) << setfill('0') << (ms % 1000) / 10;
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


std::string GstToolkit::filename_to_uri(std::string path)
{
    if (path.empty())
        return path;

    // set uri to open
    gchar *uritmp = gst_filename_to_uri(path.c_str(), NULL);
    std::string uri( uritmp );
    g_free(uritmp);
    return uri;
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


list<string> GstToolkit::all_plugin_features(const std::string &pluginname)
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

bool GstToolkit::enable_feature (const std::string &name, bool enable)
{
    if (name.empty())
        return false;

    static GstRegistry *registry = NULL;
    if (!registry)
        registry = gst_registry_get();

    GstElementFactory *factory = NULL;
    factory = gst_element_factory_find (name.c_str());
    if (!factory) return false;

    if (enable) {
        gst_plugin_feature_set_rank (GST_PLUGIN_FEATURE (factory), GST_RANK_PRIMARY + 1);
    }
    else {
        gst_plugin_feature_set_rank (GST_PLUGIN_FEATURE (factory), GST_RANK_NONE);
    }

    gst_registry_add_feature (registry, GST_PLUGIN_FEATURE (factory));
    gst_object_unref (factory);

    return true;
}

bool GstToolkit::has_feature (const string &name)
{
    if (name.empty())
        return false;

    static GstRegistry *registry = NULL;
    if (!registry)
        registry = gst_registry_get();

    GstElementFactory *factory = NULL;
    factory = gst_element_factory_find (name.c_str());
    if (!factory) return false;

    GstElement *elem = gst_element_factory_create (factory, NULL);
    gst_object_unref (factory);

    if (!elem) return false;

    gst_object_unref (elem);
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

#if GST_GL_HAVE_PLATFORM_GLX
    // https://gstreamer.freedesktop.org/documentation/nvcodec/index.html?gi-language=c#plugin-nvcodec
    // list ordered with higher priority at the end (e.g. nvidia proprietary before vaapi)
const char *plugins[17] = { "vdpaumpegdec", "omxh264dec", "omxmpeg2dec", "omxmpeg4videodec", "vaapidecodebin",
                                "nvh264sldec", "nvh264dec", "nvh265sldec", "nvh265dec", "nvmpegvideodec", "nvmpeg2videodec", "nvmpeg4videodec",
                                "nvvp8sldec", "nvvp8dec", "nvvp9sldec", "nvvp9dec", "nvav1dec"
                               };
    const int N = 17;
#elif GST_GL_HAVE_PLATFORM_CGL
    const char *plugins[2] = { "vtdec_hw", "vtdechw" };
    const int N = 2;
#else
    const char *plugins[0] = { };
    const int N = 0;
#endif


// see https://developer.ridgerun.com/wiki/index.php?title=GStreamer_modify_the_elements_rank
std::list<std::string> GstToolkit::enable_gpu_decoding_plugins(bool enable)
{
    list<string> plugins_list_;

    static GstRegistry* plugins_register = nullptr;
    if ( plugins_register == nullptr )
        plugins_register = gst_registry_get();

    int n = 0;
    for (int i = 0; i < N; i++) {
        GstPluginFeature* feature = gst_registry_lookup_feature(plugins_register, plugins[i]);
        if(feature != NULL) {
            ++n;
            plugins_list_.push_front( string( plugins[i] ) );
            gst_plugin_feature_set_rank(feature, enable ? GST_RANK_PRIMARY + n : GST_RANK_MARGINAL + n);
//            g_printerr("Gstreamer plugin %s set to %d \n", plugins[i], enable ? GST_RANK_PRIMARY + n : GST_RANK_MARGINAL + n);
            gst_object_unref(feature);
        }
    }

    return plugins_list_;
}


std::string GstToolkit::used_gpu_decoding_plugins(GstElement *gstbin)
{
    std::string found = "";

    GstIterator* it  = gst_bin_iterate_recurse(GST_BIN(gstbin));
    GValue value = G_VALUE_INIT;
    for(GstIteratorResult r = gst_iterator_next(it, &value); r != GST_ITERATOR_DONE; r = gst_iterator_next(it, &value))
    {
        if ( r == GST_ITERATOR_OK )
        {
            GstElement *e = static_cast<GstElement*>(g_value_peek_pointer(&value));
            if (e) {
                gchar *name = gst_element_get_name(e);
                for (int i = 0; i < N; i++) {
                    if (std::string(name).find(plugins[i]) != std::string::npos) {
                        found = plugins[i];
                        break;
                    }
                }
                g_free(name);
            }
        }
        g_value_unset(&value);
    }
    gst_iterator_free(it);

    return found;
}



std::string GstToolkit::used_decoding_plugins(GstElement *gstbin)
{
    std::string found = "";

    GstIterator* it  = gst_bin_iterate_recurse(GST_BIN(gstbin));
    GValue value = G_VALUE_INIT;
    for(GstIteratorResult r = gst_iterator_next(it, &value); r != GST_ITERATOR_DONE; r = gst_iterator_next(it, &value))
    {
        if ( r == GST_ITERATOR_OK )
        {
            GstElement *e = static_cast<GstElement*>(g_value_peek_pointer(&value));
            if (e) {
                const gchar *name = gst_element_get_name(e);
                found += std::string(name) + ", ";
            }
        }
        g_value_unset(&value);
    }
    gst_iterator_free(it);

    return found;
}


GstToolkit::PipelineConfigSet GstToolkit::getPipelineConfigs(const std::string &src_description)
{
    PipelineConfigSet configs;

    // create dummy pipeline to be tested
    std::string description = src_description;
    description += " name=devsrc ! fakesink name=sink";

    // parse pipeline descriptor
    GError *error = NULL;
    GstElement *pipeline_ = gst_parse_launch (description.c_str(), &error);
    if (error != NULL) {
        g_printerr("DeviceSource Could not construct test pipeline %s:\n%s", description.c_str(), error->message);
        g_clear_error (&error);
        return configs;
    }

    // get the pipeline element named "devsrc" from the Device class
    GstElement *elem = gst_bin_get_by_name (GST_BIN (pipeline_), "devsrc");
    if (elem) {

        // initialize the pipeline
        GstStateChangeReturn ret = gst_element_set_state (pipeline_, GST_STATE_READY);
        if (ret != GST_STATE_CHANGE_FAILURE) {

            // get the first pad and its content
            GstIterator *iter = gst_element_iterate_src_pads(elem);
            if (iter != nullptr) 
            {
                GValue vPad = G_VALUE_INIT;
                if (gst_iterator_next(iter, &vPad) == GST_ITERATOR_OK)
                {
                    GstPad* pad_ret = NULL;
                    pad_ret = GST_PAD(g_value_get_object(&vPad));
                    GstCaps *device_caps = gst_pad_query_caps (pad_ret, NULL);

                    // loop over all caps offered by the pad
                    int C = device_caps != nullptr ? gst_caps_get_size(device_caps) : 0;
                    for (int c = 0; c < C; ++c) {
                        // get GST cap
                        GstStructure *decice_cap_struct = gst_caps_get_structure (device_caps, c);
#ifdef GST_DEVICE_DEBUG
                        gchar *capstext = gst_structure_to_string (decice_cap_struct);
                        g_print("\nPipeline caps: %s", capstext);
                        g_free(capstext);
#endif
                        // fill our config
                        PipelineConfig config;

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

                        // add this config if valid
                        if (config.width > 0 && config.height > 0 && config.format.size() > 0)
                            configs.insert(config);
                    }

                }
                gst_iterator_free(iter);
            }
            // terminate pipeline
            gst_element_set_state (pipeline_, GST_STATE_NULL);
        }

        g_object_unref (elem);
    }

    gst_object_unref (pipeline_);

    return configs;
}
