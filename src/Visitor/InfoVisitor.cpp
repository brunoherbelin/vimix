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

#include <ostream>
#include <vector>
#include <sstream>
#include <iomanip>
#include <locale>
#include <algorithm>
#include <cctype>

#include <glm/glm.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <glm/gtc/constants.hpp>
#include <glm/gtc/matrix_access.hpp>


#include "Scene/Scene.h"
#include "Player/MediaPlayer.h"
#include "Source/MediaSource.h"
#include "Source/AudioSource.h"
#include "Source/CloneSource.h"
#include "Source/DraftSource.h"
#include "Source/RenderSource.h"
#include "Source/SessionSource.h"
#include "Source/PatternSource.h"
#include "Source/DeviceSource.h"
#include "Source/ScreenCaptureSource.h"
#include "Source/NetworkSource.h"
#include "Source/SrtReceiverSource.h"
#include "Source/TextSource.h"
#include "Source/ShaderSource.h"
#include "Session.h"
#include "Toolkit/BaseToolkit.h"
#include "Toolkit/SystemToolkit.h"
#include "Toolkit/GstToolkit.h"

#include "InfoVisitor.h"



InfoVisitor::InfoVisitor() : brief_(true), current_id_(0)
{
}

void InfoVisitor::visit(Node &n)
{
    std::ostringstream oss;
    oss << std::fixed << std::setprecision(1);
    oss << "Pos    ( " << n.translation_.x << ", " << n.translation_.y << " )" << std::endl;
    oss << "Scale ( " << n.scale_.x << ", " << n.scale_.y  << " )" << std::endl;
    oss << "Angle " << std::setprecision(2) << n.rotation_.z * 180.f / M_PI << "\u00B0" << std::endl;

    information_ = oss.str();
}

void InfoVisitor::visit(Group &)
{
}

void InfoVisitor::visit(Switch &)
{
}

void InfoVisitor::visit(Scene &)
{
}

void InfoVisitor::visit(Primitive &)
{
}


void InfoVisitor::visit(MediaPlayer &mp)
{
    // do not ask twice
    if (known(mp.id()))
        return;

    std::ostringstream oss;
    if (mp.failed()) {
        oss << BaseToolkit::transliterate(mp.filename()) << std::endl << std::endl << mp.log();
    }
    else {
        if (brief_) {
            std::string codec = mp.media().codec_name.substr(0, mp.media().codec_name.find_first_of(" "));
            std::transform(codec.begin(), codec.end(), codec.begin(),
                           [](unsigned char c) { return static_cast<char>(std::toupper(c)); });
            oss << codec << ", ";
            oss << mp.width() << " x " << mp.height();
            if (!mp.singleFrame() && mp.frameRate() > 0.)
                oss << ", " << std::fixed << std::setprecision(1) << mp.frameRate() << " fps";
            oss << "\n" << BaseToolkit::transliterate(SystemToolkit::filename(mp.filename())) << std::endl;
        }
        else {
            oss << BaseToolkit::transliterate(mp.filename()) << std::endl;
            oss << mp.media().codec_name << std::endl;
            oss << mp.width() << " x " << mp.height();
            if (!mp.singleFrame() && mp.frameRate() > 0.)
                oss << ", " << std::fixed << std::setprecision(1) << mp.frameRate() << " fps";
        }
    }

    information_ = oss.str();

    // remember
    if ( mp.isOpen() && ( mp.evaluation().done || mp.media().isimage ) )
        current_id_ = mp.id();
}

void InfoVisitor::visit(Stream &n)
{
    std::ostringstream oss;
    if (brief_) {
        oss << BaseToolkit::splitted(n.description(), '!').front();
    }
    else {
        oss << n.description();
    }
    information_ = oss.str();
}


void InfoVisitor::visit (MediaSource& s)
{

    s.mediaplayer()->accept(*this);

}

void InfoVisitor::visit (AudioSource& s)
{
    // NB: visited after MediaSource, which filled-in the information of the
    // media player but did not remember it (no evaluation of audio players)
    MediaPlayer *mp = s.mediaplayer();
    if (known(mp->id()))
        return;

    std::ostringstream oss;
    if (mp->failed()) {
        oss << BaseToolkit::transliterate(mp->filename()) << std::endl << std::endl << mp->log();
    }
    else {
        const GstClockTime d = mp->timeline()->duration();
        if (brief_) {
            oss << mp->media().codec_name.substr(0, mp->media().codec_name.find_first_of("(,"));
            if (GST_CLOCK_TIME_IS_VALID(d) && d > 0)
                oss << ", " << GstToolkit::time_to_string(d, GstToolkit::TIME_STRING_READABLE);
            oss  << std::endl << BaseToolkit::transliterate(SystemToolkit::filename(mp->filename()));
        }
        else {
            oss << BaseToolkit::transliterate(mp->filename()) << std::endl;
            oss << mp->media().codec_name << std::endl;
            if (GST_CLOCK_TIME_IS_VALID(d) && d > 0)
                oss << "Duration " << GstToolkit::time_to_string(d, GstToolkit::TIME_STRING_READABLE) << ", ";
            if (mp->media().bitrate > 0)
                oss << mp->media().bitrate / 1000 << " kbps";
        }
    }

    information_ = oss.str();

    // remember
    if ( mp->isOpen() )
        current_id_ = mp->id();
}

void InfoVisitor::visit (SessionFileSource& s)
{
    if (known(s.id()))
        return;

    std::ostringstream oss;

    if (s.session() != nullptr && s.session()->frame() != nullptr){
        uint N = s.session()->size();
        std::string numsource = std::to_string(N) + " source" + (N>1 ? "s" : "");
        uint T = s.session()->numSources();
        if (T>N)
            numsource += " (" + std::to_string(T) + " in total)";
        if (brief_) {
            oss << (s.frame()->flags() & FrameBuffer::FrameBuffer_alpha ? "RGBA, " : "RGB, ");
            oss << s.session()->frame()->width() << " x " << s.session()->frame()->height() << std::endl;
            oss << SystemToolkit::filename(s.path()) << std::endl;
            oss << numsource ;
        }
        else {
            oss << s.path() << " (" << numsource << ")" << std::endl;
            oss << (s.frame()->flags() & FrameBuffer::FrameBuffer_alpha ? "RGBA" : "RGB") << std::endl;
            oss << s.session()->frame()->width() << " x " << s.session()->frame()->height();
        }

        current_id_ = s.id();
    }
    else {
        oss << s.path() << std::endl << std::endl << "Failed to load.";
    }

    information_ = oss.str();
}

void InfoVisitor::visit (DraftSource& s)
{
    // information of the source in live session
    if (s.origin() != nullptr)
        s.origin()->accept(*this);
}

void InfoVisitor::visit (SessionGroupSource& s)
{
    if (known(s.id()))
        return;

    std::ostringstream oss;

    if (s.session() && s.session()->frame()){

        if (brief_) {
            oss << (s.session()->frame()->flags() & FrameBuffer::FrameBuffer_alpha ? "RGBA, " : "RGB, ");
            oss << s.session()->frame()->width() << " x " << s.session()->frame()->height();
        }
        else {
            oss << "Bundle of ";
            uint N = s.session()->size();
            oss << N << " source" << (N>1 ? "s" : "");
            uint T = s.session()->numSources();
            if (T>N)
                oss << " (" << std::to_string(T) << " total)";
            oss << std::endl;
            oss << (s.session()->frame()->flags() & FrameBuffer::FrameBuffer_alpha ? "RGBA" : "RGB") << std::endl;
            oss << s.session()->frame()->width() << " x " << s.session()->frame()->height();
        }
        current_id_ = s.id();
    }
    else {
        oss << std::endl << "Empty bundle.";
    }

    information_ = oss.str();
}

void InfoVisitor::visit (RenderSource& s)
{
    if (known(s.id()))
        return;

    std::ostringstream oss;

    if (s.frame()){
        if (brief_) {
            oss << (s.frame()->flags() & FrameBuffer::FrameBuffer_alpha ? "RGBA, " : "RGB, ");
            oss << s.frame()->width() << " x " << s.frame()->height();
        }
        else {
            oss << "Rendering Output (";
            oss << std::get<2>(RenderSource::ProvenanceMethod[s.renderingProvenance()]) << ") " << std::endl;
            oss << (s.frame()->flags() & FrameBuffer::FrameBuffer_alpha ? "RGBA" : "RGB") << std::endl;
            oss << s.frame()->width() << " x " << s.frame()->height();
        }
    }
    else
        oss << "Undefined";

    information_ = oss.str();
    current_id_ = s.id();
}

void InfoVisitor::visit (CloneSource& s)
{
    if (known(s.id()))
        return;

    std::ostringstream oss;

    if (!s.failed() && s.frame()){
        if (brief_) {
            oss << (s.frame()->flags() & FrameBuffer::FrameBuffer_alpha ? "RGBA, " : "RGB, ");
            oss << s.frame()->width() << " x " << s.frame()->height();
        }
        else {
            if (s.origin())
                oss << "Clone of '" << s.origin()->name() << "' + " ;
            oss << std::get<2>(FrameBufferFilter::Types[s.filter()->type()]) << " filter" << std::endl;
            oss << (s.frame()->flags() & FrameBuffer::FrameBuffer_alpha ? "RGBA" : "RGB") << std::endl;
            oss << s.frame()->width() << " x " << s.frame()->height();
        }
        current_id_ = s.id();
    }
    else
        oss << "Undefined";

    information_ = oss.str();
}

void InfoVisitor::visit (PatternSource& s)
{
    if (known(s.id()))
        return;

    std::ostringstream oss;

    Pattern *ptn = s.pattern();
    if (ptn) {
        if (ptn->failed())
            oss << ptn->description() << std::endl << ptn->log();
        else {
            if (brief_) {
                oss << (s.frame()->flags() & FrameBuffer::FrameBuffer_alpha ? "RGBA, " : "RGB, ");
                oss << ptn->width() << " x " << ptn->height();
            }
            else {
                oss << Pattern::get(ptn->type()).label << " pattern" << std::endl;
                oss << (s.frame()->flags() & FrameBuffer::FrameBuffer_alpha ? "RGBA" : "RGB") << std::endl;
                oss << ptn->width() << " x " << ptn->height();
            }
        }
    }
    else
        oss << "Undefined";

    information_ = oss.str();
    current_id_ = s.id();
}

void InfoVisitor::visit (DeviceSource& s)
{
    if (known(s.id()))
        return;

    std::ostringstream oss;

    GstToolkit::PipelineConfigSet confs = Device::manager().config( Device::manager().index(s.device()));
    if ( !confs.empty()) {
        GstToolkit::PipelineConfig best = *confs.rbegin();
        float fps = static_cast<float>(best.fps_numerator) / static_cast<float>(best.fps_denominator);

        if (brief_) {
            oss << best.stream  << ", ";
            oss << best.width << " x " << best.height << ", ";
            oss << std::fixed << std::setprecision(0) << fps << "fps";
        }
        else {
            std::string src_element = Device::manager().description( Device::manager().index(s.device()));
            src_element = src_element.substr(0, src_element.find(" "));
            oss << s.device() << " (" << src_element << ")" << std::endl;
            oss << best.stream << " " << best.format  << std::endl;
            oss << best.width << " x " << best.height << ", ";
            oss << std::fixed << std::setprecision(1) << fps << " fps";
        }
    }
    else {
        oss << s.device() << " not available.";
    }

    information_ = oss.str();
    current_id_ = s.id();
}


void InfoVisitor::visit (ScreenCaptureSource& s)
{
    if (known(s.id()))
        return;

    std::ostringstream oss;

    GstToolkit::PipelineConfigSet confs = ScreenCapture::manager().config( ScreenCapture::manager().index(s.window()));
    if ( s.pending() ) {
        oss << "Waiting for the selection of a screen or window.";
    }
    else if ( !confs.empty()) {
        GstToolkit::PipelineConfig best = *confs.rbegin();
        float fps = static_cast<float>(best.fps_numerator) / static_cast<float>(best.fps_denominator);

        if (brief_) {
            oss << best.stream << " " << best.format  << ", ";
            oss << best.width << " x " << best.height << ", ";
            oss << std::fixed << std::setprecision(0) << fps << "fps";
        }
        else {
            std::string src_element = ScreenCapture::manager().description( ScreenCapture::manager().index(s.window()));
            src_element = src_element.substr(0, src_element.find(" "));
            oss << s.window() << " (" << src_element << ")" << std::endl;
            oss << best.stream << " " << best.format  << std::endl;
            oss << best.width << " x " << best.height << ", ";
            oss << std::fixed << std::setprecision(1) << fps << " fps";
        }
    }
    else {
        oss << s.window() << " not available.";
    }

    information_ = oss.str();
    // keep updating until the screen or window is selected
    if ( !s.pending() )
        current_id_ = s.id();
}


void InfoVisitor::visit (NetworkSource& s)
{
    if (known(s.id()))
        return;

    std::ostringstream oss;

    NetworkStream *ns = s.networkStream();
    if (ns) {
        if (ns->failed()) {
            oss << s.stream()->log();
        }
        else {
            if (brief_) {
                oss << NetworkToolkit::stream_protocol_label[ns->protocol()] << std::endl;
                oss << ns->resolution().x << " x " << ns->resolution().y << std::endl;
                oss << "IP " << ns->serverAddress();
            }
            else {
                oss << s.connection() << " (" << ns->serverAddress() << ")" << std::endl;
                oss << NetworkToolkit::stream_protocol_label[ns->protocol()] << std::endl;
                oss << ns->resolution().x << " x " << ns->resolution().y;
            }
        }
    }
    else
        oss << "Undefined";

    information_ = oss.str();
    current_id_ = s.id();
}


void InfoVisitor::visit (GenericStreamSource& s)
{
    if (known(s.id()))
        return;

    std::ostringstream oss;

    Stream *stm = s.stream();
    if (stm) {
        if (stm->failed()) {
            oss << stm->log();
        }
        else {

            if (brief_) {
                oss << (s.frame()->flags() & FrameBuffer::FrameBuffer_alpha ? "RGBA, " : "RGB, ");
                oss << stm->width() << " x " << stm->height();
            }
            else {
                std::string src_element = s.gstElements().front();
                src_element = src_element.substr(0, src_element.find(" "));
                oss << "gstreamer (" << src_element << ")" << std::endl;
                oss << (s.frame()->flags() & FrameBuffer::FrameBuffer_alpha ? "RGBA" : "RGB") << std::endl;
                oss << stm->width() << " x " << stm->height();
            }
        }
    }
    else
        oss << "Undefined";

    information_ = oss.str();
    current_id_ = s.id();
}

void InfoVisitor::visit (SrtReceiverSource& s)
{
    if (known(s.id()))
        return;

    std::ostringstream oss;

    Stream *stm = s.stream();
    if (stm) {
        if (stm->failed()) {
            oss << s.uri() << std::endl << stm->log();
        }
        else if (stm->isOpen()) {
            if (brief_) {
                oss << "H264 ," << stm->width() << " x " << stm->height() << std::endl;
                oss << s.uri() << std::endl;
            }
            else {
                oss << "SRT Receiver " << s.uri() << std::endl;
                oss << "H264 (" << stm->decoderName() << ")" << std::endl;
                oss << stm->width() << " x " << stm->height();
            }
        }
        else {
            if (brief_) {
                oss << "Connecting..." << std::endl;
                oss << s.uri() << std::endl;
            }
            else {
                oss << "SRT Receiver " << s.uri() << std::endl;
                oss << "Connecting...";
            }
        }
    }
    else
        oss << "Undefined";

    information_ = oss.str();
    current_id_ = s.id();
}

void InfoVisitor::visit (TextSource& s)
{
    if (known(s.id()))
        return;

    std::ostringstream oss;

    TextContents *ptn = s.contents();
    if (ptn) {
        if (ptn->failed())
            oss << ptn->description() << std::endl << ptn->log();
        else {
            if (brief_) {
                oss << (s.frame()->flags() & FrameBuffer::FrameBuffer_alpha ? "RGBA, " : "RGB, ");
                oss << ptn->width() << " x " << ptn->height();
            }
            else {
                if (ptn->isSubtitle())
                    oss << ptn->text() << std::endl;
                else
                    oss << "Text";
                oss << (s.frame()->flags() & FrameBuffer::FrameBuffer_alpha ? "RGBA" : "RGB") << std::endl;
                oss << ptn->width() << " x " << ptn->height();
            }
        }
    }
    else
        oss << "Undefined";

    information_ = oss.str();
    current_id_ = s.id();
}

void InfoVisitor::visit (ShaderSource& s)
{
    if (known(s.id()))
        return;

    std::ostringstream oss;

    ImageFilter *ft = s.filter();
    if (ft) {
        if (brief_) {
            oss << (s.frame()->flags() & FrameBuffer::FrameBuffer_alpha ? "RGBA, " : "RGB, ");
            oss << (int) ft->resolution().x << " x " << (int) ft->resolution().y;
        }
        else {
            oss << "Shader code" << std::endl;
            oss << (s.frame()->flags() & FrameBuffer::FrameBuffer_alpha ? "RGBA" : "RGB") << std::endl;
            oss << (int) ft->resolution().x << " x " << (int) ft->resolution().y;
        }
    }
    else
        oss << "Undefined";

    information_ = oss.str();
    current_id_ = s.id();
}
