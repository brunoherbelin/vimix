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
#include "SystemToolkit.h"

#include <sys/stat.h>
#include <glib.h>
#include <gst/transcoder/gsttranscoder.h>

Transcoder::Transcoder(const std::string& input_filename)
    : input_filename_(input_filename)
    , transcoder_(nullptr)
    , started_(false)
    , finished_(false)
    , success_(false)
    , duration_(-1)
    , position_(0)
{
    // Output filename will be generated in start() based on options
}

Transcoder::~Transcoder()
{
    if (transcoder_) {
        g_object_unref(transcoder_);
        transcoder_ = nullptr;
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

    // Add force keyframes indicator
    if (options.force_keyframes) {
        suffix += "_bidir";
    }

    // Add psy-tune indicator
    switch (options.psy_tune) {
        case PsyTuning::FILM:
            suffix += "_film";
            break;
        case PsyTuning::ANIMATION:
            suffix += "_animation";
            break;
        case PsyTuning::GRAIN:
            suffix += "_grain";
            break;
        case PsyTuning::STILL_IMAGE:
            suffix += "_still";
            break;
        case PsyTuning::NONE:
        default:
            // No suffix for NONE
            break;
    }

    // Add CRF indicator if using CRF mode
    if (options.crf >= 0 && options.crf <= 51) {
        suffix += "_crf" + std::to_string(options.crf);
    }

    // Add no audio indicator
    if (options.force_no_audio) {
        suffix += "_noaudio";
    }

    if (suffix.empty()) {
        suffix = "_transcoded";
    }

    // Try output filename with generated suffix
    std::string output = base + suffix + ".mp4";

    // Check if file exists, if so, add numbers
    struct stat buffer;
    int counter = 1;
    while (stat(output.c_str(), &buffer) == 0) {
        output = base + suffix + "_" + std::to_string(counter) + ".mp4";
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

    // Generate output filename based on options
    output_filename_ = generateOutputFilename(input_filename_, options);

    // Check if input file exists
    struct stat buffer;
    if (stat(input_filename_.c_str(), &buffer) != 0) {
        error_message_ = "Input file does not exist: " + input_filename_;
        Log::Warning("Transcoder: %s", error_message_.c_str());
        return false;
    }

    Log::Info("Transcoder: Starting transcoding from '%s' to '%s'",
              input_filename_.c_str(), output_filename_.c_str());
    if (options.force_keyframes) {
        Log::Info("Transcoder: Force keyframes enabled");
    }
    if (options.psy_tune != PsyTuning::NONE) {
        Log::Info("Transcoder: Psy-tune mode: %d", static_cast<int>(options.psy_tune));
    }

    // Discover source video properties to match bitrate
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

    guint source_video_bitrate = 0;
    guint source_audio_bitrate = 0;
    bool has_audio = false;
    GstClockTime duration = GST_CLOCK_TIME_NONE;
    guint frame_height = 0;

    if (disc_info) {
        GstDiscovererResult result = gst_discoverer_info_get_result(disc_info);
        if (result != GST_DISCOVERER_OK) {
            const gchar *result_str = "";
            switch (result) {
                case GST_DISCOVERER_URI_INVALID: result_str = "Invalid URI"; break;
                case GST_DISCOVERER_ERROR: result_str = "Discovery error"; break;
                case GST_DISCOVERER_TIMEOUT: result_str = "Discovery timeout"; break;
                case GST_DISCOVERER_BUSY: result_str = "Discoverer busy"; break;
                case GST_DISCOVERER_MISSING_PLUGINS: result_str = "Missing plugins"; break;
                default: result_str = "Unknown error"; break;
            }
            Log::Warning("Transcoder: Discovery failed: %s", result_str);
        }

        duration = gst_discoverer_info_get_duration(disc_info);

        // Get video stream info
        GList *video_streams = gst_discoverer_info_get_video_streams(disc_info);
        if (video_streams) {
            GstDiscovererVideoInfo *vinfo = (GstDiscovererVideoInfo*)video_streams->data;
            source_video_bitrate = gst_discoverer_video_info_get_bitrate(vinfo);
            if (source_video_bitrate == 0) {
                source_video_bitrate = gst_discoverer_video_info_get_max_bitrate(vinfo);
            }
            frame_height = gst_discoverer_video_info_get_height(vinfo);
            gst_discoverer_stream_info_list_free(video_streams);
        } else {
            Log::Warning("Transcoder: No video stream detected");
        }

        // Get audio stream info
        GList *audio_streams = gst_discoverer_info_get_audio_streams(disc_info);
        if (audio_streams) {
            has_audio = true;
            GstDiscovererAudioInfo *ainfo = (GstDiscovererAudioInfo*)audio_streams->data;
            source_audio_bitrate = gst_discoverer_audio_info_get_bitrate(ainfo);
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
    g_free(src_uri);

    // If bitrate not available from metadata, calculate from file size and duration
    if (source_video_bitrate == 0 && duration != GST_CLOCK_TIME_NONE) {
        unsigned long long file_size_bytes = SystemToolkit::file_size(input_filename_);
        if (file_size_bytes > 0) {
            guint64 file_size_bits = file_size_bytes * 8;
            double duration_seconds = (double)duration / GST_SECOND;
            guint total_bitrate = (guint)(file_size_bits / duration_seconds);

            // Subtract audio bitrate to estimate video bitrate
            source_video_bitrate = total_bitrate - source_audio_bitrate;
            Log::Info("Transcoder: Calculated video bitrate from file size: %u bps (file: %llu bytes, duration: %.2f sec)",
                      source_video_bitrate, file_size_bytes, duration_seconds);
        }
    }

    // Set target bitrates (use source bitrate or reasonable defaults)
    guint target_video_bitrate = source_video_bitrate > 0 ? source_video_bitrate : 5000000; // 5 Mbps default (in bps)

    // Apply a quality factor (1.05 = 5% higher to ensure no quality loss)
    const float quality_factor = 1.05f;
    target_video_bitrate = (guint)(target_video_bitrate * quality_factor / 1000); // convert to kbps
    Log::Info("Transcoder: Target video bitrate: %u kbps", target_video_bitrate);

    // Create encoding profile for H.264/AAC in MP4 container
    // Container: MP4
    GstEncodingContainerProfile *container_profile =
        gst_encoding_container_profile_new("mp4-profile",
                                           "MP4 container profile",
                                           gst_caps_from_string("video/quicktime,variant=iso"),
                                           nullptr);

    // Disable automatic profile creation - only use explicitly added profiles
    gst_encoding_profile_set_allow_dynamic_output(GST_ENCODING_PROFILE(container_profile), FALSE);

    // Video profile: H.264
    // Create a preset with x264enc to force using that encoder
    GstElement *x264_preset = gst_element_factory_make("x264enc", "x264preset");
    if (!x264_preset) {
        error_message_ = "Failed to create x264enc element";
        Log::Warning("Transcoder: %s", error_message_.c_str());
        gst_encoding_profile_unref(GST_ENCODING_PROFILE(container_profile));
        return false;
    }

    // Configure x264enc properties
    g_object_set(x264_preset, "speed-preset", 5, NULL);  // fast

    // Use CRF mode if specified, otherwise use bitrate mode
    if (options.crf >= 0 && options.crf <= 51) {
        g_object_set(x264_preset, "pass", 5, NULL);
        g_object_set(x264_preset, "quantizer", options.crf, NULL);        
        g_object_set(x264_preset, "bitrate", 2 * target_video_bitrate, NULL);  // kbps
        Log::Info("Transcoder: Using CRF mode with value: %d", options.crf);
    } else {
        g_object_set(x264_preset, "pass", 0, NULL);
        g_object_set(x264_preset, "bitrate", target_video_bitrate, NULL);  // kbps
        Log::Info("Transcoder: Using bitrate mode: %u kbps", target_video_bitrate);
    }

    // Configure keyframes
    if (options.force_keyframes) {
        g_object_set(x264_preset, "key-int-max", 
                        frame_height > 1400 ? 15 : 30, NULL);
        Log::Info("Transcoder: Add a keyframe every %d frames", frame_height > 1400 ? 15 : 30);

    } else {
        g_object_set(x264_preset, "key-int-max", 250, NULL);
    }

    // Configure psy-tune
    if (options.psy_tune != PsyTuning::NONE) {
        g_object_set(x264_preset, "psy-tune", static_cast<int>(options.psy_tune), NULL);
    }

    // Save the preset to filesystem
    const gchar *preset_name = "vimix_x264_transcoding";
    
    if (!gst_preset_save_preset(GST_PRESET(x264_preset), preset_name)) {
        error_message_ = "Failed to save x264enc preset";
        Log::Warning("Transcoder: %s", error_message_.c_str());
    }
    else
        Log::Info("Transcoder: Created x264enc preset '%s'", preset_name);
    gst_object_unref(x264_preset);

    // Create video profile using the saved preset
    GstCaps *video_caps = gst_caps_from_string("video/x-h264,profile=main");
    GstEncodingVideoProfile *video_profile =
        gst_encoding_video_profile_new(video_caps, preset_name, nullptr, 0);

    // Set video profile presence to always encode
    gst_encoding_profile_set_presence(GST_ENCODING_PROFILE(video_profile), 1);

    // Add video profiles to container
    gst_encoding_container_profile_add_profile(container_profile,
                                               GST_ENCODING_PROFILE(video_profile));

    gst_caps_unref(video_caps);

    // Handle audio encoding based on source and options
    if (has_audio) {
        if (!options.force_no_audio) {
            // Use detected bitrate or default to 128 kbps
            guint target_audio_bitrate = source_audio_bitrate > 0 ? source_audio_bitrate / 1000 : 128;
            Log::Info("Transcoder: Audio stream detected, target bitrate: %u kbps", target_audio_bitrate);

            // Audio profile: AAC
            GstCaps *audio_caps = gst_caps_from_string("audio/mpeg,mpegversion=4,stream-format=raw");

            // Create audio profile with bitrate
            GstEncodingAudioProfile *audio_profile =
                gst_encoding_audio_profile_new(audio_caps, nullptr, nullptr, 0);

            // Set audio bitrate on the profile
            gst_encoding_profile_set_presence(GST_ENCODING_PROFILE(audio_profile), 1); // encode audio

            // Configure audio encoder properties (for avenc_aac)
            GstStructure *audio_element_props = gst_structure_new_empty("audio-properties");
            gst_structure_set(audio_element_props, "bitrate", G_TYPE_INT, target_audio_bitrate * 1000, NULL); // convert to bps
            gst_encoding_profile_set_element_properties(GST_ENCODING_PROFILE(audio_profile),
                                                        audio_element_props);
            // Add audio profile to container
            gst_encoding_container_profile_add_profile(container_profile,
                                                    GST_ENCODING_PROFILE(audio_profile));
            gst_caps_unref(audio_caps);
        }
        else {
            // Add audio profile with presence=0 to explicitly skip audio encoding
            Log::Info("Transcoder: Audio removal forced by options");
        }
    } 
    
    // Create transcoder with the encoding profile
    src_uri = gst_filename_to_uri(input_filename_.c_str(), nullptr);
    gchar *dest_uri = gst_filename_to_uri(output_filename_.c_str(), nullptr);

    if (!src_uri || !dest_uri) {
        error_message_ = "Failed to create URIs from filenames";
        Log::Warning("Transcoder: %s", error_message_.c_str());
        g_free(src_uri);
        g_free(dest_uri);
        gst_encoding_profile_unref(GST_ENCODING_PROFILE(container_profile));
        return false;
    }

    GstTranscoder *transcoder = gst_transcoder_new_full(src_uri, dest_uri,
                                                        GST_ENCODING_PROFILE(container_profile));
    g_free(src_uri);
    g_free(dest_uri);

    if (!transcoder) {
        error_message_ = "Failed to create GstTranscoder";
        Log::Warning("Transcoder: %s", error_message_.c_str());
        gst_encoding_profile_unref(GST_ENCODING_PROFILE(container_profile));
        return false;
    }

    // Store transcoder for cleanup
    transcoder_ = G_OBJECT(transcoder);

    // transcoder should try to avoid reencoding streams where reencoding is not strictly needed
    gst_transcoder_set_avoid_reencoding(transcoder, true);

    // Connect to transcoder signals
    GstTranscoderSignalAdapter *transcoder_signal = gst_transcoder_get_sync_signal_adapter (transcoder);

    g_signal_connect(transcoder_signal, "done", G_CALLBACK(+[](GstTranscoder *trans, gpointer user_data) {
        Transcoder *self = static_cast<Transcoder*>(user_data);
        self->finished_ = true;
        self->success_ = true;
    }), this);

    g_signal_connect(transcoder_signal, "error", G_CALLBACK(+[](GstTranscoder *trans, GError *error,
                                                         GstStructure *details, gpointer user_data) {
        Transcoder *self = static_cast<Transcoder*>(user_data);
        self->error_message_ = std::string("Transcoding error: ") + error->message;
        Log::Warning("Transcoder: %s", self->error_message_.c_str());
        if (details) {
            gchar *details_str = gst_structure_to_string(details);
            Log::Info("Transcoder error details: %s", details_str);
            g_free(details_str);
        }
        self->finished_ = true;
        self->success_ = false;
    }), this);

    g_signal_connect(transcoder_signal, "position-updated",
                    G_CALLBACK(+[](GstTranscoder *trans, GstClockTime position, gpointer user_data) {
        Transcoder *self = static_cast<Transcoder*>(user_data);
        self->position_ = position;
    }), this);

    g_signal_connect(transcoder_signal, "duration-changed",
                    G_CALLBACK(+[](GstTranscoder *trans, GstClockTime duration, gpointer user_data) {
        Transcoder *self = static_cast<Transcoder*>(user_data);
        self->duration_ = duration;
    }), this);

    g_signal_connect(transcoder_signal, "warning", G_CALLBACK(+[](GstTranscoder *trans, GError *error,
                                                           GstStructure *details, gpointer user_data) {
        Log::Notify("Transcoder warning: %s", error->message);
        if (details) {
            gchar *details_str = gst_structure_to_string(details);
            Log::Info("Warning details: %s", details_str);
            g_free(details_str);
        }
    }), this);

    // Start transcoding
    gst_transcoder_run_async(transcoder);

    started_ = true;
    return true;
}

void Transcoder::stop()
{
    // Only stop if transcoding is in progress
    if (!started_ || finished_) {
        return;
    }

    if (transcoder_) {
        GstTranscoder *transcoder = GST_TRANSCODER(transcoder_);

        // Get the pipeline from the transcoder
        GstElement *pipeline = gst_transcoder_get_pipeline(transcoder);
        if (pipeline) {
            // Set pipeline to NULL state to stop transcoding
            gst_element_set_state(pipeline, GST_STATE_NULL);
            gst_object_unref(pipeline);
        }

        // Mark as finished (but not successful)
        finished_ = true;
        success_ = false;
        error_message_ = "Transcoding stopped by user";

        Log::Info("Transcoder: Interrupted transcoding");

        // Remove incomplete output file
        if (!output_filename_.empty()) {
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

bool Transcoder::finished() const
{
    return finished_;
}

bool Transcoder::success() const
{
    return success_ && finished_;
}

double Transcoder::progress() const
{
    if (!started_ || finished_) {
        return finished_ ? 1.0 : 0.0;
    }

    if (duration_ > 0 && position_ >= 0) {
        return static_cast<double>(position_) / static_cast<double>(duration_);
    }

    return 0.0;
}
