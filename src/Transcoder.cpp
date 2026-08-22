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
#include "Toolkit/SystemToolkit.h"

#include <sys/stat.h>
#include <filesystem>
#include <glib.h>
#include <cctype>
#include <cstring>

namespace {

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
        Log::Info("Transcoder: reached maximum of %d images, stopping", Transcoder::MAX_JPEG_FRAMES);
        gst_pad_push_event(pad, gst_event_new_eos());
    }

    return GST_PAD_PROBE_OK;
}

} // namespace

Transcoder::Transcoder(const std::string& input_filename)
    : input_filename_(input_filename)
    , pipeline_(nullptr)
    , bus_(nullptr)
    , started_(false)
    , is_image_sequence_(false)
    , finished_(false)
    , success_(false)
{
    // Output filename will be generated in start() based on options
}

Transcoder::~Transcoder()
{
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

    // Build suffix based on transcoder options
    std::string suffix = "";
    if (options.force_keyframes)
        suffix += "_bidir";
    if (options.force_no_audio)
        suffix += "_noaudio";
    if (suffix.empty())
        suffix = "_transcoded";

    // JPEG_MULTI produces a folder of numbered images, not a single file
    if (options.profile == GstToolkit::JPEG_MULTI) {
        std::string folder = base + suffix;
        std::string output = folder;
        struct stat buffer;
        int counter = 1;
        while (stat(output.c_str(), &buffer) == 0) {
            output = folder + "_" + std::to_string(counter);
            counter++;
        }
        return output;
    }

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
        error_message_ = "Transcoder already started";
        return false;
    }

    is_image_sequence_ = (options.profile == GstToolkit::JPEG_MULTI);

    // Generate output filename (or folder, for JPEG_MULTI) based on options
    output_filename_ = generateOutputFilename(input_filename_, options);

    // Check if input file exists
    struct stat buffer;
    if (stat(input_filename_.c_str(), &buffer) != 0) {
        error_message_ = "Input file does not exist: " + input_filename_;
        Log::Warning("Transcoder: %s", error_message_.c_str());
        return false;
    }

    Log::Info("Transcoder: Starting transcoding from '%s' to '%s' (%s)",
              input_filename_.c_str(), output_filename_.c_str(),
              GstToolkit::profile_name[options.profile]);

    // Discover source to detect interlacing and the presence of an audio
    // stream (no more bitrate matching: profiles are fixed-quality)
    gchar *src_uri = gst_filename_to_uri(input_filename_.c_str(), nullptr);
    if (!src_uri) {
        error_message_ = "Failed to create URI from filename";
        Log::Warning("Transcoder: %s", error_message_.c_str());
        return false;
    }

    GstDiscoverer *discoverer = gst_discoverer_new(10 * GST_SECOND, nullptr);
    if (!discoverer) {
        error_message_ = "Failed to create discoverer";
        Log::Warning("Transcoder: %s", error_message_.c_str());
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

    // Pick the encoding pipeline fragment for the chosen profile: hardware
    // accelerated if available and enabled, software otherwise
    std::string video_encoder = GstToolkit::getHardwareEncodingPipeline(options.profile);
    bool hardware = Settings::application.render.gpu_decoding && !video_encoder.empty();
    if (!hardware)
        video_encoder = GstToolkit::getEncodingPipeline(options.profile);

    if (video_encoder.empty()) {
        error_message_ = std::string("No encoder available for profile ") + GstToolkit::profile_name[options.profile];
        Log::Warning("Transcoder: %s", error_message_.c_str());
        g_free(src_uri);
        return false;
    }

    // Tighten the keyframe interval for smoother backward playback (no-op
    // for ProRes/JPEG, which are already all-intra)
    if (options.force_keyframes) {
        int interval = GstToolkit::getPlayBackwardGop(options.profile, (int) frame_width, (int) frame_height);
        std::string patched = apply_keyframe_interval(video_encoder, interval);
        if (patched != video_encoder)
            Log::Info("Transcoder: keyframe interval set to %d frames for backward playback", interval);
        video_encoder = patched;
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
        if (!SystemToolkit::create_directory(output_filename_)) {
            error_message_ = "Failed to create output folder " + output_filename_;
            Log::Warning("Transcoder: %s", error_message_.c_str());
            return false;
        }
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
        error_message_ = std::string("Could not construct pipeline: ") + error->message;
        Log::Warning("Transcoder: %s", error_message_.c_str());
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
        error_message_ = "Failed to start transcoding pipeline";
        Log::Warning("Transcoder: %s", error_message_.c_str());
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
            error_message_ = std::string("Transcoding error: ") + (err ? err->message : "unknown");
            Log::Warning("Transcoder: %s", error_message_.c_str());
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

    finished_ = true;
    success_ = false;
    error_message_ = "Transcoding stopped by user";

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
