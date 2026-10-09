/*
 * This file is part of vimix - video live mixer
 *
 * **Copyright** (C) 2019-2025 Bruno Herbelin <bruno.herbelin@gmail.com>
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

#include "Transcoder.h"
#include "Log.h"
#include "Settings.h"
#include "Upscaler.h"
#include "SplitMediaPlayer.h"
#include "IconsFontAwesome5.h"
#include "Toolkit/SystemToolkit.h"
#include "Toolkit/BaseToolkit.h"

#include <string>
#include <sys/stat.h>
#include <chrono>
#include <filesystem>
#include <glib.h>
#include <gst/app/gstappsink.h>
#include <gst/app/gstappsrc.h>
#include <gst/video/video.h>
#include <algorithm>
#include <cctype>
#include <cstring>
#include <stdexcept>
#include <vector>

namespace {

// Upscaling only: how many frames the appsink and appsrc queues may hold.
// Small on purpose, an upscaled frame being several times the size of a
// source frame (4K x4 is 100MB of RGB).
constexpr int UPSCALE_QUEUE_LENGTH = 4;

// Upscaling only: stall watchdog. Once a per-frame baseline is known (from
// the first frames), give up if a frame takes more than STALL_TIMEOUT_FACTOR
// times that baseline, but never sooner than STALL_MIN_TIMEOUT_MS. This
// catches an encoder which cannot keep up or cannot handle the resolution
// (a hardware encoder past its limits): it would otherwise block the worker
// forever on a push (appsrc block=true). Until the baseline is known,
// STALL_INITIAL_TIMEOUT_MS is a coarse safety net covering model loading.
constexpr int STALL_TIMEOUT_FACTOR = 3;
constexpr long long STALL_MIN_TIMEOUT_MS = 30000;
constexpr long long STALL_INITIAL_TIMEOUT_MS = 300000;

long long now_ms()
{
    return (long long) std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::steady_clock::now().time_since_epoch()).count();
}

// Look for a keyframe-interval property in a GstToolkit encoding pipeline
// fragment -- "key-int-max=" (x264enc/x265enc/vah264enc/vah265enc),
// "gop-size=" (nvh264enc/nvh265enc/openh264enc),  "keyframe-max-dist="
// (vp9enc), or "max-keyframe-interval=" (vtenc_) -- and replace its value with `interval`. 
// A no-op when none of those are present (ProRes/JPEG): both are all-intra
// already, so there is nothing to tighten.
std::string apply_keyframe_interval(const std::string &pipeline, int interval)
{
    static const char *properties[] = { "key-int-max=", "gop-size=", "keyframe-max-dist=", "max-keyframe-interval=" };

    for (const char *prop : properties) {
        size_t pos = pipeline.find(prop);
        if (pos == std::string::npos)
            continue;

        size_t value_start = pos + strlen(prop);
        size_t value_end = value_start;
        while (value_end < pipeline.size() && isdigit((unsigned char) pipeline[value_end]))
            value_end++;

        std::string patched = pipeline;
        patched.replace(value_start, value_end - value_start, std::to_string(interval));
        return patched;
    }

    return pipeline;
}

// Pad probe installed on the multifilesink's sink pad when transcoding to
// GstToolkit::JPEG_MULTI: lets exactly MAX_JPEG_FRAMES buffers through, then
// pushes EOS once (so the pipeline finishes cleanly) and drops the rest.
GstPadProbeReturn limit_jpeg_frames_probe(GstPad *pad, GstPadProbeInfo *info, gpointer user_data)
{
    if (!(GST_PAD_PROBE_INFO_TYPE(info) & GST_PAD_PROBE_TYPE_BUFFER))
        return GST_PAD_PROBE_OK;

    guint64 *count = static_cast<guint64 *>(user_data);

    if (*count >= (guint64) Transcoder::MAX_JPEG_FRAMES)
        return GST_PAD_PROBE_DROP;

    (*count)++;
    if (*count == (guint64) Transcoder::MAX_JPEG_FRAMES) {
        Log::Warning("Transcoder: reached maximum of %d images; stopping.", Transcoder::MAX_JPEG_FRAMES);
        gst_pad_push_event(pad, gst_event_new_eos());
    }

    return GST_PAD_PROBE_OK;
}

} // namespace

Transcoder::Transcoder(const std::string& input_filename, const std::string& output_filename)
    : input_filename_(input_filename)
    , output_filename_(output_filename)
    , pipeline_(nullptr)
    , bus_(nullptr)
    , is_image_sequence_(false)
    , is_still_image_(false)
    , started_(false)
    , finished_(false)
    , success_(false)
    , abort_(false)
    , duration_(-1)
    , position_(0)
    , upscale_factor_(1)
    , range_begin_(GST_CLOCK_TIME_NONE)
    , range_end_(GST_CLOCK_TIME_NONE)
    , range_ready_(false)
    , range_by_eos_(false)
    , range_pad_(nullptr)
    , copy_mux_(nullptr)
    , copy_audio_(false)
    , copy_failed_(false)
{
    // Unless given, output filename will be generated in start() based on options
}

Transcoder::Transcoder(const std::list<std::string>& input_files, const std::string& output_filename)
    : input_filename_(input_files.empty() ? std::string() : input_files.front())
    , input_files_(input_files.size() > 1 ? input_files : std::list<std::string>())
    , output_filename_(output_filename)
    , pipeline_(nullptr)
    , bus_(nullptr)
    , is_image_sequence_(false)
    , is_still_image_(false)
    , started_(false)
    , finished_(false)
    , success_(false)
    , abort_(false)
    , duration_(-1)
    , position_(0)
    , upscale_factor_(1)
    , range_begin_(GST_CLOCK_TIME_NONE)
    , range_end_(GST_CLOCK_TIME_NONE)
    , range_ready_(false)
    , range_by_eos_(false)
    , range_pad_(nullptr)
    , copy_mux_(nullptr)
    , copy_audio_(false)
    , copy_failed_(false)
{
    // Unless given, output filename will be generated in start() based on options
}

Transcoder::~Transcoder()
{
    // an upscaling worker still running must be stopped and joined before
    // the members it uses are destroyed
    stop();
    if (worker_.joinable())
        worker_.join();

    if (pipeline_) {
        gst_element_set_state(pipeline_, GST_STATE_NULL);
        gst_object_unref(pipeline_);
        pipeline_ = nullptr;
    }
    if (bus_) {
        gst_object_unref(bus_);
        bus_ = nullptr;
    }
    if (range_pad_) {
        gst_object_unref(range_pad_);
        range_pad_ = nullptr;
    }
    if (copy_mux_) {
        gst_object_unref(copy_mux_);
        copy_mux_ = nullptr;
    }
}

void Transcoder::setError(const std::string &message)
{
    std::lock_guard<std::mutex> lock(message_mutex_);
    error_message_ = message;
}

void Transcoder::setStatus(const std::string &message)
{
    std::lock_guard<std::mutex> lock(message_mutex_);
    status_message_ = message;
}

std::string Transcoder::error() const
{
    std::lock_guard<std::mutex> lock(message_mutex_);
    return error_message_;
}

std::string Transcoder::status() const
{
    std::lock_guard<std::mutex> lock(message_mutex_);

    // prefix with animation if the status is non-empty
    static const char* animation[] = { ICON_FA_HOURGLASS_START,ICON_FA_HOURGLASS_HALF,ICON_FA_HOURGLASS_END,ICON_FA_HOURGLASS };
    if (!status_message_.empty())
        return std::string(animation[(g_get_monotonic_time() / 300000) % 4]) + " " + status_message_;

    return status_message_;
}

std::string Transcoder::generateOutputFilename(const std::string& input, const TranscoderOptions& options)
{
    // Find the last dot to get extension
    size_t dot_pos = input.rfind('.');
    size_t slash_pos = input.rfind('/');

    std::string base;
    if (dot_pos != std::string::npos && (slash_pos == std::string::npos || dot_pos > slash_pos)) {
        base = input.substr(0, dot_pos);
    } else {
        base = input;
    }

    // JPEG_MULTI produces a folder of numbered images, not a single file
    if (!options.isImage() && options.profile() == GstToolkit::JPEG_MULTI) {
        std::string folder = base;
        std::string output = folder;
        struct stat buffer;
        int counter = 1;
        while (stat(output.c_str(), &buffer) == 0) {
            output = folder + "_" + std::to_string(counter);
            counter++;
        }
        return output;
    }

    // Build suffix based on transcoder options
    std::string suffix = "";
    const int factor = Upscaler::model(options.upscaler).factor;
    if (factor > 1)
        suffix += "_upscaled_x" + std::to_string(factor);
    // keyframes and audio do not apply to a still image
    if (!options.isImage()) {
        if (options.stream_copy)
            suffix += "_copy";
        else if (options.force_keyframes)
            suffix += "_bidir";
        if (options.force_no_audio)
            suffix += "_noaudio";
    }

    // Still image: the extension of the chosen format. Otherwise WebM
    // container for VPX (vp9enc) and QuickTime container for the rest,
    // matching VideoRecorder's convention (Recorder.cpp)
    std::string extension;
    if (options.isImage())
        extension = GstToolkit::imageFileExtension(options.format());
    else if (options.stream_copy)
        extension = "mkv";
    else
        extension = (options.profile() == GstToolkit::VPX_RT) ? "webm" : "mov";

    std::string output = base + suffix + "." + extension;
    struct stat buffer;
    int counter = 1;
    while (stat(output.c_str(), &buffer) == 0) {
        output = base + suffix + "_" + std::to_string(counter) + "." + extension;
        counter++;
    }

    return output;
}

bool Transcoder::start(const TranscoderOptions& options)
{
    if (started_) {
        setError("Transcoder already started");
        return false;
    }

    is_still_image_ = options.isImage();
    is_image_sequence_ = !is_still_image_ && !options.stream_copy && options.profile() == GstToolkit::JPEG_MULTI;

    // range of the source to transcode (not for a still image)
    if (!is_still_image_) {
        range_begin_ = options.begin;
        range_end_ = options.end;
    }

    // Resolve the upscaling model: an unknown name, or a build without the
    // ncnn Vulkan backend, simply means no upscaling
    const UpscalerModel &upscaler = Upscaler::model(options.upscaler);
    upscale_factor_ = (Upscaler::available() && upscaler.factor > 1) ? upscaler.factor : 1;

    // Generate output filename (or folder, for JPEG_MULTI) based on options;
    // a sequence of videos is named after the part common to all files
    if (output_filename_.empty()) {
        std::string input = input_filename_;
        if (!input_files_.empty()) {
            std::list<std::string> names;
            for (const auto &f : input_files_)
                names.push_back( SystemToolkit::base_filename(f) );
            std::string name = BaseToolkit::common_prefix(names);
            while (!name.empty() && (name.back() == '_' || name.back() == '-' || name.back() == '.' || name.back() == ' '))
                name.pop_back();
            if (!name.empty())
                input = SystemToolkit::path_filename(input_filename_) + name + ".split";
        }
        output_filename_ = generateOutputFilename(input, options);
    }

    // Check if input file exists
    struct stat buffer;
    if (stat(input_filename_.c_str(), &buffer) != 0) {
        setError("Input file does not exist: " + input_filename_);
        Log::Warning("Transcoder: Input file does not exist: %s", input_filename_.c_str());
        return false;
    }

    Log::Info("Transcoder: Starting transcoding from '%s' to '%s' (%s)",
              input_filename_.c_str(), output_filename_.c_str(),
              is_still_image_ ? GstToolkit::image_name[options.format()]
                              : GstToolkit::profile_name[options.profile()]);

    // Discover source to detect interlacing and the presence of an audio
    // stream (no more bitrate matching: profiles are fixed-quality)
    gchar *src_uri = gst_filename_to_uri(input_filename_.c_str(), nullptr);
    if (!src_uri) {
        setError("Failed to create URI from filename");
        Log::Warning("Transcoder: Failed to create URI from filename");
        return false;
    }

    GstDiscoverer *discoverer = gst_discoverer_new(10 * GST_SECOND, nullptr);
    if (!discoverer) {
        setError("Failed to create discoverer");
        Log::Warning("Transcoder: Failed to create discoverer");
        g_free(src_uri);
        return false;
    }

    GError *discover_error = nullptr;
    GstDiscovererInfo *disc_info = gst_discoverer_discover_uri(discoverer, src_uri, &discover_error);

    bool has_audio = false;
    bool source_interlaced = false;
    bool source_is_image = false;
    guint frame_width = 0;
    guint frame_height = 0;

    if (disc_info) {
        GList *video_streams = gst_discoverer_info_get_video_streams(disc_info);
        if (video_streams) {
            GstDiscovererVideoInfo *vinfo = (GstDiscovererVideoInfo*)video_streams->data;
            source_is_image = gst_discoverer_video_info_is_image(vinfo);
            source_interlaced = gst_discoverer_video_info_is_interlaced(vinfo);
            if (source_interlaced)
                Log::Info("Transcoder: Source video is interlaced, deinterlacing will be applied");
            frame_width = gst_discoverer_video_info_get_width(vinfo);
            frame_height = gst_discoverer_video_info_get_height(vinfo);
            gst_discoverer_stream_info_list_free(video_streams);
        } else {
            Log::Warning("Transcoder: No video stream detected");
        }

        GList *audio_streams = gst_discoverer_info_get_audio_streams(disc_info);
        if (audio_streams) {
            has_audio = true;
            gst_discoverer_stream_info_list_free(audio_streams);
        }
        gst_discoverer_info_unref(disc_info);
    } else {
        Log::Warning("Transcoder: Could not get discoverer info");
    }

    if (discover_error) {
        Log::Warning("Transcoder: Discovery error: %s", discover_error->message);
        g_error_free(discover_error);
    }

    g_object_unref(discoverer);

    // Stream copy: no decoding, no encoding
    if (!is_still_image_ && options.stream_copy) {
        g_free(src_uri);
        if (!input_files_.empty()) {
            setError("Cannot copy streams of a sequence of videos");
            return false;
        }
        return startCopy(input_filename_, has_audio && !options.force_no_audio);
    }

    // A sequence of videos is decoded as a single stream by splitmuxsrc
    // (src_uri, of the first file, was used only to discover the videos)
    if (!input_files_.empty()) {
        g_free(src_uri);
        src_uri = g_strdup( SplitMediaPlayer::SplitUri(input_files_).c_str() );
    }

    // Upscaling multiplies the frame size: everything downstream (the
    // resolution check, the keyframe interval, the encoder caps) works on
    // the size of the frames actually handed to the encoder
    // Rescaling to a given output size (not when upscaling)
    const bool rescale = !is_still_image_ && upscale_factor_ == 1 && options.width > 0 && options.height > 0;
    const int frame_out_width = rescale ? (int) options.width : (int) frame_width * upscale_factor_;
    const int frame_out_height = rescale ? (int) options.height : (int) frame_height * upscale_factor_;

    // A still format can only hold one frame: refuse to silently turn a video
    // into its first frame, which is never what the user meant
    if (is_still_image_ && !source_is_image) {
        setError("Cannot encode a video to a still image format: " + input_filename_
                 + " is not an image");
        Log::Warning("Transcoder: %s is not an image, cannot encode it to %s",
                     input_filename_.c_str(), GstToolkit::image_name[options.format()]);
        g_free(src_uri);
        return false;
    }

    // verify the chosen format can hold the resolution of the produced frames
    const std::string unsupported =
        is_still_image_
            ? GstToolkit::unsupportedImageResolution(options.format(), frame_out_width, frame_out_height)
            : GstToolkit::unsupportedResolution(options.profile(), frame_out_width, frame_out_height,
                                                Settings::application.render.gpu_decoding);
    if (!unsupported.empty()) {
        setError(unsupported);
        Log::Warning("Transcoder: %s", unsupported.c_str());
        g_free(src_uri);
        return false;
    }

    // Pick the encoding pipeline fragment. A still image has a single encoder
    // and no hardware variant; a video profile is hardware accelerated when
    // available and enabled, software otherwise.
    std::string video_encoder;
    if (is_still_image_) {
        video_encoder = GstToolkit::getImageEncodingPipeline(options.format());
        if (video_encoder.empty()) {
            setError(std::string("No encoder available for image format ")
                     + GstToolkit::image_name[options.format()]);
            Log::Warning("Transcoder: No encoder available for image format %s",
                         GstToolkit::image_name[options.format()]);
            g_free(src_uri);
            return false;
        }
    }
    else {
        video_encoder = GstToolkit::getHardwareEncodingPipeline(options.profile());
        bool hardware = Settings::application.render.gpu_decoding && !video_encoder.empty();
        if (!hardware)
            video_encoder = GstToolkit::getEncodingPipeline(options.profile());

        if (video_encoder.empty()) {
            setError(std::string("No encoder available for profile ") + GstToolkit::profile_name[options.profile()]);
            Log::Warning("Transcoder: No encoder available for profile %s", GstToolkit::profile_name[options.profile()]);
            g_free(src_uri);
            return false;
        }

        // Tighten the keyframe interval for smoother backward playback (no-op
        // for ProRes/JPEG, which are already all-intra)
        if (options.force_keyframes) {
            int interval = GstToolkit::getPlayBackwardGop(options.profile(), frame_out_width, frame_out_height);
            std::string patched = apply_keyframe_interval(video_encoder, interval);
            if (patched != video_encoder)
                Log::Info("Transcoder: keyframe interval set to %d frames for backward playback", interval);
            video_encoder = patched;
        }
    }

    // The numbered JPEG sequence goes into a folder of its own, created up
    // front so that a failure is reported before anything else is attempted
    if (is_image_sequence_ && !SystemToolkit::create_directory(output_filename_)) {
        setError("Failed to create output folder " + output_filename_);
        Log::Warning("Transcoder: Failed to create output folder %s", output_filename_.c_str());
        g_free(src_uri);
        return false;
    }

    // Upscaling: Real-ESRGAN inference cannot live inside a gstreamer
    // pipeline, so decoding and encoding become two pipelines joined by the
    // worker thread of runUpscale(). Only the decoding side can be described
    // here; the encoding side needs the actual frame size, which is known
    // only once the first frame has been decoded. Audio cannot cross that
    // appsink / appsrc boundary, so the output is video only.
    if (upscale_factor_ > 1) {
        decode_desc_ = "uridecodebin uri=\"" + std::string(src_uri) + "\" name=dec dec. ! queue ! ";
        if (source_interlaced)
            decode_desc_ += "deinterlace method=2 ! ";

        // Offering RGBA ahead of RGB keeps the transparency of a source which
        // has one: gstreamer settles on RGBA only when the decoder actually
        // produces an alpha channel and falls back to RGB otherwise, so an
        // opaque source never pays for a fourth channel. Only worth asking for
        // when the output can store it -- the video encoders and JPEG cannot,
        // and the alpha would be dropped further down anyway.
        const bool keep_alpha = is_still_image_ && GstToolkit::imageSupportsAlpha(options.format());
        decode_desc_ += keep_alpha
            ? "videoconvert ! video/x-raw,format=(string){ RGBA, RGB } ! appsink name=sink"
            : "videoconvert ! video/x-raw,format=RGB ! appsink name=sink";

        video_encoder_ = video_encoder;
        g_free(src_uri);

        if (is_still_image_)
            Log::Info("Transcoder: Upscaling x%d with model '%s', encoding one %d x %d image",
                      upscale_factor_, upscaler.name, frame_out_width, frame_out_height);
        else {
            if (has_audio)
                Log::Info("Transcoder: Audio track dropped (not supported when upscaling)");
            Log::Info("Transcoder: Upscaling x%d with model '%s', encoding %d x %d frames",
                      upscale_factor_, upscaler.name, frame_out_width, frame_out_height);
        }

        started_ = true;
        worker_ = std::thread(&Transcoder::runUpscale, this, options);
        return true;
    }

    // Build the gstreamer pipeline: uridecodebin feeds a video branch
    // (deinterlace if needed, then the profile's encoder) and, when the
    // source has audio and it isn't force-disabled, an audio branch (codec
    // matching VideoRecorder's convention: opus for VPX/WebM, aac otherwise)
    std::string description = "uridecodebin uri=\"";
    description += src_uri;
    description += "\" name=dec ";
    g_free(src_uri);

    description += "dec. ! queue name=vq ! ";
    if (source_interlaced)
        description += "deinterlace method=2 ! ";
    description += "videoconvert ! ";
    // change framerate and / or size (letterboxed) if requested
    // videorate ensures regular timestamps: sources with missing frames (variable
    // framerate) otherwise lead the parser to drop PTS, and the muxer to fail
    const bool reframe = !is_still_image_ && options.framerate_n > 0 && options.framerate_d > 0;
    if (!is_still_image_)
        description += "videorate name=rate ! ";
    description += "videoscale ! ";
    if (rescale || reframe) {
        description += "video/x-raw";
        if (rescale)
            description += ",width=" + std::to_string(frame_out_width) + ",height=" + std::to_string(frame_out_height)
                         + ",pixel-aspect-ratio=1/1";
        if (reframe)
            description += ",framerate=" + std::to_string(options.framerate_n) + "/" + std::to_string(options.framerate_d);
        description += " ! ";
    }
    description += video_encoder;

    if (is_still_image_) {
        // one image in, one image out: no muxer, no audio. The source decodes
        // to a single buffer followed by EOS, which ends the pipeline.
        description += "filesink name=sink location=\"" + output_filename_ + "\"";
    }
    else if (is_image_sequence_) {
        // numbered JPEG sequence: no muxer/audio, capped by the pad probe below
        std::string pattern = SystemToolkit::full_filename(output_filename_, "%05d.jpg");
        description += "multifilesink name=sink location=\"" + pattern + "\"";
    }
    else {
        const char *muxer = (options.profile() == GstToolkit::VPX_RT) ? "webmmux" : "qtmux";
        description += muxer;
        description += " name=mux ! filesink name=sink location=\"" + output_filename_ + "\" ";

        if (has_audio && !options.force_no_audio) {
            description += "dec. ! queue name=aq ! audioconvert ! audioresample ! ";
            if (options.profile() == GstToolkit::VPX_RT)
                description += "opusenc ! opusparse ! queue ! mux. ";
            else
                description += "avenc_aac ! aacparse ! queue ! mux. ";
            Log::Info("Transcoder: Encoding audio track");
        }
        else if (has_audio)
            Log::Info("Transcoder: Audio removal forced by options");
    }

#ifndef NDEBUG
    Log::Info("Transcoder: pipeline '%s'", description.c_str());
#endif

    GError *error = nullptr;
    pipeline_ = gst_parse_launch(description.c_str(), &error);
    if (error != nullptr) {
        setError(std::string("Could not construct pipeline: ") + error->message);
        Log::Warning("Transcoder: Could not construct pipeline: %s", error->message);
        g_clear_error(&error);
        return false;
    }

    // splitmuxsrc of a sequence of videos plays the list of files
    if (!input_files_.empty()) {
        GstElement *dec = gst_bin_get_by_name(GST_BIN(pipeline_), "dec");
        SplitMediaPlayer::SetupSplitSource(dec, &input_files_);
        if (dec)
            gst_object_unref(dec);
    }

    // Cap the number of JPEG files written
    if (is_image_sequence_) {
        GstElement *sink = gst_bin_get_by_name(GST_BIN(pipeline_), "sink");
        if (sink) {
            GstPad *pad = gst_element_get_static_pad(sink, "sink");
            if (pad) {
                gst_pad_add_probe(pad, GST_PAD_PROBE_TYPE_BUFFER,
                                   limit_jpeg_frames_probe, new guint64(0),
                                   [](gpointer data) { delete static_cast<guint64 *>(data); });
                gst_object_unref(pad);
            }
            gst_object_unref(sink);
        }
    }

    // videorate should not duplicate the last frame (up to 1 second by default) when
    // the stream ends; property available since GStreamer 1.22
    GstElement *rate = gst_bin_get_by_name(GST_BIN(pipeline_), "rate");
    if (rate) {
        if (g_object_class_find_property(G_OBJECT_GET_CLASS(rate), "max-closing-segment-duplication-duration"))
            g_object_set(G_OBJECT(rate), "max-closing-segment-duplication-duration", (guint64) 0, NULL);
        gst_object_unref(rate);
    }

    // Range: hold back the data entering each branch until the seek,
    // and end each branch at the first buffer after the range: a stop position
    // in the seek would clip the duration of the last frame, and the muxer
    // would then not be able to tell the framerate from the frames durations
    range_by_eos_ = true;
    if (hasRange()) {
        for (const char *name : {"vq", "aq"}) {
            GstElement *queue = gst_bin_get_by_name(GST_BIN(pipeline_), name);
            if (queue) {
                GstPad *pad = gst_element_get_static_pad(queue, "sink");
                addRangeProbe(pad);
                gst_object_unref(pad);
                gst_object_unref(queue);
            }
        }
    }

    bus_ = gst_element_get_bus(pipeline_);

    GstStateChangeReturn ret = gst_element_set_state(pipeline_, GST_STATE_PLAYING);
    if (ret == GST_STATE_CHANGE_FAILURE) {
        setError("Failed to start transcoding pipeline");
        Log::Warning("Transcoder: Failed to start transcoding pipeline");
        return false;
    }

    // seek to the exact range
    if (hasRange() && !applyRange(true)) {
        gst_element_set_state(pipeline_, GST_STATE_NULL);
        removeIncompleteOutput();
        return false;
    }

    started_ = true;
    return true;
}

bool Transcoder::hasRange() const
{
    return GST_CLOCK_TIME_IS_VALID(range_begin_) || GST_CLOCK_TIME_IS_VALID(range_end_);
}

struct RangeProbe {
    Transcoder *transcoder;
    bool passing;
    bool ended;
};

void Transcoder::addRangeProbe(GstPad *sinkpad)
{
    if (sinkpad)
        gst_pad_add_probe(sinkpad, (GstPadProbeType) (GST_PAD_PROBE_TYPE_BUFFER | GST_PAD_PROBE_TYPE_EVENT_FLUSH),
                          callback_range_probe, new RangeProbe({this, false, false}),
                          [](gpointer data) { delete static_cast<RangeProbe *>(data); });
}

GstPadProbeReturn Transcoder::callback_range_probe(GstPad *pad, GstPadProbeInfo *info, gpointer user_data)
{
    RangeProbe *probe = static_cast<RangeProbe *>(user_data);

    if (GST_PAD_PROBE_INFO_TYPE(info) & GST_PAD_PROBE_TYPE_BUFFER) {
        Transcoder *t = probe->transcoder;

        if (probe->passing) {
            if (probe->ended)
                return GST_PAD_PROBE_DROP;
            // first buffer after the range: end this branch
            // (upstream then receives EOS when pushing more)
            GstBuffer *buffer = GST_PAD_PROBE_INFO_BUFFER(info);
            if (t->range_by_eos_ && GST_CLOCK_TIME_IS_VALID(t->range_end_) &&
                buffer && GST_BUFFER_PTS_IS_VALID(buffer) && GST_BUFFER_PTS(buffer) >= t->range_end_) {
                probe->ended = true;
                gst_pad_send_event(pad, gst_event_new_eos());
                return GST_PAD_PROBE_DROP;
            }
            return GST_PAD_PROBE_OK;
        }

        // data flows: remember where to send the seek, and drop the buffer
        {
            std::lock_guard<std::mutex> lock(t->range_mutex_);
            if (t->range_pad_ == nullptr)
                t->range_pad_ = gst_pad_get_peer(pad);
        }
        t->range_ready_ = true;
        return GST_PAD_PROBE_DROP;
    }

    // the seek flushed this branch: let the range pass
    GstEvent *event = GST_PAD_PROBE_INFO_EVENT(info);
    if (event && GST_EVENT_TYPE(event) == GST_EVENT_FLUSH_STOP)
        probe->passing = true;

    return GST_PAD_PROBE_OK;
}

bool Transcoder::applyRange(bool accurate)
{
    // wait for data to flow in the pipeline (buffers being dropped)
    for (int t = 0; !range_ready_ && t < 2000; ++t) {
        pollBus();
        if (finished_)
            return false;
        std::this_thread::sleep_for(std::chrono::milliseconds(5));
    }

    GstPad *pad = nullptr;
    {
        std::lock_guard<std::mutex> lock(range_mutex_);
        if (range_pad_)
            pad = GST_PAD(gst_object_ref(range_pad_));
    }
    if (!pad) {
        setError("Cannot read the input file");
        Log::Warning("Transcoder: no data to seek in %s", input_filename_.c_str());
        return false;
    }

    // flushing seek upstream of the branch, to the begin and end of range
    // (accurate when decoding, on keyframe before begin when copying streams)
    GstSeekFlags flags = (GstSeekFlags) (GST_SEEK_FLAG_FLUSH |
                         (accurate ? GST_SEEK_FLAG_ACCURATE : (GST_SEEK_FLAG_KEY_UNIT | GST_SEEK_FLAG_SNAP_BEFORE)));
    // (without stop position when the end of range is done by EOS)
    const bool stop = GST_CLOCK_TIME_IS_VALID(range_end_) && !range_by_eos_;
    GstEvent *seek = gst_event_new_seek(1.0, GST_FORMAT_TIME, flags,
                        GST_SEEK_TYPE_SET, GST_CLOCK_TIME_IS_VALID(range_begin_) ? range_begin_ : 0,
                        stop ? GST_SEEK_TYPE_SET : GST_SEEK_TYPE_NONE,
                        stop ? range_end_ : GST_CLOCK_TIME_NONE);
    const bool done = gst_pad_send_event(pad, seek);
    gst_object_unref(pad);

    if (!done) {
        setError("Cannot seek in the input file");
        Log::Warning("Transcoder: failed to seek in %s", input_filename_.c_str());
    }
    return done;
}

bool Transcoder::startCopy(const std::string &src_filename, bool has_audio)
{
    Log::Info("Transcoder: Copying streams of '%s' to '%s'", src_filename.c_str(), output_filename_.c_str());

    // pipeline: parsebin gives the (parsed) compressed streams, linked to the
    // muxer when they appear. Matroska accepts virtually any codec.
    std::string description = "filesrc location=\"" + src_filename + "\" ! parsebin name=dec "
                              "matroskamux name=mux ! filesink name=sink location=\"" + output_filename_ + "\"";

    GError *error = nullptr;
    pipeline_ = gst_parse_launch(description.c_str(), &error);
    if (error != nullptr) {
        setError(std::string("Could not construct pipeline: ") + error->message);
        Log::Warning("Transcoder: Could not construct pipeline: %s", error->message);
        g_clear_error(&error);
        return false;
    }

    copy_audio_ = has_audio;
    range_by_eos_ = false;   // copy: no frame duration clipped, stop at end of range
    copy_mux_ = gst_bin_get_by_name(GST_BIN(pipeline_), "mux");
    GstElement *dec = gst_bin_get_by_name(GST_BIN(pipeline_), "dec");
    g_signal_connect(dec, "pad-added", G_CALLBACK(callback_copy_pad_added), this);
    gst_object_unref(dec);

    bus_ = gst_element_get_bus(pipeline_);

    GstStateChangeReturn ret = gst_element_set_state(pipeline_, GST_STATE_PLAYING);
    if (ret == GST_STATE_CHANGE_FAILURE) {
        setError("Failed to start copy pipeline");
        Log::Warning("Transcoder: Failed to start copy pipeline");
        return false;
    }

    // seek to keyframe before range
    if (hasRange() && !applyRange(false)) {
        gst_element_set_state(pipeline_, GST_STATE_NULL);
        removeIncompleteOutput();
        return false;
    }

    started_ = true;
    return true;
}

GstPadProbeReturn Transcoder::callback_copy_check_pts(GstPad *, GstPadProbeInfo *info, gpointer user_data)
{
    GstBuffer *buffer = GST_PAD_PROBE_INFO_BUFFER(info);
    if (buffer && GST_BUFFER_PTS_IS_VALID(buffer))
        return GST_PAD_PROBE_OK;

    // stream without presentation timestamps (e.g. MPEG-4 ASP in AVI): the
    // copy would be broken, fail with an error (once) and drop the buffer
    Transcoder *t = static_cast<Transcoder *>(user_data);
    if (!t->copy_failed_.exchange(true)) {
        GError *error = g_error_new_literal(GST_STREAM_ERROR, GST_STREAM_ERROR_FORMAT,
                        "Cannot copy a video without timestamps; encode it instead");
        gst_element_post_message(t->pipeline_,
                                 gst_message_new_error(GST_OBJECT(t->pipeline_), error, nullptr));
        g_error_free(error);
    }
    return GST_PAD_PROBE_DROP;
}

void Transcoder::callback_copy_pad_added(GstElement *, GstPad *pad, gpointer user_data)
{
    Transcoder *t = static_cast<Transcoder *>(user_data);

    // kind of stream
    GstCaps *caps = gst_pad_get_current_caps(pad);
    if (!caps)
        caps = gst_pad_query_caps(pad, nullptr);
    const gchar *name = gst_structure_get_name(gst_caps_get_structure(caps, 0));
    const bool video = g_str_has_prefix(name, "video/") || g_str_has_prefix(name, "image/");
    const bool audio = g_str_has_prefix(name, "audio/");
    gst_caps_unref(caps);

    // ignore other streams (e.g. subtitles), and audio if not wanted
    if (!video && !(audio && t->copy_audio_))
        return;

    // branch: queue -> muxer
    GstElement *queue = gst_element_factory_make("queue", nullptr);
    gst_bin_add(GST_BIN(t->pipeline_), queue);
    gst_element_sync_state_with_parent(queue);

    GstPad *sinkpad = gst_element_get_static_pad(queue, "sink");
    if (t->hasRange())
        t->addRangeProbe(sinkpad);
    gst_pad_link(pad, sinkpad);
    gst_object_unref(sinkpad);

    GstPad *muxpad = gst_element_request_pad_simple(t->copy_mux_, video ? "video_%u" : "audio_%u");
    GstPad *srcpad = gst_element_get_static_pad(queue, "src");
    if (muxpad == nullptr || gst_pad_link(srcpad, muxpad) != GST_PAD_LINK_OK)
        Log::Warning("Transcoder: cannot copy stream %s", name);
    // the muxer needs timestamps, which some containers do not give without decoding
    if (video)
        gst_pad_add_probe(srcpad, GST_PAD_PROBE_TYPE_BUFFER, callback_copy_check_pts, t, nullptr);
    gst_object_unref(srcpad);
    if (muxpad)
        gst_object_unref(muxpad);
}

void Transcoder::pollBus()
{
    if (!bus_ || finished_)
        return;

    GstMessage *msg;
    while ((msg = gst_bus_pop_filtered(bus_, (GstMessageType)(GST_MESSAGE_EOS | GST_MESSAGE_ERROR))) != nullptr) {

        if (GST_MESSAGE_TYPE(msg) == GST_MESSAGE_ERROR) {
            GError *err = nullptr;
            gst_message_parse_error(msg, &err, nullptr);
            const std::string what = std::string("Transcoding error: ") + (err ? err->message : "unknown");
            setError(what);
            Log::Warning("Transcoder: %s", what.c_str());
            if (err)
                g_error_free(err);
            finished_ = true;
            success_ = false;
        }
        else { // GST_MESSAGE_EOS
            finished_ = true;
            success_ = true;
            Log::Info("Transcoder: transcoding of '%s' completed", output_filename_.c_str());
        }

        gst_message_unref(msg);

        if (finished_) {
            gst_element_set_state(pipeline_, GST_STATE_NULL);
            // do not leave a broken file behind
            if (!success_)
                removeIncompleteOutput();
            break;
        }
    }
}

void Transcoder::removeIncompleteOutput()
{
    if (output_filename_.empty())
        return;

    if (is_image_sequence_) {
        std::error_code ec;
        std::filesystem::remove_all(output_filename_, ec);
        if (!ec)
            Log::Info("Transcoder: Removed incomplete output folder: %s", output_filename_.c_str());
    }
    else {
        struct stat buffer;
        if (stat(output_filename_.c_str(), &buffer) == 0) {
            if (remove(output_filename_.c_str()) == 0) {
                Log::Info("Transcoder: Removed incomplete output file: %s", output_filename_.c_str());
            } else {
                Log::Warning("Transcoder: Failed to remove incomplete output file: %s", output_filename_.c_str());
            }
        }
    }
}

void Transcoder::stop()
{
    // Only stop if transcoding is in progress
    if (!started_ || finished_)
        return;

    if (upscale_factor_ > 1) {
        abort_ = true;
        setStatus("Cancelling...");
        return;
    }

    if (pipeline_)
        gst_element_set_state(pipeline_, GST_STATE_NULL);

    finished_ = true;
    success_ = false;
    setError("Cancelled by user");
    setStatus("");

    removeIncompleteOutput();
}

bool Transcoder::finished()
{
    if (started_ && !finished_)
        pollBus();
    return finished_;
}

bool Transcoder::success()
{
    finished();
    return success_ && finished_;
}

double Transcoder::progress()
{
    if (!started_)
        return 0.0;

    pollBus();
    if (finished_)
        return 1.0;

    // Upscaling: there is no pipeline to query, the worker publishes the
    // timestamp of the last frame it decoded instead
    if (upscale_factor_ > 1) {
        const gint64 dur = duration_, pos = position_;
        if (dur > 0 && pos >= 0)
            return std::min(1.0, static_cast<double>(pos) / static_cast<double>(dur));
        return 0.0;
    }

    if (!pipeline_)
        return 0.0;

    gint64 pos = 0, dur = 0;
    if (gst_element_query_position(pipeline_, GST_FORMAT_TIME, &pos) &&
        gst_element_query_duration(pipeline_, GST_FORMAT_TIME, &dur) &&
        dur > 0 && pos >= 0) {
        // progress within the range
        const gint64 b = GST_CLOCK_TIME_IS_VALID(range_begin_) ? (gint64) range_begin_ : 0;
        const gint64 e = GST_CLOCK_TIME_IS_VALID(range_end_) ? std::min((gint64) range_end_, dur) : dur;
        if (e > b)
            return std::clamp(static_cast<double>(pos - b) / static_cast<double>(e - b), 0.0, 1.0);
    }

    return 0.0;
}

// Background upscaling: decode -> Real-ESRGAN -> encode.
//
// The source is decoded to packed RGB by one pipeline ending in an appsink,
// every frame is enlarged on the GPU, and the result is pushed into a second
// pipeline starting with an appsrc and ending in the profile's encoder. Both
// ends are bounded (appsink max-buffers, appsrc max-bytes with block=true)
// so that the fastest stage waits for the slowest instead of accumulating
// frames in memory, the inference being orders of magnitude slower than
// either decoding or encoding.
void Transcoder::runUpscale(TranscoderOptions options)
{
    GstElement *dec_pipe = nullptr, *enc_pipe = nullptr;
    GstElement *sink = nullptr, *src = nullptr;
    GstSample *sample = nullptr;
    int frames = 0;

    // the watchdog below is armed before any pipeline exists and disarmed
    // on every exit path, hence the flags declared here.
    //
    // A single image never arms it (budget 0): the watchdog guards against an
    // encoder blocked forever on a push under backpressure, which cannot
    // happen with one frame and no muxer, while its budget can only ever be
    // the coarse initial one -- a heavy model on a large photo legitimately
    // runs longer than that and would be aborted for no reason.
    std::atomic<long long> last_progress{ now_ms() };
    std::atomic<long long> budget_ms{ is_still_image_ ? 0 : STALL_INITIAL_TIMEOUT_MS };
    std::atomic<bool> stalled{ false };
    std::atomic<bool> watchdog_stop{ false };

    // A frame which takes far longer than the ones before it means the
    // encoder is stuck: tear both pipelines down to unblock the push and
    // abort. Without this the worker would block forever and stop() with it.
    std::thread watchdog([&] {
        while (!watchdog_stop.load()) {
            std::this_thread::sleep_for(std::chrono::milliseconds(250));
            const long long budget = budget_ms.load();
            if (budget > 0 && now_ms() - last_progress.load() > budget) {
                stalled = true;
                abort_ = true;
                if (enc_pipe) gst_element_set_state(enc_pipe, GST_STATE_NULL);
                if (dec_pipe) gst_element_set_state(dec_pipe, GST_STATE_NULL);
                break;
            }
        }
    });
    struct WatchdogGuard {
        std::atomic<bool> &stop;
        std::thread &thread;
        ~WatchdogGuard() { stop = true; if (thread.joinable()) thread.join(); }
    } watchdog_guard{ watchdog_stop, watchdog };

    const std::string stall_message =
        "Encoding stalled (no progress for too long): the encoder most likely cannot "
        "handle this resolution on this machine. Try a smaller upscaling factor or "
        "another codec.";

    try {
        // Loading the model downloads its files on first use and initializes
        // Vulkan: seconds during which no frame is produced yet
        setStatus("Preparing model...");
        Upscaler::Engine upscaler( Upscaler::model(options.upscaler) );
        Log::Info("Transcoder: upscaling with %s", upscaler.describe().c_str());
        // A single image has no duration, so progress() cannot move: the
        // status line is the only feedback during what can be minutes of
        // inference with one of the heavy models.
        setStatus(is_still_image_ ? "Upscaling image..." : "");

        if (abort_)
            throw std::runtime_error("Cancelled by user");

        // -------- decoding side: source -> packed RGB frames
        GError *err = nullptr;
#ifndef NDEBUG
        Log::Info("Transcoder: decoding pipeline '%s'", decode_desc_.c_str());
#endif
        dec_pipe = gst_parse_launch(decode_desc_.c_str(), &err);
        if (!dec_pipe) {
            const std::string what = err ? err->message : "unknown";
            if (err) g_error_free(err);
            throw std::runtime_error("Could not construct decoding pipeline: " + what);
        }
        sink = gst_bin_get_by_name(GST_BIN(dec_pipe), "sink");

        // splitmuxsrc of a sequence of videos plays the list of files
        if (!input_files_.empty()) {
            GstElement *dec = gst_bin_get_by_name(GST_BIN(dec_pipe), "dec");
            SplitMediaPlayer::SetupSplitSource(dec, &input_files_);
            if (dec)
                gst_object_unref(dec);
        }

        // bounded queue, and never drop: every frame must be encoded
        g_object_set(sink, "sync", FALSE, "drop", FALSE,
                     "max-buffers", UPSCALE_QUEUE_LENGTH, nullptr);
        gst_element_set_state(dec_pipe, GST_STATE_PLAYING);

        // the first frame gives the actual geometry and framerate
        sample = gst_app_sink_pull_sample(GST_APP_SINK(sink));
        if (!sample)
            throw std::runtime_error("No video stream in " + input_filename_);

        GstVideoInfo in_info;
        gst_video_info_from_caps(&in_info, gst_sample_get_caps(sample));
        const int w = GST_VIDEO_INFO_WIDTH(&in_info);
        const int h = GST_VIDEO_INFO_HEIGHT(&in_info);
        int fps_n = GST_VIDEO_INFO_FPS_N(&in_info);
        int fps_d = GST_VIDEO_INFO_FPS_D(&in_info);
        if (fps_n <= 0) { fps_n = 30; fps_d = 1; }
        const int ow = w * upscale_factor_;
        const int oh = h * upscale_factor_;

        // Whichever of RGBA / RGB the decoder settled on: RGBA only happens
        // when the source carries transparency and the output format can
        // store it, and the whole path below follows that choice.
        const GstVideoFormat pixel_format = GST_VIDEO_INFO_FORMAT(&in_info);
        const int channels = (pixel_format == GST_VIDEO_FORMAT_RGBA) ? 4 : 3;

        gint64 dur = 0;
        if (gst_element_query_duration(dec_pipe, GST_FORMAT_TIME, &dur))
            duration_ = dur;

        // -------- encoding side: upscaled frames -> the chosen format
        // The video encoders need even dimensions, which an odd source size
        // times an odd factor does not give: videoscale trims that last
        // pixel. The still formats have no such constraint, so an image is
        // written at its full upscaled size.
        const int enc_w = is_still_image_ ? ow : (ow & ~1);
        const int enc_h = is_still_image_ ? oh : (oh & ~1);
        std::string desc = "appsrc name=src format=time ! queue ! videoconvert ! videoscale ! "
                           "video/x-raw,width=" + std::to_string(enc_w) +
                           ",height=" + std::to_string(enc_h) + " ! ";
        desc += video_encoder_;
        if (is_still_image_)
            desc += "filesink name=encsink location=\"" + output_filename_ + "\"";
        else if (is_image_sequence_)
            desc += "multifilesink name=encsink location=\"" +
                    SystemToolkit::full_filename(output_filename_, "%05d.jpg") + "\"";
        else
            desc += std::string((options.profile() == GstToolkit::VPX_RT) ? "webmmux" : "qtmux") +
                    " name=mux ! filesink name=encsink location=\"" + output_filename_ + "\"";
#ifndef NDEBUG
        Log::Info("Transcoder: encoding pipeline '%s'", desc.c_str());
#endif
        enc_pipe = gst_parse_launch(desc.c_str(), &err);
        if (!enc_pipe) {
            const std::string what = err ? err->message : "unknown";
            if (err) g_error_free(err);
            throw std::runtime_error("Could not construct encoding pipeline: " + what);
        }
        src = gst_bin_get_by_name(GST_BIN(enc_pipe), "src");

        GstVideoInfo out_info;
        gst_video_info_set_format(&out_info, pixel_format, ow, oh);
        out_info.fps_n = fps_n;
        out_info.fps_d = fps_d;
        GstCaps *out_caps = gst_video_info_to_caps(&out_info);
        g_object_set(src, "caps", out_caps, nullptr);
        gst_caps_unref(out_caps);
        // block=true with a budget of a few frames applies backpressure, so
        // pushed frames cannot pile up if the encoder lags behind
        g_object_set(src, "block", TRUE,
                     "max-bytes", (guint64) out_info.size * UPSCALE_QUEUE_LENGTH, nullptr);
        gst_element_set_state(enc_pipe, GST_STATE_PLAYING);

        // -------- frame loop
        std::vector<unsigned char> in_pixels((size_t) w * h * channels);
        std::vector<unsigned char> out_pixels((size_t) ow * oh * channels);

        while (sample && !abort_) {

            // decoded frame -> tightly packed pixels (rows may be padded)
            GstVideoInfo sample_info;
            gst_video_info_from_caps(&sample_info, gst_sample_get_caps(sample));
            GstVideoFrame vframe;
            gst_video_frame_map(&vframe, &sample_info, gst_sample_get_buffer(sample), GST_MAP_READ);
            const guint8 *pixels = (const guint8 *) GST_VIDEO_FRAME_PLANE_DATA(&vframe, 0);
            const int stride = GST_VIDEO_FRAME_PLANE_STRIDE(&vframe, 0);
            for (int y = 0; y < h; y++)
                memcpy(&in_pixels[(size_t) y * w * channels], pixels + (size_t) y * stride,
                       (size_t) w * channels);
            const GstClockTime pts = GST_BUFFER_PTS(gst_sample_get_buffer(sample));
            gst_video_frame_unmap(&vframe);
            gst_sample_unref(sample);
            sample = nullptr;

            // the expensive part: Real-ESRGAN on the GPU
            upscaler.process(in_pixels.data(), w, h, out_pixels.data(), channels);

            // push the upscaled frame (restoring the encoder's row alignment)
            GstBuffer *buf = gst_buffer_new_allocate(nullptr, out_info.size, nullptr);
            gst_buffer_add_video_meta(buf, GST_VIDEO_FRAME_FLAG_NONE, pixel_format, ow, oh);
            GstMapInfo map;
            gst_buffer_map(buf, &map, GST_MAP_WRITE);
            const int ostride = GST_VIDEO_INFO_PLANE_STRIDE(&out_info, 0);
            for (int y = 0; y < oh; y++)
                memcpy(map.data + (size_t) y * ostride, &out_pixels[(size_t) y * ow * channels],
                       (size_t) ow * channels);
            gst_buffer_unmap(buf, &map);

            GST_BUFFER_PTS(buf) = gst_util_uint64_scale(frames, GST_SECOND * fps_d, fps_n);
            GST_BUFFER_DURATION(buf) = gst_util_uint64_scale(1, GST_SECOND * fps_d, fps_n);
            frames++;
            if (gst_app_src_push_buffer(GST_APP_SRC(src), buf) != GST_FLOW_OK)
                throw std::runtime_error(stalled ? stall_message
                                                 : "Failed to push frame to the encoder");

            // the frame completed: record progress and, from the first
            // couple of frames, arm the watchdog on that baseline
            const long long tnow = now_ms();
            const long long frame_ms = tnow - last_progress.load();
            last_progress = tnow;
            if (frames <= 2)
                budget_ms = std::max<long long>(STALL_TIMEOUT_FACTOR * frame_ms, STALL_MIN_TIMEOUT_MS);

            if (GST_CLOCK_TIME_IS_VALID(pts))
                position_ = pts;

            // a still format holds exactly one frame: stop here rather than
            // relying on the decoder to report EOS after the first buffer
            if (is_still_image_)
                break;

            // same safety cap as the pad probe of the non-upscaled path
            if (is_image_sequence_ && frames >= MAX_JPEG_FRAMES) {
                Log::Warning("Transcoder: reached maximum of %d images; stopping.", MAX_JPEG_FRAMES);
                break;
            }

            sample = gst_app_sink_pull_sample(GST_APP_SINK(sink));
        }
        if (sample) {
            gst_sample_unref(sample);
            sample = nullptr;
        }

        if (abort_)
            throw std::runtime_error(stalled ? stall_message : "Cancelled by user");

        // -------- finalize: end of stream, and let the muxer write its index
        gst_app_src_end_of_stream(GST_APP_SRC(src));
        GstBus *bus = gst_element_get_bus(enc_pipe);
        GstMessage *msg = gst_bus_timed_pop_filtered(bus, GST_CLOCK_TIME_NONE,
                              (GstMessageType)(GST_MESSAGE_EOS | GST_MESSAGE_ERROR));
        const bool eos = msg && GST_MESSAGE_TYPE(msg) == GST_MESSAGE_EOS;
        if (!eos) {
            GError *e = nullptr;
            if (msg)
                gst_message_parse_error(msg, &e, nullptr);
            const std::string what = e ? e->message : "unknown";
            if (e) g_error_free(e);
            if (msg) gst_message_unref(msg);
            gst_object_unref(bus);
            throw std::runtime_error("Transcoder error: " + what);
        }
        gst_message_unref(msg);
        gst_object_unref(bus);

        Log::Info("Transcoder: transcoding of '%s' completed (%d frames upscaled)",
                  output_filename_.c_str(), frames);
        success_ = true;
    }
    catch (const std::exception &e) {
        setError(e.what());
        setStatus("");
        Log::Warning("Transcoder: %s", e.what());
        success_ = false;
        if (sample)
            gst_sample_unref(sample);
    }

    // The watchdog holds a reference to both pipelines: it has to be stopped
    // and joined before they are freed below. The guard above only covers an
    // exception escaping this function altogether.
    watchdog_stop = true;
    if (watchdog.joinable())
        watchdog.join();

    if (dec_pipe) {
        gst_element_set_state(dec_pipe, GST_STATE_NULL);
        if (sink) gst_object_unref(sink);
        gst_object_unref(dec_pipe);
    }
    if (enc_pipe) {
        gst_element_set_state(enc_pipe, GST_STATE_NULL);
        if (src) gst_object_unref(src);
        gst_object_unref(enc_pipe);
    }

    // stop() does not wait for this thread, so clearing up after a failed or
    // cancelled run is ours to do, once the pipelines have released the file
    if (!success_)
        removeIncompleteOutput();

    setStatus("");

    // published last: the user interface deletes this Transcoder as soon as
    // it sees finished_, and that destructor joins this very thread
    finished_ = true;
}

//
// SequenceTranscoder
//

SequenceTranscoder::SequenceTranscoder(const std::list<std::string>& input_files)
    : progress_(0.0)
    , started_(false)
    , abort_(false)
    , finished_(false)
    , success_(false)
{
    for (const auto &f : input_files)
        input_.push_back({f, GST_CLOCK_TIME_NONE, GST_CLOCK_TIME_NONE});
}

SequenceTranscoder::SequenceTranscoder(const std::string& input_file,
                                       const std::list< std::pair<GstClockTime, GstClockTime> >& ranges)
    : progress_(0.0)
    , started_(false)
    , abort_(false)
    , finished_(false)
    , success_(false)
{
    for (const auto &r : ranges)
        input_.push_back({input_file, r.first, r.second});
}

SequenceTranscoder::~SequenceTranscoder()
{
    stop();
    if (worker_.joinable())
        worker_.join();
}

bool SequenceTranscoder::start(const TranscoderOptions& options, const std::string &output_folder)
{
    if (started_ || input_.empty() || (!options.isImage() && options.profile() == GstToolkit::JPEG_MULTI)) {
        std::lock_guard<std::mutex> lock(mutex_);
        error_message_ = "Invalid sequence or options";
        return false;
    }

    // Name of the sequence: for images, the first file without trailing numbers;
    // for videos, the part common to the names of all files (e.g. '20260611_1'
    // for 20260611_151758.mp4 and 20260611_192634.mp4)
    const std::string &first = input_.front().file;
    std::string name;
    if (options.isImage()) {
        name = SystemToolkit::base_filename(first);
        while (!name.empty() && (isdigit(name.back()) || name.back() == '_' || name.back() == '-' || name.back() == '.'))
            name.pop_back();
    }
    else {
        std::list<std::string> names;
        for (const auto &item : input_)
            names.push_back( SystemToolkit::base_filename(item.file) );
        name = BaseToolkit::common_prefix(names);
        // no separator left at the end
        while (!name.empty() && (name.back() == '_' || name.back() == '-' || name.back() == '.' || name.back() == ' '))
            name.pop_back();
    }
    if (name.empty())
        name = "sequence";

    // Given folder, or new folder next to the input files, named after
    // them and the options, e.g. 'frames_webp', 'frames_png_upscaled_x4' or 'clip_split'
    if (!output_folder.empty())
        output_folder_ = output_folder;
    else {
        if (options.isImage()) {
            name += std::string("_") + GstToolkit::imageFileExtension(options.format());
            const int factor = Upscaler::model(options.upscaler).factor;
            if (Upscaler::available() && factor > 1)
                name += "_upscaled_x" + std::to_string(factor);
        }
        else
            name += "_split";

        const std::string base = SystemToolkit::path_filename(first) + name;
        output_folder_ = base;
        for (int counter = 1; SystemToolkit::file_exists(output_folder_); ++counter)
            output_folder_ = base + "_" + std::to_string(counter);
    }

    if (!SystemToolkit::file_exists(output_folder_) && !SystemToolkit::create_directory(output_folder_)) {
        std::lock_guard<std::mutex> lock(mutex_);
        error_message_ = "Cannot create folder " + output_folder_;
        return false;
    }

    Log::Info("Transcoder: Starting transcoding of %d %s into '%s' (%s)",
              (int) input_.size(), options.isImage() ? "images" : "videos", output_folder_.c_str(),
              options.isImage() ? GstToolkit::image_name[options.format()]
                                : options.stream_copy ? "copy" : GstToolkit::profile_name[options.profile()]);

    started_ = true;
    worker_ = std::thread(&SequenceTranscoder::run, this, options);
    return true;
}

void SequenceTranscoder::run(TranscoderOptions options)
{
    // extension of the new format (same container conventions as Transcoder)
    const bool image = options.isImage();
    const std::string extension = image ? GstToolkit::imageFileExtension(options.format())
                                        : options.stream_copy ? "mkv"
                                        : options.profile() == GstToolkit::VPX_RT ? "webm" : "mov";
    // video files are numbered after the name of the folder
    std::string name = SystemToolkit::base_filename(output_folder_);
    if (name.size() > 6 && name.compare(name.size() - 6, 6, "_split") == 0)
        name.resize(name.size() - 6);
    const char *label = image ? " Image " : " Video ";
    const size_t total = input_.size();
    size_t count = 0;
    std::string error;

    for (const auto &item : input_) {
        const std::string &input = item.file;

        // images keep their name, with extension of the new format;
        // videos are numbered consecutively
        std::string output = output_folder_ + "/";
        if (image)
            output += SystemToolkit::base_filename(input) + "." + extension;
        else {
            char number[16];
            snprintf(number, 16, "_%05d.", (int) count + 1);
            output += name + number + extension;
        }

        // range of the video
        options.begin = item.begin;
        options.end = item.end;

        // transcode this item, and wait for it to finish
        Transcoder transcoder(input, output);
        if (!transcoder.start(options))
            error = transcoder.error();
        else {
            while (!transcoder.finished()) {
                if (abort_) {
                    transcoder.stop();
                    break;
                }
                progress_ = (static_cast<double>(count) + transcoder.progress()) / static_cast<double>(total);
                const std::string status = transcoder.status();
                {
                    std::lock_guard<std::mutex> lock(mutex_);
                    status_message_ = label + std::to_string(count + 1) + " / " + std::to_string(total) +
                                      (status.empty() ? "" : " -" + status);
                }
                std::this_thread::sleep_for(std::chrono::milliseconds(20));
            }
            if (!abort_ && !transcoder.success())
                error = transcoder.error();
        }

        if (abort_ || !error.empty())
            break;

        std::lock_guard<std::mutex> lock(mutex_);
        output_files_.push_back(output);
        progress_ = static_cast<double>(++count) / static_cast<double>(total);
    }

    // incomplete: remove the files produced (and the folder if left empty)
    if (abort_ || !error.empty()) {
        std::error_code ec;
        std::lock_guard<std::mutex> lock(mutex_);
        for (const auto &f : output_files_)
            std::filesystem::remove(f, ec);
        std::filesystem::remove(output_folder_, ec);
        error_message_ = abort_ ? "Cancelled" : error;
        output_files_.clear();
    }
    else
        Log::Info("Transcoder: transcoding of %d %s into '%s' completed", (int) total,
                  image ? "images" : "videos", output_folder_.c_str());

    success_ = !abort_ && error.empty();
    finished_ = true;
}

void SequenceTranscoder::stop()
{
    if (started_ && !finished_)
        abort_ = true;
}

double SequenceTranscoder::progress() const
{
    return finished_ ? 1.0 : progress_.load();
}

std::string SequenceTranscoder::error() const
{
    std::lock_guard<std::mutex> lock(mutex_);
    return error_message_;
}

std::string SequenceTranscoder::status() const
{
    std::lock_guard<std::mutex> lock(mutex_);
    return status_message_;
}

std::list<std::string> SequenceTranscoder::outputFiles() const
{
    std::lock_guard<std::mutex> lock(mutex_);
    return output_files_;
}
