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

#include <gst/gst.h>
#include <gst/pbutils/pbutils.h>
#include <gst/app/gstappsink.h>

#include "SystemToolkit.h"
#include "Decorations.h"
#include "Visitor.h"
#include "Log.h"

#include "TextSource.h"

/// filesrc location=/home/bh/vimix/test.srt ! subparse ! txt.
/// videotestsrc pattern=black background-color=0x00000000 ! video/x-raw,width=1280,height=768,framerate=24/1 !
/// textoverlay name=txt halignment=center valignment=center font-desc="sans,72" !

bool TextContents::SubtitleDiscoverer(const std::string &path)
{
    bool ret = false;

    // if path is valid
    if (SystemToolkit::file_exists(path.c_str())) {
        // set uri to open
        gchar *uritmp = gst_filename_to_uri(path.c_str(), NULL);
        // if uri is valid
        if (uritmp != NULL) {
            // create discoverer
            GError *err = NULL;
            GstDiscoverer *discoverer = gst_discoverer_new(GST_SECOND, &err);
            if (discoverer != nullptr) {
                // get discoverer info for the URI
                GstDiscovererInfo *info = NULL;
                info = gst_discoverer_discover_uri(discoverer, uritmp, &err);
                GstDiscovererResult result = gst_discoverer_info_get_result(info);
                // discoverer success
                if (result == GST_DISCOVERER_OK) {
                    // get subtitle streams
                    GList *streams = gst_discoverer_info_get_subtitle_streams(info);
                    // return true if at least one subtitle stream is discovered
                    ret = (g_list_length(streams) > 0);
                    gst_discoverer_stream_info_list_free(streams);
                }
                if (info)
                    gst_discoverer_info_unref(info);
                g_object_unref(discoverer);
            }
            g_clear_error(&err);
            g_free(uritmp);
        }
    }

    return ret;
}


TextContents::TextContents()
    : Stream(), src_(nullptr), txt_(nullptr),
    fontdesc_(""), color_(0xffffffff), outline_(2), outline_color_(4278190080),
    halignment_(1), valignment_(2), xalignment_(0.f), yalignment_(0.f)
{
}

void TextContents::execute_open()
{
    // reset
    opened_ = false;
    textureinitialized_ = false;

    // Add custom app sink to the gstreamer pipeline
    std::string description = description_;
    description += " ! appsink name=sink";

    // parse pipeline descriptor
    GError *error = NULL;
    pipeline_ = gst_parse_launch(description.c_str(), &error);
    if (error != NULL) {
        fail(std::string("TextContents: Could not construct pipeline: ") + error->message + "\n"
             + description);
        g_clear_error(&error);
        return;
    }
    g_object_set(G_OBJECT(pipeline_), "name", std::to_string(id_).c_str(), NULL);
    gst_pipeline_set_auto_flush_bus(GST_PIPELINE(pipeline_), true);

    // GstCaps *caps = gst_static_caps_get (&frame_render_caps);
    std::string capstring = "video/x-raw,format=RGBA,width=" + std::to_string(width_)
                            + ",height=" + std::to_string(height_);
    GstCaps *caps = gst_caps_from_string(capstring.c_str());
    if (!caps || !gst_video_info_from_caps(&v_frame_video_info_, caps)) {
        fail("TextContents: Could not configure video frame info");
        return;
    }

    // setup appsink
    GstElement *sink = gst_bin_get_by_name(GST_BIN(pipeline_), "sink");
    if (!sink) {
        fail("TextContents: Could not configure pipeline sink.");
        return;
    }

    // instruct sink to use the required caps
    gst_app_sink_set_caps(GST_APP_SINK(sink), caps);
    gst_caps_unref(caps);

    // Instruct appsink to drop old buffers when the maximum amount of queued buffers is reached.
    gst_app_sink_set_max_buffers(GST_APP_SINK(sink), 30);
    gst_app_sink_set_drop(GST_APP_SINK(sink), true);

    // set the callbacks
    GstAppSinkCallbacks callbacks;
#if GST_VERSION_MINOR > 18 && GST_VERSION_MAJOR > 0
    callbacks.new_event = NULL;
#if GST_VERSION_MINOR > 23
    callbacks.propose_allocation = NULL;
#endif
#endif
    callbacks.new_preroll = callback_new_preroll;
    callbacks.eos = callback_end_of_stream;
    callbacks.new_sample = callback_new_sample;
    gst_app_sink_set_callbacks(GST_APP_SINK(sink), &callbacks, this, NULL);
    gst_app_sink_set_emit_signals(GST_APP_SINK(sink), false);

    //
    // TextContents
    //
    // keep source and text elements for dynamic properties change
    src_ = gst_bin_get_by_name(GST_BIN(pipeline_), "src");
    txt_ = gst_bin_get_by_name(GST_BIN(pipeline_), "txt");

    if (txt_)  {
        // if there is a src in the pipeline, it should be a file
        if (src_) {
            // set the location of the file
            g_object_set(G_OBJECT(src_), "location", text_.c_str(), NULL);
        }
        else {
            // set the content of the text overlay
            g_object_set(G_OBJECT(txt_), "text", text_.c_str(), NULL);
        }

        // Auto default font
        if (fontdesc_.empty()) {
            fontdesc_ = "sans ";
            fontdesc_ += std::to_string( height_ / 10 );
        }

        // set text properties
        g_object_set(G_OBJECT(txt_),
                     "font-desc",
                     fontdesc_.c_str(),
                     "color",
                     color_,
                     "outline-color",
                     outline_color_,
                     "halignment",
                     halignment_ < 3 ? halignment_ : 5,
                     "line-alignment",
                     halignment_ < 3 ? halignment_ : 1,
                     "valignment",
                     valignment_ < 2   ? valignment_ + 1
                     : valignment_ > 2 ? 5
                                       : 4,
                     "draw-outline",
                     outline_ > 0,
                     "draw-shadow",
                     outline_ > 1,
                     "auto-resize",
                     FALSE,
                     NULL);
        setHorizontalPadding(xalignment_);
        setVerticalPadding(yalignment_);
    }


    // set to desired state (PLAY or PAUSE)
    live_ = false;
    GstStateChangeReturn ret = gst_element_set_state(pipeline_, desired_state_);
    if (ret == GST_STATE_CHANGE_FAILURE) {
        fail(std::string("TextContents: Could not open ") + description_);
        return;
    } else if (ret == GST_STATE_CHANGE_NO_PREROLL) {
        Log::Info("TextContents: %s is a live stream", std::to_string(id_).c_str());
        live_ = true;
    }

    // instruct the sink to send samples synched in time if not live source
    gst_base_sink_set_sync(GST_BASE_SINK(sink), !live_);

    bus_ = gst_element_get_bus(pipeline_);
    // avoid filling up bus with messages
    gst_bus_set_flushing(bus_, true);

    // all good
    Log::Info("TextContents: %s Opened '%s' (%d x %d)",
              std::to_string(id_).c_str(),
              description.c_str(),
              width_,
              height_);
    opened_ = true;

    // launch a timeout to check on open status
    std::thread(timeout_initialize, this).detach();
}

void TextContents::open(const std::string &text, glm::ivec2 res)
{
    std::string gstreamer_pattern
        = "videotestsrc pattern=black background-color=0x00000000 ! "
          "textoverlay text=\"\" halignment=center valignment=center ";

    // set text
    text_ = text;
    // test if text is the filename of a subtitle
    if (TextContents::SubtitleDiscoverer(text_)) {
        // setup a pipeline that reads the file and parses subtitle
        // Log::Info("Using %s as subtitle file", text.c_str());
        gstreamer_pattern = "filesrc name=src ! subparse ! queue ! txt. ";
    } else {
        // else, setup a pipeline with custom appsrc
        // Log::Info("Using '%s' as raw text content", text.c_str());
        gstreamer_pattern = "";
    }
    gstreamer_pattern += "videotestsrc name=bg pattern=black background-color=0x00000000 ! "
                         "textoverlay name=txt ";

    // (private) open stream
    Stream::open(gstreamer_pattern, res.x, res.y);
}

void TextContents::setText(const std::string &t)
{
    if ( src_ == nullptr && text_.compare(t) != 0) {
        // set text
        text_ = t;
        // apply if ready
        if (txt_)
            g_object_set(G_OBJECT(txt_), "text", text_.c_str(), NULL);
    }
}

void TextContents::setFontDescriptor(const std::string &fd)
{
    if (!fd.empty() && fontdesc_.compare(fd) != 0) {
        // set text
        fontdesc_ = fd;
        // apply if ready
        if (txt_)
            g_object_set(G_OBJECT(txt_),"font-desc", fontdesc_.c_str(),  NULL);
    }
}

void TextContents::setColor(uint c)
{
    if ( color_ != c ) {
        // set value
        color_ = c;
        // apply if ready
        if (txt_)
            g_object_set(G_OBJECT(txt_), "color", color_, NULL);
    }
}


void TextContents::setOutline(uint o)
{
    if (outline_ != o) {
        // set value
        outline_ = o;
        // apply if ready
        if (txt_) {
            g_object_set(G_OBJECT(txt_),
                         "draw-outline", outline_ > 0,
                         "draw-shadow", outline_ > 1,
                         NULL);
        }
    }
}

void TextContents::setOutlineColor(uint c)
{
    if ( outline_color_ != c ) {
        // set value
        outline_color_ = c;
        // apply if ready
        if (txt_)
            g_object_set(G_OBJECT(txt_), "outline-color", outline_color_, NULL);
    }
}

void TextContents::setHorizontalAlignment(uint h)
{
    if ( halignment_ != h ) {
        // set value
        halignment_ = h;
        // apply if ready
        if (txt_) {
            g_object_set(G_OBJECT(txt_),
                         "halignment", halignment_ < 3 ? halignment_ : 4,
                         "line-alignment", halignment_ < 3 ? halignment_ : 1,
                         NULL);
        }
    }
}

void TextContents::setVerticalAlignment(uint v)
{
    if ( valignment_ != v ) {
        // set value
        valignment_ = v;
        // apply if ready
        if (txt_) {
            g_object_set(G_OBJECT(txt_),
                         "valignment", valignment_ < 2 ? valignment_+1 : valignment_ > 2 ? 3 : 4,
                         NULL);
        }
    }
}

void TextContents::setHorizontalPadding(float x)
{
    xalignment_ = x;
    // apply if ready
    if (txt_) {
        if (halignment_ > 2)
            g_object_set(G_OBJECT(txt_), "xpos", CLAMP(xalignment_, 0.f, 1.f), NULL);
        else
            g_object_set(G_OBJECT(txt_), "xpad", CLAMP((int)xalignment_, 0, 10000), NULL);
    }
}

void TextContents::setVerticalPadding(float y)
{
    yalignment_ = y;
    // apply if ready
    if (txt_) {
        if (valignment_ > 2)
            g_object_set(G_OBJECT(txt_), "ypos", CLAMP(yalignment_, 0.f, 1.f), NULL);
        else
            g_object_set(G_OBJECT(txt_), "ypad", CLAMP((int)yalignment_, 0, 10000), NULL);
    }
}

TextSource::TextSource(uint64_t id) : StreamSource(id)
{
    // create stream
    stream_ = static_cast<Stream *>( new TextContents );

    // set symbol
    symbol_ = new Symbol(Symbol::TEXT, glm::vec3(0.75f, 0.75f, 0.01f));
    symbol_->scale_.y = 1.5f;
}


void TextSource::setContents(const std::string &c, glm::ivec2 resolution)
{
    // set contents
    contents()->open( c, resolution );

    // play gstreamer
    stream_->play(true);

    // delete and reset render buffer to enforce re-init of StreamSource
    if (renderbuffer_)
        delete renderbuffer_;
    renderbuffer_ = nullptr;

    // will be ready after init and one frame rendered
    ready_ = false;
}

TextContents *TextSource::contents() const
{
    return dynamic_cast<TextContents *>(stream_);
}

void TextSource::accept(Visitor& v)
{
    StreamSource::accept(v);
    v.visit(*this);
}

glm::ivec2 TextSource::icon() const
{
    return glm::ivec2(ICON_SOURCE_TEXT);
}

std::string TextSource::info() const
{
    if ( contents()->isSubtitle() )
        return "Subtitle text";
    else
        return "Free text";
}
