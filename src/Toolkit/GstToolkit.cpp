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
#define GST_DEVICE_DEBUG

#include <sstream>
#include <iomanip>
#include <filesystem>
#include <vector>
#include <map>
#include <mutex>
#include <cmath>

using namespace std;
namespace fs = std::filesystem;

#include <gst/gl/gl.h>
#include <gst/video/video.h>

#include "GstToolkit.h"

bool GstToolkit::isRGBFormat(const std::string &format)
{
    if (format.empty())
        return false;
    const GstVideoFormat f = gst_video_format_from_string(format.c_str());
    if (f == GST_VIDEO_FORMAT_UNKNOWN)
        return false;
    const GstVideoFormatInfo *info = gst_video_format_get_info(f);
    return info != nullptr && GST_VIDEO_FORMAT_INFO_IS_RGB(info);
}

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


const char* GstToolkit::profile_name[GstToolkit::DEFAULT] = {
    "H264 (Real-time)",
    "H264 (Quality)",
    "H265 (Real-time)",
    "H265 (Quality)",
    "ProRes (Realtime)",
    "ProRes (Quality)",
    "VP9  (Realtime)",
    "Multiple JPEG"
};

namespace {

// There are two profiles of encoding: RT and HQ
// RT : "Real-time", Should encode live at 60 fps a 1080p video ; quality is at the highest considering this constraint.
// HQ : "High Quality", Should encode a 1080p video with visually lossless quality; encoding speed is less a constraint, but should still be live at 30fps
//
// For each profile, several encoders are available, depending on the platform and the hardware acceleration available.
// The user can select the encoder to use in the settings. Typically videos are encoded in H264 or H265
// Usual sofware encoders are x264 and x265. Fallback h264 encoder is openh264 if x264 is not available.
// If available, hardware accelerated encoders are used: NVENC (NVIDIA) or VAAPI (Intel/AMD) deoending on the platform and the GPU.
// On MacOS, VideoToolbox is used for encoding.

// Software encoder gst pipeline fragment for each profile, computed once
// (magic static) to reflect the codecs actually installed on this system.
const std::vector<std::string> &software_profile_description()
{
    static const std::vector<std::string> table = [] {
        std::vector<std::string> t {
            "x264enc pass=qual quantizer=20 speed-preset=veryfast bframes=2 key-int-max=30 bitrate=25000 ! video/x-h264, profile=(string)main ! h264parse ! ",
            "x264enc pass=qual quantizer=18 speed-preset=medium bframes=3 key-int-max=30 bitrate=50000 ! video/x-h264, profile=(string)high ! h264parse ! ",
            "x265enc speed-preset=superfast tune=0 key-int-max=30 option-string=\"crf=20:vbv-maxrate=25000:vbv-bufsize=25000\" ! video/x-h265, profile=(string)main ! h265parse ! ",
            "x265enc speed-preset=medium tune=0 key-int-max=30 option-string=\"crf=18:vbv-maxrate=50000:vbv-bufsize=50000\" ! video/x-h265, profile=(string)main-444 ! h265parse ! ",
            "avenc_prores_ks pass=quant quantizer=8 profile=standard quant-mat=default threads=0 vendor=apl0 ! ",
            "avenc_prores_ks pass=quant quantizer=4 profile=hq quant-mat=default threads=0 vendor=apl0 ! ",
            "vp9enc end-usage=cq cq-level=24 target-bitrate=25000000 deadline=1 cpu-used=6 lag-in-frames=0 keyframe-max-dist=30 threads=4 row-mt=true tile-columns=2 ! ",
            // JPEG encoding in YCbCr 4:2:0 as from any camera
            "videoconvert ! video/x-raw, format=I420, colorimetry=(string)1:4:0:0 ! "
            "jpegenc idct-method=float ! "
        };

        if (!GstToolkit::has_feature("x264enc")) {
            // fallback if x264 is not available
            if (GstToolkit::has_feature("openh264enc")) {
                t[GstToolkit::H264_RT] = "openh264enc rate-control=quality qp-min=27 qp-max=27 gop-size=30 complexity=low "
                    "slice-mode=auto multi-thread=0 enable-frame-skip=false ! video/x-h264 ! h264parse ! ";
                t[GstToolkit::H264_HQ] = "openh264enc rate-control=quality qp-min=25 qp-max=25 gop-size=30 complexity=medium "
                    "slice-mode=auto multi-thread=0 enable-frame-skip=false ! video/x-h264 ! h264parse ! ";
            }
            // disable h264 encoders if none available
            else {
                t[GstToolkit::H264_RT] = "";
                t[GstToolkit::H264_HQ] = "";
            }
        }
        // disable h265 encoders if not available
        if (!GstToolkit::has_feature("x265enc")) {
            t[GstToolkit::H265_RT] = "";
            t[GstToolkit::H265_HQ] = "";
        }

        return t;
    }();

    return table;
}

// Adapters from frames in OpenGL memory (RGBA, see GstToolkit::Memory) to an encoder,
// depending on the input the encoder accepts (NB: from system memory, no adapter is needed:
// the upstream videoconvert negotiates the format with the encoder).
// - the encoder takes GLMemory: nothing to do
const char *GL_TO_GL = "";
// - the encoder takes system memory: download (to RGBA) and convert (e.g. to I420)
const char *GL_TO_SYSTEM = "gldownload ! videoconvert ! ";
// - the encoder takes NV12 in system memory only (nvcudah264enc before GStreamer 1.26):
//   convert to NV12 on the GPU (3 MB instead of 8 MB of RGBA per 1080p frame), then download
const char *GL_TO_SYSTEM_NV12 = "glcolorconvert ! video/x-raw(memory:GLMemory),format=NV12 ! gldownload ! ";
// - the encoder takes VAMemory (VA-API): gldownload exports a DMABuf when the GL platform
//   supports it (Mesa), and falls back to system memory otherwise; vapostproc puts it into a
//   VA surface and performs the RGBA to NV12 conversion on the VA hardware (faster than in GL).
const char *GL_TO_VA = "gldownload ! vapostproc ! video/x-raw(memory:VAMemory) ! ";

// Test (once) if all the elements of a gst pipeline fragment are available
// (e.g. "gldownload ! vapostproc ! video/x-raw(memory:VAMemory) ! " needs
// gldownload and vapostproc; caps are ignored).
bool fragment_available(const std::string &fragment)
{
    static std::map<std::string, bool> cache;
    static std::mutex cache_access;

    const std::lock_guard<std::mutex> lock(cache_access);

    auto known = cache.find(fragment);
    if (known != cache.end())
        return known->second;

    bool available = true;
    std::istringstream elements(fragment);
    std::string element;
    while (available && std::getline(elements, element, '!')) {
        // first word of each element of the fragment
        std::istringstream words(element);
        std::string name;
        if (words >> name && name.find('/') == std::string::npos)
            available = GstToolkit::has_feature(name);
    }

    cache[fragment] = available;
    return available;
}

// gst element feature name to test with has_feature(), corresponding pipeline
// fragment, and adapter from frames in OpenGL memory, per profile, for the
// hardware encoder available on this platform (empty if none).
struct HardwareEncoderTable {
    std::vector<std::string> feature;
    std::vector<std::string> pipeline;
    std::vector<std::string> gl_adapter;
};

const HardwareEncoderTable &hardware_encoder_table()
{
    static const HardwareEncoderTable table = [] {
        HardwareEncoderTable r;

#if GST_GL_HAVE_PLATFORM_GLX
        // under GLX (Linux), gstreamer might have nvidia or vaapi encoders
#if GST_VERSION_MAJOR > 0 && GST_VERSION_MINOR > 25
        // Since GStreamer 1.26, nvh264enc and nvh265enc are the NVENC encoders with presets p1 to p7
        static const std::vector<std::string> nvidia_encoder = {
            "nvh264enc",
            "nvh264enc",
            "nvh265enc",
            "nvh265enc",
            "", "", "",
            "nvjpegenc"
        };
        static const std::vector<std::string> nvidia_profile_description = {
            // nvh264enc encoder
            "nvh264enc rc-mode=constqp preset=p4 bframes=2 gop-size=30 qp-const-i=23 qp-const-p=25 qp-const-b=27 ! video/x-h264, profile=(string)main ! h264parse ! ",
            "nvh264enc rc-mode=constqp preset=p6 bframes=3 rc-lookahead=16 b-adapt=true gop-size=30 ! video/x-h264, profile=(string)high ! h264parse ! ",
            // nvh265enc encoder
            "nvh265enc rc-mode=constqp preset=p4 bframes=2 gop-size=30 qp-const-i=23 qp-const-p=25 qp-const-b=27 ! video/x-h265, profile=(string)main ! h265parse ! ",
            "nvh265enc rc-mode=constqp preset=p6 bframes=3 rc-lookahead=16 b-adapt=true gop-size=30 qp-const-i=21 qp-const-p=23 qp-const-b=25 ! video/x-h265, profile=(string)main ! h265parse ! ",
            "", "", "",
            "nvjpegenc quality=85 ! "
        };
        // nvh264enc and nvh265enc take GLMemory, nvjpegenc does not
        static const std::vector<std::string> nvidia_gl_adapter = {
            GL_TO_GL, GL_TO_GL, GL_TO_GL, GL_TO_GL,
            "", "", "",
            GL_TO_SYSTEM
        };
#else
        // Before GStreamer 1.26, nvh264enc and nvh265enc are the legacy NVENC encoders: their presets are
        // legacy NVENC presets, not supported anymore by recent NVIDIA drivers (error 'Selected preset not supported').
        // The same encoders as nvh264enc and nvh265enc in GStreamer 1.26 are nvcudah264enc and nvcudah265enc, with
        // other property names (rate-control=cqp for rc-mode=constqp, b-frames for bframes, qp-i/p/b for qp-const-i/p/b).
        static const std::vector<std::string> nvidia_encoder = {
            "nvcudah264enc",
            "nvcudah264enc",
            "nvcudah265enc",
            "nvcudah265enc",
            "", "", "",
            "nvjpegenc"
        };
        static const std::vector<std::string> nvidia_profile_description = {
            // nvcudah264enc encoder
            "nvcudah264enc rate-control=cqp preset=p4 b-frames=2 gop-size=30 qp-i=23 qp-p=25 qp-b=27 ! video/x-h264, profile=(string)main ! h264parse ! ",
            "nvcudah264enc rate-control=cqp preset=p6 b-frames=3 rc-lookahead=16 b-adapt=true gop-size=30 ! video/x-h264, profile=(string)high ! h264parse ! ",
            // nvcudah265enc encoder
            "nvcudah265enc rate-control=cqp preset=p4 b-frames=2 gop-size=30 qp-i=23 qp-p=25 qp-b=27 ! video/x-h265, profile=(string)main ! h265parse ! ",
            "nvcudah265enc rate-control=cqp preset=p6 b-frames=3 rc-lookahead=16 b-adapt=true gop-size=30 qp-i=21 qp-p=23 qp-b=25 ! video/x-h265, profile=(string)main ! h265parse ! ",
            "", "", "",
            "nvjpegenc quality=85 ! "
        };
        // nvcudah264enc and nvcudah265enc take GLMemory only if GStreamer was built with CUDA-GL
        // interop (not the case in the snap), and otherwise only NV12 (or Y444) in system memory
        static const std::vector<std::string> nvidia_gl_adapter = {
            GL_TO_SYSTEM_NV12, GL_TO_SYSTEM_NV12, GL_TO_SYSTEM_NV12, GL_TO_SYSTEM_NV12,
            "", "", "",
            GL_TO_SYSTEM
        };
#endif
        static const std::vector<std::string> vaapi_encoder = {
            "vah264enc",
            "vah264enc",
            "vah265enc",
            "vah265enc",
            "", "", "",
            "vajpegenc"
        };
        static const std::vector<std::string> vaapi_profile_description = {
            // vah264enc encoder
            "vah264enc rate-control=cqp target-usage=2 key-int-max=30 qpi=24 qpp=26 qpb=28 ! video/x-h264 ! h264parse ! ",
            "vah264enc rate-control=cqp target-usage=4 key-int-max=30 qpi=22 qpp=24 qpb=26 ! video/x-h264 ! h264parse ! ",
            // vah265enc encoder
            "vah265enc rate-control=cqp target-usage=2 key-int-max=30 qpi=25 qpp=27 qpb=29 ! video/x-h265 ! h265parse ! ",
            "vah265enc rate-control=cqp target-usage=2 key-int-max=30 qpi=23 qpp=25 qpb=27 ! video/x-h265 ! h265parse ! ",
            "", "", "",
            "vajpegenc quality=85 ! "
        };
        // VA-API encoders take VAMemory
        static const std::vector<std::string> vaapi_gl_adapter = {
            GL_TO_VA, GL_TO_VA, GL_TO_VA, GL_TO_VA,
            "", "", "",
            GL_TO_VA
        };

        // test nvidia encoder
        if (GstToolkit::has_feature(nvidia_encoder[0])) {
            // consider that if first nvidia encoder is valid, all others should also be available
            r.feature = nvidia_encoder;
            r.pipeline = nvidia_profile_description;
            r.gl_adapter = nvidia_gl_adapter;
        }
        // test vaapi encoder
        else if (GstToolkit::has_feature(vaapi_encoder[0])) {
            r.feature = vaapi_encoder;
            r.pipeline = vaapi_profile_description;
            r.gl_adapter = vaapi_gl_adapter;
        }
#elif GST_GL_HAVE_PLATFORM_CGL
        // under CGL (Mac), gstreamer might have the VideoToolbox
        r.feature = {
            "vtenc_h264_hw",
            "vtenc_h264_hw",
            "vtenc_h265_hw",
            "vtenc_h265_hw",
            "vtenc_prores",
            "vtenc_prores",
            "", ""
        };
        r.pipeline = {
            // Control vtenc_h264_hw encoder
            "vtenc_h264_hw max-keyframe-interval=30 quality=0.5 ! h264parse ! ",
            "vtenc_h264_hw max-keyframe-interval=30 quality=0.9 ! h264parse ! ",
            "vtenc_h265_hw max-keyframe-interval=30 quality=0.5 ! h265parse ! ",
            "vtenc_h265_hw max-keyframe-interval=30 quality=0.9 ! h265parse ! ",
            "vtenc_prores  max-keyframe-interval=30 quality=0.4 ! ",
            "vtenc_prores  max-keyframe-interval=30 quality=0.9 ! ",
            "", ""
        };
        // VideoToolbox encoders take system memory
        r.gl_adapter = std::vector<std::string>(r.feature.size(), GL_TO_SYSTEM);
        // in other platforms, no hardware encoder
#endif
        return r;
    }();

    return table;
}

// gst element feature name to test with has_feature(), corresponding pipeline
// fragment, and adapter from frames in OpenGL memory, for the low-latency H264
// hardware encoders used for streaming, by order of preference.
struct StreamingEncoder {
    std::string feature;
    std::string pipeline;
    std::string gl_adapter;
};

const std::vector<StreamingEncoder> &hardware_streaming_encoders()
{
    static const std::vector<StreamingEncoder> encoders = {
#if GST_GL_HAVE_PLATFORM_GLX
#if GST_VERSION_MAJOR > 0 && GST_VERSION_MINOR > 25
        {"nvh264enc", "nvh264enc preset=p4 rc-mode=cbr-ld-hq bitrate=6000 ! ", GL_TO_GL},
#else
        // before GStreamer 1.26, legacy nvh264enc presets are not supported by recent NVIDIA drivers: use
        // nvcudah264enc (see hardware_encoder_table); cbr-ld-hq is converted to cbr with low-latency tune
        {"nvcudah264enc", "nvcudah264enc preset=p4 rate-control=cbr tune=low-latency bitrate=6000 ! ", GL_TO_SYSTEM_NV12},
#endif
        {"vah264enc", "vah264enc rate-control=cbr bitrate=6000 target-usage=2 b-frames=0 aud=true cabac=true ! ", GL_TO_VA},
#elif GST_GL_HAVE_PLATFORM_CGL
        {"vtenc_h264_hw", "vtenc_h264_hw realtime=1 allow-frame-reordering=0 ! ", GL_TO_SYSTEM},
#endif
    };

    return encoders;
}

// software low-latency H264 encoder used for streaming
const StreamingEncoder software_streaming_encoder = {
    "x264enc", "x264enc pass=qual quantizer=22 speed-preset=veryfast ! ", GL_TO_SYSTEM
};

} // namespace

string GstToolkit::getEncodingPipeline(GstToolkit::Profile p, GstToolkit::Memory input)
{
    if (p < 0 || p >= GstToolkit::DEFAULT)
        return "";

    const std::string &encoder = software_profile_description()[p];
    if (encoder.empty())
        return "";

    // software encoders take system memory
    if (input == GstToolkit::MEMORY_GL) {
        if (!fragment_available(GL_TO_SYSTEM))
            return "";
        return GL_TO_SYSTEM + encoder;
    }

    return encoder;
}

string GstToolkit::getHardwareEncodingPipeline(GstToolkit::Profile p, GstToolkit::Memory input)
{
    if (p < 0 || p >= GstToolkit::DEFAULT)
        return "";

    const HardwareEncoderTable &hw = hardware_encoder_table();
    if ((size_t) p >= hw.feature.size() || !GstToolkit::has_feature(hw.feature[p]))
        return "";

    if (input == GstToolkit::MEMORY_GL) {
        if (!fragment_available(hw.gl_adapter[p]))
            return "";
        return hw.gl_adapter[p] + hw.pipeline[p];
    }

    return hw.pipeline[p];
}

string GstToolkit::getStreamingEncodingPipeline(bool hardware, GstToolkit::Memory input)
{
    const StreamingEncoder *encoder = nullptr;

    // first available hardware encoder
    if (hardware) {
        for (const StreamingEncoder &e : hardware_streaming_encoders()) {
            if (GstToolkit::has_feature(e.feature)) {
                encoder = &e;
                break;
            }
        }
    }
    // otherwise software encoder
    if (encoder == nullptr && GstToolkit::has_feature(software_streaming_encoder.feature))
        encoder = &software_streaming_encoder;

    if (encoder == nullptr)
        return "";

    if (input == GstToolkit::MEMORY_GL) {
        if (!fragment_available(encoder->gl_adapter))
            return "";
        return encoder->gl_adapter + encoder->pipeline;
    }

    return encoder->pipeline;
}

namespace {

// Name of the gstreamer element encoding the given profile: the first
// element of the pipeline fragment registered as an encoder (e.g. "nvh264enc"
// in "nvh264enc rc-mode=... ! h264parse ! ", "jpegenc" in "videoconvert !
// video/x-raw, ... ! jpegenc ! "), or its first word if none is known.
// Hardware encoder if requested and available, software encoder otherwise.
std::string encoder_element(GstToolkit::Profile profile, bool hardware)
{
    std::string pipeline;
    if (hardware)
        pipeline = GstToolkit::getHardwareEncodingPipeline(profile);
    if (pipeline.empty())
        pipeline = GstToolkit::getEncodingPipeline(profile);

    std::istringstream fragments(pipeline);
    std::string fragment;
    while (std::getline(fragments, fragment, '!')) {
        std::istringstream words(fragment);
        std::string name;
        words >> name;
        GstElementFactory *factory = name.empty() ? nullptr : gst_element_factory_find(name.c_str());
        if (factory) {
            const bool encoder = gst_element_factory_list_is_type(factory, GST_ELEMENT_FACTORY_TYPE_ENCODER);
            gst_object_unref(factory);
            if (encoder)
                return name;
        }
    }

    return pipeline.substr(0, pipeline.find(' '));
}

// Maximum frame width and height accepted by a gstreamer element, from the
// caps of its sink pad template (zero if unlimited or element unknown).
// The nvcodec and va plugins build these caps by asking the device, so the
// values are those of the GPU of this machine. Queried once per element.
std::pair<int, int> element_max_frame_size(const std::string &element)
{
    static std::map<std::string, std::pair<int, int> > cache;
    static std::mutex cache_access;

    if (element.empty())
        return std::pair<int, int>(0, 0);

    const std::lock_guard<std::mutex> lock(cache_access);

    auto known = cache.find(element);
    if (known != cache.end())
        return known->second;

    std::pair<int, int> maxsize(0, 0);
    GstElementFactory *factory = gst_element_factory_find( element.c_str() );
    if (factory) {
        const GList *templates = gst_element_factory_get_static_pad_templates(factory);
        for (const GList *t = templates; t != nullptr; t = t->next) {
            GstStaticPadTemplate *padtemplate = (GstStaticPadTemplate *) t->data;
            // only the input of the encoder is relevant
            if (padtemplate->direction != GST_PAD_SINK)
                continue;
            GstCaps *caps = gst_static_pad_template_get_caps(padtemplate);
            for (guint i = 0; i < gst_caps_get_size(caps); ++i) {
                const GstStructure *s = gst_caps_get_structure(caps, i);
                // keep the largest frame size accepted by any of the caps
                // (a range of sizes, or a single fixed size)
                const GValue *w = gst_structure_get_value(s, "width");
                if (w && GST_VALUE_HOLDS_INT_RANGE(w))
                    maxsize.first = MAX(maxsize.first, gst_value_get_int_range_max(w));
                else if (w && G_VALUE_HOLDS_INT(w))
                    maxsize.first = MAX(maxsize.first, g_value_get_int(w));
                const GValue *h = gst_structure_get_value(s, "height");
                if (h && GST_VALUE_HOLDS_INT_RANGE(h))
                    maxsize.second = MAX(maxsize.second, gst_value_get_int_range_max(h));
                else if (h && G_VALUE_HOLDS_INT(h))
                    maxsize.second = MAX(maxsize.second, g_value_get_int(h));
            }
            gst_caps_unref(caps);
        }
        gst_object_unref(factory);
    }

    cache[element] = maxsize;
    return maxsize;
}

}

void GstToolkit::encoderMaxFrameSize(GstToolkit::Profile profile, bool hardware, int *width, int *height)
{
    const std::pair<int, int> maxsize = element_max_frame_size( encoder_element(profile, hardware) );

    if (width)
        *width = maxsize.first;
    if (height)
        *height = maxsize.second;
}

bool GstToolkit::supportsResolution(GstToolkit::Profile profile, int width, int height, bool hardware)
{
    const std::pair<int, int> maxsize = element_max_frame_size( encoder_element(profile, hardware) );

    // zero means no limit declared by the encoder
    if (maxsize.first > 0 && width > maxsize.first)
        return false;
    if (maxsize.second > 0 && height > maxsize.second)
        return false;

    return true;
}

GstToolkit::Profile GstToolkit::alternativeProfile(GstToolkit::Profile profile, int width, int height, bool hardware)
{
    if ( supportsResolution(profile, width, height, hardware) )
        return profile;

    // prefer the H265 equivalent of the given profile
    const GstToolkit::Profile equivalent = (profile == GstToolkit::H264_HQ) ? GstToolkit::H265_HQ : GstToolkit::H265_RT;
    if ( profile != equivalent && supportsResolution(equivalent, width, height, hardware) )
        return equivalent;

    // else, the first profile which can encode this resolution
    for (int p = GstToolkit::H264_RT; p < GstToolkit::DEFAULT; ++p) {
        if ( supportsResolution((GstToolkit::Profile) p, width, height, hardware) )
            return (GstToolkit::Profile) p;
    }

    return profile;
}

std::string GstToolkit::unsupportedResolution(GstToolkit::Profile profile, int width, int height, bool hardware)
{
    if ( supportsResolution(profile, width, height, hardware) )
        return std::string();

    const std::string element = encoder_element(profile, hardware);
    const std::pair<int, int> maxsize = element_max_frame_size(element);

    std::ostringstream msg;
    msg << profile_name[profile] << " cannot encode " << width << " x " << height
        << " because the encoder " << element << " is limited to "
        << maxsize.first << " x " << maxsize.second << ".";

    // suggest another profile if one can encode this resolution
    const GstToolkit::Profile alternative = alternativeProfile(profile, width, height, hardware);
    if (alternative != profile)
        msg << " Select " << profile_name[alternative] << " instead.";

    return msg.str();
}


// ---------------------------------------------------------- still images

const char* GstToolkit::image_name[GstToolkit::IMAGE_INVALID] = {
    "PNG  (lossless)",
    "JPEG (quality)",
    "WEBP (compressed)"
};

namespace {

// Encoder element and pipeline fragment per still format, plus the maximum
// dimension the *file format* accepts. The elements all advertise unbounded
// caps (width/height up to INT_MAX), so unlike the video encoders their real
// limits cannot be read back with element_max_frame_size(): PNG stores its
// dimensions as 32 bit integers, JPEG as 16 bit (65535), and WEBP's bitstream
// caps both axes at 16383 -- reachable by upscaling a 4096 pixel photo x4.
struct ImageEncoder {
    const char *element;
    const char *pipeline;
    const char *extension;
    int max_size;
    bool alpha;     // the file format can store an alpha channel
};

const ImageEncoder kImageEncoders[GstToolkit::IMAGE_INVALID] = {
    { "pngenc",  "pngenc compression-level=9 ! ",            "png",  0,     true  },
    { "jpegenc", "videoconvert ! video/x-raw, format=Y444, colorimetry=(string)1:4:0:0 ! "
                 "jpegenc quality=95 idct-method=float ! ",  "jpg",  65535, false },
    { "webpenc", "webpenc lossless=false quality=90 speed=4 ! ", "webp", 16383, true }
};

bool valid_image(GstToolkit::Image i)
{
    return i >= GstToolkit::IMAGE_PNG && i < GstToolkit::IMAGE_INVALID;
}

// Still format closest to the given one which can hold an image of this size:
// the given format if it can, else the first format which can.
GstToolkit::Image alternative_image(GstToolkit::Image image, int width, int height)
{
    if ( GstToolkit::supportsImageResolution(image, width, height) )
        return image;

    for (int i = GstToolkit::IMAGE_PNG; i < GstToolkit::IMAGE_INVALID; ++i) {
        if ( GstToolkit::supportsImageResolution((GstToolkit::Image) i, width, height) )
            return (GstToolkit::Image) i;
    }

    return image;
}

}

std::string GstToolkit::getImageEncodingPipeline(GstToolkit::Image image)
{
    if (!valid_image(image))
        return std::string();

    // an encoder can be missing (webpenc lives in gst-plugins-bad); report no
    // pipeline rather than building one which would fail at gst_parse_launch
    if (!GstToolkit::has_feature(kImageEncoders[image].element))
        return std::string();

    return kImageEncoders[image].pipeline;
}

const char* GstToolkit::imageFileExtension(GstToolkit::Image image)
{
    return valid_image(image) ? kImageEncoders[image].extension : "png";
}

bool GstToolkit::imageSupportsAlpha(GstToolkit::Image image)
{
    return valid_image(image) && kImageEncoders[image].alpha;
}

bool GstToolkit::supportsImageResolution(GstToolkit::Image image, int width, int height)
{
    if (!valid_image(image))
        return false;

    // zero means no limit for this format, or an unknown resolution
    const int max_size = kImageEncoders[image].max_size;
    if (max_size > 0 && (width > max_size || height > max_size))
        return false;

    return true;
}

std::string GstToolkit::unsupportedImageResolution(GstToolkit::Image image, int width, int height)
{
    if ( supportsImageResolution(image, width, height) )
        return std::string();

    if (!valid_image(image))
        return std::string("Unknown image format.");

    std::ostringstream msg;
    msg << image_name[image] << " cannot store " << width << " x " << height
        << " because the format is limited to " << kImageEncoders[image].max_size
        << " pixels in each direction.";

    // suggest another format if one can hold this resolution
    const GstToolkit::Image alternative = alternative_image(image, width, height);
    if (alternative != image)
        msg << " Select " << image_name[alternative] << " instead.";

    return msg.str();
}

int GstToolkit::getPlayBackwardGop(GstToolkit::Profile profile, int width, int height)
{
    const int default_interval = 30;                 // profile default, ~1s @ 30fps
    const int min_interval = 8;                      // floor: avoid bloating file size / encode time
    const double reference_pixels = 1920.0 * 1080.0; // 1080p as the "interval unchanged" point

    double codec_factor = 1.0; // H.264: reference decode cost
    if (profile == GstToolkit::H265_RT || profile == GstToolkit::H265_HQ)
        codec_factor = 0.6;    // H.265 decode is markedly heavier per pixel
    else if (profile == GstToolkit::VPX_RT)
        codec_factor = 0.5;    // VP9 software decode is heavier still

    double pixels = (double) MAX(1, width) * (double) MAX(1, height);
    double resolution_factor = MIN(1.0, reference_pixels / pixels);

    int interval = (int) std::lround(default_interval * codec_factor * resolution_factor);
    return CLAMP(interval, min_interval, default_interval);
}

float GstToolkit::canPlayBackward(bool has_bframes, int width, int height,
                                   guint keyframe_count, guint gop_size_min, guint gop_size_max)
{
    // no usable keyframe/GOP data at all: backward playback needs a
    // keyframe to seek to and re-decode forward from
    if (keyframe_count < 1 || gop_size_min < 1 || gop_size_max < 1)
        return 0.f;

    GstToolkit::Profile profile = has_bframes ? GstToolkit::H265_RT : GstToolkit::H264_RT;
    int target = getPlayBackwardGop(profile, width, height);

    // worst-case GOP within budget: comfortably smooth
    if ((int) gop_size_max <= target)
        return 1.f;

    // beyond budget: score falls off smoothly as the worst-case reverse
    // decode cost grows past what's comfortable at this resolution/codec
    return CLAMP((float) target / (float) gop_size_max, 0.f, 1.f);
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
// list ordered with higher priority at the end (e.g. nvidia proprietary before va-api)
const char *plugins[15] = { "vulkanh264dec", "vulkanh265dec", "vaav1dec", "nvav1dec",
                                "vah264dec", "nvh264dec", "vah265dec", "nvh265dec", "vampeg2videodec", "nvmpeg2videodec", "nvmpeg4videodec",
                                "vavp8dec", "nvvp8dec", "vavp9dec", "nvvp9dec"
                               };
    const int N = 15;
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


// query the caps of the first src pad of the element (caller must unref)
static GstCaps *query_src_caps(GstElement *elem)
{
    GstCaps *caps = NULL;
    GstIterator *iter = gst_element_iterate_src_pads(elem);
    if (iter != nullptr) {
        GValue vPad = G_VALUE_INIT;
        if (gst_iterator_next(iter, &vPad) == GST_ITERATOR_OK) {
            GstPad *pad = GST_PAD(g_value_get_object(&vPad));
            if (pad)
                caps = gst_pad_query_caps(pad, NULL);
            g_value_unset(&vPad);
        }
        gst_iterator_free(iter);
    }
    return caps;
}

// true if at least one structure of the caps gives a fixed width and height
static bool caps_have_fixed_size(GstCaps *caps)
{
    if (caps == NULL || gst_caps_is_any(caps) || gst_caps_is_empty(caps))
        return false;

    for (guint c = 0; c < gst_caps_get_size(caps); ++c) {
        GstStructure *s = gst_caps_get_structure(caps, c);
        int w = 0, h = 0;
        if (gst_structure_get_int(s, "width", &w) && gst_structure_get_int(s, "height", &h))
            return true;
    }
    return false;
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

        // Query caps in READY state first: devices are opened (e.g. v4l2src) but
        // no streaming thread is running. In PAUSED, a live source starts negotiating
        // in its streaming thread, and querying caps concurrently crashes v4l2src.
        GstCaps *device_caps = NULL;
        if (gst_element_set_state (pipeline_, GST_STATE_READY) != GST_STATE_CHANGE_FAILURE)
            device_caps = query_src_caps(elem);

        // Some sources only open the device when going to PAUSED (e.g. avfvideosrc)
        if (!caps_have_fixed_size(device_caps)) {
            if (device_caps)
                gst_caps_unref(device_caps);
            device_caps = NULL;
            if (gst_element_set_state (pipeline_, GST_STATE_PAUSED) != GST_STATE_CHANGE_FAILURE)
                device_caps = query_src_caps(elem);
        }

        if (device_caps) {
            // loop over all caps offered by the pad
            int C = gst_caps_get_size(device_caps);
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

                // only system memory caps (e.g. ignore memory:DMABuf with format DMA_DRM)
                GstCapsFeatures *features = gst_caps_get_features (device_caps, c);
                if ( features && !gst_caps_features_is_any(features) &&
                     !gst_caps_features_is_equal(features, GST_CAPS_FEATURES_MEMORY_SYSTEM_MEMORY))
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
                            gchar *serialized = gst_value_serialize( gst_value_list_get_value(val, n) );
                            std::string f = serialized ? serialized : "";
                            g_free(serialized);

                            // preference order : 1) RGB formats, 2) ALL OTHER
                            // select if it is RGB (e.g. RGBx, BGRA) and not already RGB in config
                            if ( isRGBFormat(f) && !isRGBFormat(config.format) ) {
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
                        gchar *serialized = gst_value_serialize(val);
                        config.format = serialized ? serialized : "";
                        g_free(serialized);
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
                if (config.width > 0 && config.height > 0 && config.fps_numerator > 0 && config.fps_denominator > 0)
                    configs.insert(config);
            }
            gst_caps_unref(device_caps);
        }

        // terminate pipeline
        gst_element_set_state (pipeline_, GST_STATE_NULL);

        g_object_unref (elem);
    }

    gst_object_unref (pipeline_);

    return configs;
}


void GstToolkit::download_file(const std::string &url, const std::string &dest)
{
    fs::path d(dest);
    if (d.has_parent_path())
        fs::create_directories(d.parent_path());
    std::string part = dest + ".part";

    fprintf(stderr, "Downloading %s to %s.", url.c_str(), dest.c_str());
    std::string desc = "curlhttpsrc location=\"" + url +
                       "\" ! filesink location=\"" + part + "\"";
    GError *err = nullptr;
    GstElement *pipe = gst_parse_launch(desc.c_str(), &err);
    if (!pipe) {
        std::string msg = err ? err->message : "unknown";
        if (err) g_error_free(err);
        throw std::runtime_error("gst_parse_launch: " + msg);
    }
    gst_element_set_state(pipe, GST_STATE_PLAYING);

    GstBus *bus = gst_element_get_bus(pipe);
    GstMessage *msg = gst_bus_timed_pop_filtered(bus, GST_CLOCK_TIME_NONE,
        (GstMessageType)(GST_MESSAGE_EOS | GST_MESSAGE_ERROR));
    bool ok = GST_MESSAGE_TYPE(msg) == GST_MESSAGE_EOS;
    std::string what;
    if (!ok) {
        GError *e = nullptr;
        gst_message_parse_error(msg, &e, nullptr);
        what = e ? e->message : "unknown";
        if (e) g_error_free(e);
    }
    gst_message_unref(msg);
    gst_object_unref(bus);
    gst_element_set_state(pipe, GST_STATE_NULL);
    gst_object_unref(pipe);

    if (!ok) {
        fs::remove(part);
        throw std::runtime_error("download failed: " + what);
    }
    fs::rename(part, dest);
    fprintf(stderr, "saved %s (%.1f MB)\n", dest.c_str(),
            fs::file_size(dest) / 1e6);
}

bool GstToolkit::encoder_works(const char *enc)
{
    GstElementFactory *f = gst_element_factory_find(enc);
    if (!f)
        return false;
    gst_object_unref(f);

    // 320x240: comfortably above hardware encoders' minimum frame sizes
    // (NVENC rejects tiny frames), still instant to encode
    std::string desc =
        "videotestsrc num-buffers=1 ! video/x-raw,width=320,height=240 ! "
        "videoconvert ! " + std::string(enc) + " ! fakesink";
    GError *err = nullptr;
    GstElement *pipe = gst_parse_launch(desc.c_str(), &err);
    if (!pipe) {
        if (err) g_error_free(err);
        return false;
    }
    gst_element_set_state(pipe, GST_STATE_PLAYING);
    GstBus *bus = gst_element_get_bus(pipe);
    GstMessage *msg = gst_bus_timed_pop_filtered(bus, 5 * GST_SECOND,
        (GstMessageType)(GST_MESSAGE_EOS | GST_MESSAGE_ERROR));
    bool ok = msg && GST_MESSAGE_TYPE(msg) == GST_MESSAGE_EOS;
    if (msg)
        gst_message_unref(msg);
    gst_object_unref(bus);
    gst_element_set_state(pipe, GST_STATE_NULL);
    gst_object_unref(pipe);
    if (!ok)
        fprintf(stderr, "encoder %s not usable, trying next\n", enc);
    return ok;
}
