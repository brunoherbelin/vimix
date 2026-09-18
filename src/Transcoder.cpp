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
#include "Toolkit/SystemToolkit.h"

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
// "gop-size=" (nvh264enc/nvh265enc/openh264enc), or "keyframe-max-dist="
// (vp9enc) -- and replace its value with `interval`. A no-op when none of
// those are present, which is correct for ProRes/JPEG: both are all-intra
// already, so there is nothing to tighten.
std::string apply_keyframe_interval(const std::string &pipeline, int interval)
{
    static const char *properties[] = { "key-int-max=", "gop-size=", "keyframe-max-dist=" };

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

Transcoder::Transcoder(const std::string& input_filename)
    : input_filename_(input_filename)
    , pipeline_(nullptr)
    , bus_(nullptr)
    , is_image_sequence_(false)
    , started_(false)
    , finished_(false)
    , success_(false)
    , abort_(false)
    , duration_(-1)
    , position_(0)
    , upscale_factor_(1)
{
    // Output filename will be generated in start() based on options
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
    if (options.profile == GstToolkit::JPEG_MULTI) {
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
    if (options.force_keyframes)
        suffix += "_bidir";
    if (options.force_no_audio)
        suffix += "_noaudio";

    // WebM container for VPX (vp9enc), QuickTime container otherwise,
    // matching VideoRecorder's convention (Recorder.cpp)
    const char *extension = (options.profile == GstToolkit::VPX_RT) ? "webm" : "mov";

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

    is_image_sequence_ = (options.profile == GstToolkit::JPEG_MULTI);

    // Resolve the upscaling model: an unknown name, or a build without the
    // ncnn Vulkan backend, simply means no upscaling
    const UpscalerModel &upscaler = Upscaler::model(options.upscaler);
    upscale_factor_ = (Upscaler::available() && upscaler.factor > 1) ? upscaler.factor : 1;

    // Generate output filename (or folder, for JPEG_MULTI) based on options
    output_filename_ = generateOutputFilename(input_filename_, options);

    // Check if input file exists
    struct stat buffer;
    if (stat(input_filename_.c_str(), &buffer) != 0) {
        setError("Input file does not exist: " + input_filename_);
        Log::Warning("Transcoder: Input file does not exist: %s", input_filename_.c_str());
        return false;
    }

    Log::Info("Transcoder: Starting transcoding from '%s' to '%s' (%s)",
              input_filename_.c_str(), output_filename_.c_str(),
              GstToolkit::profile_name[options.profile]);

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
    guint frame_width = 0;
    guint frame_height = 0;

    if (disc_info) {
        GList *video_streams = gst_discoverer_info_get_video_streams(disc_info);
        if (video_streams) {
            GstDiscovererVideoInfo *vinfo = (GstDiscovererVideoInfo*)video_streams->data;
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

    // Upscaling multiplies the frame size: everything downstream (the
    // resolution check, the keyframe interval, the encoder caps) works on
    // the size of the frames actually handed to the encoder
    const int frame_out_width = (int) frame_width * upscale_factor_;
    const int frame_out_height = (int) frame_height * upscale_factor_;

    // verify the profile can encode the resolution of the produced video
    const std::string unsupported =
        GstToolkit::unsupportedResolution(options.profile, frame_out_width, frame_out_height,
                                          Settings::application.render.gpu_decoding);
    if (!unsupported.empty()) {
        setError(unsupported);
        Log::Warning("Transcoder: %s", unsupported.c_str());
        g_free(src_uri);
        return false;
    }

    // Pick the encoding pipeline fragment for the chosen profile: hardware
    // accelerated if available and enabled, software otherwise
    std::string video_encoder = GstToolkit::getHardwareEncodingPipeline(options.profile);
    bool hardware = Settings::application.render.gpu_decoding && !video_encoder.empty();
    if (!hardware)
        video_encoder = GstToolkit::getEncodingPipeline(options.profile);

    if (video_encoder.empty()) {
        setError(std::string("No encoder available for profile ") + GstToolkit::profile_name[options.profile]);
        Log::Warning("Transcoder: No encoder available for profile %s", GstToolkit::profile_name[options.profile]);
        g_free(src_uri);
        return false;
    }

    // Tighten the keyframe interval for smoother backward playback (no-op
    // for ProRes/JPEG, which are already all-intra)
    if (options.force_keyframes) {
        int interval = GstToolkit::getPlayBackwardGop(options.profile, frame_out_width, frame_out_height);
        std::string patched = apply_keyframe_interval(video_encoder, interval);
        if (patched != video_encoder)
            Log::Info("Transcoder: keyframe interval set to %d frames for backward playback", interval);
        video_encoder = patched;
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
        decode_desc_ += "videoconvert ! video/x-raw,format=RGB ! appsink name=sink";
        video_encoder_ = video_encoder;
        g_free(src_uri);

        if (has_audio)
            Log::Info("Transcoder: Audio track dropped (not supported when upscaling)");
        Log::Info("Transcoder: Upscaling x%d with model '%s', encoding %d x %d frames",
                  upscale_factor_, upscaler.name, frame_out_width, frame_out_height);

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

    description += "dec. ! queue ! ";
    if (source_interlaced)
        description += "deinterlace method=2 ! ";
    description += "videoconvert ! videoscale ! ";
    description += video_encoder;

    if (is_image_sequence_) {
        // numbered JPEG sequence: no muxer/audio, capped by the pad probe below
        std::string pattern = SystemToolkit::full_filename(output_filename_, "%05d.jpg");
        description += "multifilesink name=sink location=\"" + pattern + "\"";
    }
    else {
        const char *muxer = (options.profile == GstToolkit::VPX_RT) ? "webmmux" : "qtmux";
        description += muxer;
        description += " name=mux ! filesink name=sink location=\"" + output_filename_ + "\" ";

        if (has_audio && !options.force_no_audio) {
            description += "dec. ! queue ! audioconvert ! audioresample ! ";
            if (options.profile == GstToolkit::VPX_RT)
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

    bus_ = gst_element_get_bus(pipeline_);

    GstStateChangeReturn ret = gst_element_set_state(pipeline_, GST_STATE_PLAYING);
    if (ret == GST_STATE_CHANGE_FAILURE) {
        setError("Failed to start transcoding pipeline");
        Log::Warning("Transcoder: Failed to start transcoding pipeline");
        return false;
    }

    started_ = true;
    return true;
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
            break;
        }
    }
}

void Transcoder::stop()
{
    // Only stop if transcoding is in progress
    if (!started_ || finished_)
        return;

    if (pipeline_)
        gst_element_set_state(pipeline_, GST_STATE_NULL);

    // Upscaling: ask the worker to give up and wait for it. It checks
    // abort_ once per frame, so this blocks for at most the inference time
    // of the frame in flight.
    abort_ = true;
    if (worker_.joinable())
        worker_.join();

    // the worker may have completed just before it noticed the abort: its
    // output is whole, there is nothing to interrupt nor to remove
    if (success_)
        return;

    finished_ = true;
    success_ = false;
    setError("Transcoding stopped by user");
    setStatus("");

    Log::Info("Transcoder: Interrupted transcoding");

    // Remove incomplete output
    if (!output_filename_.empty()) {
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
        return static_cast<double>(pos) / static_cast<double>(dur);
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
    // on every exit path, hence the flags declared here
    std::atomic<long long> last_progress{ now_ms() };
    std::atomic<long long> budget_ms{ STALL_INITIAL_TIMEOUT_MS };
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
        setStatus("Preparing upscaling model...");
        Upscaler::Engine upscaler( Upscaler::model(options.upscaler) );
        Log::Info("Transcoder: upscaling with %s", upscaler.describe().c_str());
        setStatus("");

        if (abort_)
            throw std::runtime_error("Transcoding stopped by user");

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

        gint64 dur = 0;
        if (gst_element_query_duration(dec_pipe, GST_FORMAT_TIME, &dur))
            duration_ = dur;

        // -------- encoding side: upscaled RGB frames -> the chosen profile
        // The encoders need even dimensions, which an odd source size times
        // an odd factor does not give: videoscale trims that last pixel.
        std::string desc = "appsrc name=src format=time ! queue ! videoconvert ! videoscale ! "
                           "video/x-raw,width=" + std::to_string(ow & ~1) +
                           ",height=" + std::to_string(oh & ~1) + " ! ";
        desc += video_encoder_;
        if (is_image_sequence_)
            desc += "multifilesink name=encsink location=\"" +
                    SystemToolkit::full_filename(output_filename_, "%05d.jpg") + "\"";
        else
            desc += std::string((options.profile == GstToolkit::VPX_RT) ? "webmmux" : "qtmux") +
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
        gst_video_info_set_format(&out_info, GST_VIDEO_FORMAT_RGB, ow, oh);
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
        std::vector<unsigned char> in_rgb((size_t) w * h * 3);
        std::vector<unsigned char> out_rgb((size_t) ow * oh * 3);

        while (sample && !abort_) {

            // decoded frame -> tightly packed RGB (rows may be padded)
            GstVideoInfo sample_info;
            gst_video_info_from_caps(&sample_info, gst_sample_get_caps(sample));
            GstVideoFrame vframe;
            gst_video_frame_map(&vframe, &sample_info, gst_sample_get_buffer(sample), GST_MAP_READ);
            const guint8 *pixels = (const guint8 *) GST_VIDEO_FRAME_PLANE_DATA(&vframe, 0);
            const int stride = GST_VIDEO_FRAME_PLANE_STRIDE(&vframe, 0);
            for (int y = 0; y < h; y++)
                memcpy(&in_rgb[(size_t) y * w * 3], pixels + (size_t) y * stride, (size_t) w * 3);
            const GstClockTime pts = GST_BUFFER_PTS(gst_sample_get_buffer(sample));
            gst_video_frame_unmap(&vframe);
            gst_sample_unref(sample);
            sample = nullptr;

            // the expensive part: Real-ESRGAN on the GPU
            upscaler.process(in_rgb.data(), w, h, out_rgb.data());

            // push the upscaled frame (restoring the encoder's row alignment)
            GstBuffer *buf = gst_buffer_new_allocate(nullptr, out_info.size, nullptr);
            gst_buffer_add_video_meta(buf, GST_VIDEO_FRAME_FLAG_NONE, GST_VIDEO_FORMAT_RGB, ow, oh);
            GstMapInfo map;
            gst_buffer_map(buf, &map, GST_MAP_WRITE);
            const int ostride = GST_VIDEO_INFO_PLANE_STRIDE(&out_info, 0);
            for (int y = 0; y < oh; y++)
                memcpy(map.data + (size_t) y * ostride, &out_rgb[(size_t) y * ow * 3], (size_t) ow * 3);
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
            throw std::runtime_error(stalled ? stall_message : "Transcoding stopped by user");

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
            throw std::runtime_error("Encoding error: " + what);
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

    finished_ = true;
}
