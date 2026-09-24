#ifndef __GSTGUI_TOOLKIT_H_
#define __GSTGUI_TOOLKIT_H_

#include <gst/gst.h>

#include <string>
#include <list>
#include <set>

namespace GstToolkit
{

typedef enum {
    TIME_STRING_FIXED = 0,
    TIME_STRING_ADJUSTED,
    TIME_STRING_MINIMAL,
    TIME_STRING_READABLE
} time_string_mode;

std::string time_to_string(guint64 t, time_string_mode m = TIME_STRING_ADJUSTED);
std::string filename_to_uri(std::string filename);

std::string gst_version();

std::list<std::string> all_plugins();
std::list<std::string> enable_gpu_decoding_plugins(bool enable = true);
std::string used_gpu_decoding_plugins(GstElement *gstbin);
std::string used_decoding_plugins(GstElement *gstbin);

std::list<std::string> all_plugin_features(const std::string &pluginname);
bool has_feature (const std::string &name);
bool enable_feature (const std::string &name, bool enable);


// Video encoding profiles used for recording / exporting video
typedef enum {
    H264_RT = 0,
    H264_HQ,
    H265_RT,
    H265_HQ,
    PRORES_RT,
    PRORES_HQ,
    VPX_RT,
    JPEG_MULTI,
    DEFAULT
} Profile;

extern const char* profile_name[DEFAULT];

// Memory of the raw video frames given to an encoding pipeline fragment,
// i.e. what the upstream pipeline delivers:
// - MEMORY_SYSTEM : frames in system memory, in any format (the upstream
//   pipeline ends with a videoconvert, which negotiates the encoder format)
// - MEMORY_GL : RGBA frames in OpenGL memory (video/x-raw(memory:GLMemory))
// The encoding pipeline fragments start with the adapter needed to give
// these frames to the encoder (e.g. download from OpenGL memory for an
// encoder which cannot take it), followed by the encoder and parser.
typedef enum {
    MEMORY_SYSTEM = 0,
    MEMORY_GL
} Memory;

// gst pipeline fragment ([adapter !] encoder ! parser ! , e.g. "x264enc ... ! h264parse ! ")
// for the given profile, using whichever software encoder is actually
// installed: falls back from x264enc/x265enc to openh264enc, or returns an
// empty string if no software encoder is available for that profile.
std::string getEncodingPipeline(Profile p, Memory input = MEMORY_SYSTEM);

// Hardware-accelerated equivalent of getEncodingPipeline(): NVENC or VAAPI
// on Linux, VideoToolbox on macOS (detected once and cached). Returns an
// empty string if no hardware encoder is available for that profile on
// this platform/GPU, or if the adapter for the given input is not available.
std::string getHardwareEncodingPipeline(Profile p, Memory input = MEMORY_SYSTEM);

// gst pipeline fragment ([adapter !] encoder ! , e.g. "x264enc ... ! ") of the
// low-latency H264 encoder used for network streaming (constant bit rate, no
// parser: the caller adds the caps, parser and payloader it needs). Hardware
// encoder if requested and available, software encoder otherwise; empty
// string if none is available.
std::string getStreamingEncodingPipeline(bool hardware, Memory input = MEMORY_SYSTEM);

// Maximum frame width and height accepted by the encoder used for the given
// profile, read from the caps of the gstreamer element (zero if it imposes no
// limit). Hardware encoders are typically limited (e.g. 4096 x 4096 for NVENC
// H264) whereas software encoders are not. Queried once per element.
void encoderMaxFrameSize(Profile p, bool hardware, int *width, int *height);

// Test if the encoder used for the given profile can encode frames of this
// size. A width or height of zero means the resolution is unknown: supported.
bool supportsResolution(Profile p, int width, int height, bool hardware);

// Profile closest to the given one which can encode frames of this size:
// the given profile if it can, else its H265 equivalent if it can, else the
// first profile which can, else the given profile.
Profile alternativeProfile(Profile p, int width, int height, bool hardware);

// Empty string if the resolution is supported, or an explanatory error
// message (naming the encoder, its limit and an alternative profile).
std::string unsupportedResolution(Profile p, int width, int height, bool hardware);


// Still image encoding formats, used to transcode a single image (as opposed
// to the Profile enum above, which encodes a stream of frames). Kept separate
// from Profile so that neither can be passed where the other is expected, and
// so that helpers iterating the video profiles (alternativeProfile) never
// stray into still formats.
typedef enum {
    IMAGE_PNG = 0,
    IMAGE_JPEG,
    IMAGE_WEBP,
    IMAGE_INVALID
} Image;

extern const char* image_name[IMAGE_INVALID];

// gst pipeline fragment (encoder ! , e.g. "pngenc compression-level=9 ! ") for
// the given still format, or an empty string when its encoder is not installed
// on this system. Probed once, as getEncodingPipeline() does for video.
std::string getImageEncodingPipeline(Image i);

// Filename extension for the given still format, without the dot ("png").
const char* imageFileExtension(Image i);

// True if the given still format can store an alpha channel. JPEG cannot,
// so transparency is dropped when encoding to it; PNG and WEBP can, and a
// source carrying alpha should be kept in RGBA all the way to the encoder.
bool imageSupportsAlpha(Image i);

// Test if the given still format can hold an image of this size. Unlike the
// video encoders, these declare unbounded caps (width/height up to INT_MAX)
// while the file formats themselves are limited, so the limits come from a
// table rather than from the element. A width or height of zero means the
// resolution is unknown: supported.
bool supportsImageResolution(Image i, int width, int height);

// Empty string if the resolution fits the format, or an explanatory error
// message naming the format, its limit and a format which would fit.
std::string unsupportedImageResolution(Image i, int width, int height);

// Keyframe interval (in frames) for smooth backward playback of video
// encoded with the given profile at the given frame size. 
// This is a static estimate from those factors, not a benchmark.
// Default interval is 30 per second.
int getPlayBackwardGop(Profile profile, int width, int height);

// Score, from 0.f (cannot play backward at all) to 1.f (comfortably
// smooth), for how well a stream with the given actual keyframe structure
// (in frames) can be played backward at the given frame size. 
float canPlayBackward(bool has_bframes, int width, int height,
                       guint keyframe_count, guint gop_size_min, guint gop_size_max);


// True for a raw video format holding RGB components
bool isRGBFormat(const std::string &format);

struct PipelineConfig {
    gint width;
    gint height;
    gint fps_numerator;
    gint fps_denominator;
    std::string stream;
    std::string format;

    PipelineConfig() {
        width = 0;
        height = 0;
        fps_numerator = 30;
        fps_denominator = 1;
        stream = "";
        format = "";
    }

    inline bool operator < (const PipelineConfig b) const
    {
        gint formatscore = isRGBFormat(this->format) ? 2 : 1; // best score for RGB formats
        gint b_formatscore = isRGBFormat(b.format) ? 2 : 1;
        float fps = static_cast<float>(this->fps_numerator) / static_cast<float>(this->fps_denominator);
        float b_fps = static_cast<float>(b.fps_numerator) / static_cast<float>(b.fps_denominator);
        float aspect = static_cast<float>(this->width) / static_cast<float>(this->height);
        float b_aspect = static_cast<float>(b.width) / static_cast<float>(b.height);
        float score = aspect * fps * static_cast<float>(this->width * this->height * formatscore);
        float b_score = b_aspect * b_fps * static_cast<float>(b.width * b.height * b_formatscore);
        return score < b_score;
    }
};

struct better_config_comparator
{
    inline bool operator () (const PipelineConfig a, const PipelineConfig b) const
    {
        return (a < b);
    }
};

typedef std::set<PipelineConfig, better_config_comparator> PipelineConfigSet;

PipelineConfigSet getPipelineConfigs(const std::string &src_description);

// Download url to dest with GStreamer (souphttpsrc follows the HTTPS
// redirects), via a .part file renamed only on success so an interrupted
// run never leaves a truncated file behind. Throws std::runtime_error on
// failure.
void download_file(const std::string &url, const std::string &dest);

// Functional probe of a GStreamer encoder: 
// Encodes one tiny test frame end-to-end and checks for EOS.
// Returns true if the encoder works, false otherwise.
bool encoder_works(const char *enc);

// Raises a GStreamer debug category's threshold for its lifetime, restoring it
// on destruction. Used to mute one chatty, benign category around a call
// without touching global logging or any other category.
class GstCategoryHush {
public:
    GstCategoryHush(const char *category, GstDebugLevel level) : name_(category)
    {
        gst_debug_set_threshold_for_name(category, level);
    }
    ~GstCategoryHush() { gst_debug_unset_threshold_for_name(name_); }
    GstCategoryHush(const GstCategoryHush &) = delete;
    GstCategoryHush &operator=(const GstCategoryHush &) = delete;
private:
    const char *name_;
};

}

#endif // __GSTGUI_TOOLKIT_H_
