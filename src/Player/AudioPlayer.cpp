/*
 * This file is part of vimix - video live mixer
 *
 * **Copyright** (C) 2019-2026 Bruno Herbelin <bruno.herbelin@gmail.com>
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

#include <future>
#include <mutex>

#include "Log.h"
#include "Settings.h"
#include "Toolkit/SystemToolkit.h"

#include "Player/AudioPlayer.h"


AudioPlayer::AudioPlayer() : MediaPlayer()
{
    // audio is the content of the media
    audio_enabled_ = true;
    // no need for hardware decoding and GL memory for a small visualization
    force_software_decoding_ = true;
    // no video effect on the visualization
    video_filter_available_ = false;
}

void AudioPlayer::startDiscovery()
{
    // discover the audio file
    // (no evaluation: only video streams can be evaluated)
    discoverer_ = std::async( AudioPlayer::AudioDiscoverer, uri_);
}

void AudioPlayer::setupPipeline()
{
    // playbin renders audio and its visualization
    // (NB: audio is always rendered to produce the visualization; mute is volume 0)
    gint flags = 0;
    g_object_get( G_OBJECT (pipeline_), "flags", &flags, NULL);
    flags |= GST_PLAY_FLAG_AUDIO | GST_PLAY_FLAG_VIS;
    g_object_set( G_OBJECT (pipeline_), "flags", flags, NULL);

    // setup audio sink
#ifdef APPLE
    GstElement *sink = gst_element_factory_make ("osxaudiosink", "osxaudiosink");
    if (!sink)
        Log::Warning("AudioPlayer %s Could not configure OSX audio sink", std::to_string(id_).c_str());
    else
        g_object_set ( G_OBJECT (pipeline_), "audio-sink", sink, NULL);
#endif

    // set the visualization plugin
    GError *error = NULL;
    GstElement *vis = gst_parse_bin_from_description (AUDIO_VISUALIZATION_PLUGIN
                                                      " ! videoconvert ! capsfilter caps=video/x-raw,format=RGB",
                                                      TRUE, &error);
    if (vis)
        g_object_set( G_OBJECT (pipeline_), "vis-plugin", vis, NULL);
    else
        Log::Warning("AudioPlayer %s Could not create '%s' visualization: %s",
                     std::to_string(id_).c_str(), AUDIO_VISUALIZATION_PLUGIN,
                     error ? error->message : "");
    g_clear_error (&error);
}

#define LIMIT_DISCOVERER

MediaInfo AudioPlayer::AudioDiscoverer(const std::string &uri)
{
    // returned value
    MediaInfo audio_stream_info;

    // audio files can only be played if audio is enabled
    if ( !Settings::application.accept_audio ) {
        audio_stream_info.log = "Audio disabled in Settings";
        return audio_stream_info;
    }

    // visualization plugin is required
    GstElementFactory *factory = gst_element_factory_find (AUDIO_VISUALIZATION_PLUGIN);
    if ( factory == NULL ) {
        audio_stream_info.log = AUDIO_VISUALIZATION_PLUGIN " plugin missing";
        return audio_stream_info;
    }
    gst_object_unref (factory);

#ifdef LIMIT_DISCOVERER
    // Limiting the number of discoverer thread to TWO in parallel
    // (see MediaPlayer::UriDiscoverer)
    static std::mutex mtx_primary;
    static std::mutex mtx_secondary;
    bool use_primary = true;
    if ( !mtx_primary.try_lock() ) { // non-blocking
        use_primary = false;
        mtx_secondary.lock(); // blocking
    }
#endif

    // if uri is valid
    gchar *location = uri.empty() ? NULL : gst_uri_get_location(uri.c_str());
    if ( location != NULL && SystemToolkit::file_exists( location ) ) {

        GError *err = NULL;
        GstDiscoverer *discoverer = gst_discoverer_new (DISCOVER_TIMOUT * GST_SECOND, &err);

        /* Instantiate the Discoverer */
        if (!discoverer) {
            Log::Warning("AudioPlayer Error creating discoverer instance: %s\n", err ? err->message : "");
        }
        else {
            GstDiscovererInfo *info = gst_discoverer_discover_uri (discoverer, uri.c_str(), &err);
            GstDiscovererResult result = gst_discoverer_info_get_result (info);
            switch (result) {
            case GST_DISCOVERER_URI_INVALID:
                audio_stream_info.log = "Invalid URI";
                break;
            case GST_DISCOVERER_ERROR:
                audio_stream_info.log = std::string( "Warning: " ) + (err ? err->message : "");
                result = GST_DISCOVERER_OK; // try to read the file anyways
                break;
            case GST_DISCOVERER_TIMEOUT:
                audio_stream_info.log = "Timeout loading";
                break;
            case GST_DISCOVERER_BUSY:
                audio_stream_info.log = "Busy";
                break;
            case GST_DISCOVERER_MISSING_PLUGINS:
            {
                const GstStructure *s = gst_discoverer_info_get_misc (info);
                gchar *str = gst_structure_to_string (s);
                audio_stream_info.log = std::string( "Warning: " ) + std::string(str);
                g_free (str);
                result = GST_DISCOVERER_OK; // try to read the file anyways
            }
                break;
            default:
                break;
            }

            if ( result == GST_DISCOVERER_OK ) {
                // get first audio stream
                GList *audios = gst_discoverer_info_get_audio_streams(info);
                if ( g_list_length(audios) > 0 ) {
                    GstDiscovererStreamInfo *tmpinf = (GstDiscovererStreamInfo *) audios->data;

                    // the media is the visualization of the audio
                    audio_stream_info.width = audio_stream_info.par_width = AUDIO_VISUALIZATION_WIDTH;
                    audio_stream_info.height = AUDIO_VISUALIZATION_HEIGHT;
                    audio_stream_info.framerate_n = AUDIO_VISUALIZATION_FPS;
                    audio_stream_info.framerate_d = 1;
                    audio_stream_info.dt = GST_SECOND / AUDIO_VISUALIZATION_FPS;
                    audio_stream_info.isimage = false;
                    audio_stream_info.hasaudio = true;
                    audio_stream_info.end = gst_discoverer_info_get_duration (info);
                    audio_stream_info.seekable = gst_discoverer_info_get_seekable (info);
                    if ( GST_IS_DISCOVERER_AUDIO_INFO(tmpinf) )
                        audio_stream_info.bitrate = gst_discoverer_audio_info_get_bitrate( GST_DISCOVERER_AUDIO_INFO(tmpinf) );

                    // try to fill-in the codec information
                    GstCaps *caps = gst_discoverer_stream_info_get_caps (tmpinf);
                    if (caps) {
                        gchar *codecstring = gst_pb_utils_get_codec_description(caps);
                        audio_stream_info.codec_name = std::string( codecstring );
                        g_free(codecstring);
                        gst_caps_unref (caps);
                    }

                    // inform that it succeeded
                    audio_stream_info.valid = true;
                }
                else
                    audio_stream_info.log = "No audio stream";

                gst_discoverer_stream_info_list_free(audios);
            }

            if (info)
                gst_discoverer_info_unref (info);

            g_object_unref( discoverer );
        }

        g_clear_error (&err);
    }
    else {
        audio_stream_info.log = "No such file";
    }
    g_free(location);

#ifdef LIMIT_DISCOVERER
    if (use_primary)
        mtx_primary.unlock();
    else
        mtx_secondary.unlock();
#endif

    return audio_stream_info;
}
