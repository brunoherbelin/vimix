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

#include <vector>
#include <sstream>
#include <iomanip>

#include <glm/glm.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <glm/gtc/constants.hpp>
#include <glm/gtc/matrix_access.hpp>


#include "Scene.h"
#include "MediaPlayer.h"
#include "MediaSource.h"
#include "CloneSource.h"
#include "RenderSource.h"
#include "SessionSource.h"
#include "PatternSource.h"
#include "DeviceSource.h"
#include "ScreenCaptureSource.h"
#include "NetworkSource.h"
#include "SrtReceiverSource.h"
#include "MultiFileSource.h"
#include "TextSource.h"
#include "Session.h"
#include "BaseToolkit.h"
#include "SystemToolkit.h"

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
    if (current_id_ == mp.id())
        return;

    std::ostringstream oss;
    if (mp.failed()) {
        oss << mp.filename() << std::endl << std::endl << mp.log();
    }
    else {
        if (brief_) {
            oss << SystemToolkit::filename(mp.filename()) << std::endl;
            oss << mp.media().codec_name.substr(0, mp.media().codec_name.find_first_of(" (,")) << ", ";
            oss << mp.width() << " x " << mp.height();
            if (!mp.singleFrame() && mp.frameRate() > 0.)
                oss << ", " << std::fixed << std::setprecision(0) << mp.frameRate() << " fps";
        }
        else {
            oss << mp.filename() << std::endl;
            oss << mp.media().codec_name << std::endl;
            oss << mp.width() << " x " << mp.height() ;
            if (!mp.singleFrame() && mp.frameRate() > 0.)
                oss << ", " << std::fixed << std::setprecision(1) << mp.frameRate() << " fps";
        }
    }

    information_ = oss.str();

    // remember
    if ( mp.isOpen() )
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
    if (current_id_ == s.id())
        return;

    s.mediaplayer()->accept(*this);

    if (s.ready())
        current_id_ = s.id();
}

void InfoVisitor::visit (SessionFileSource& s)
{
    if (current_id_ == s.id())
        return;

    std::ostringstream oss;

    if (s.session() != nullptr && s.session()->frame() != nullptr){
        uint N = s.session()->size();
        std::string numsource = std::to_string(N) + " source" + (N>1 ? "s" : "");
        uint T = s.session()->numSources();
        if (T>N)
            numsource += " (" + std::to_string(T) + " total)";
        if (brief_) {
            oss << SystemToolkit::filename(s.path()) << std::endl;
            oss << numsource << ", " ;
            oss << "RGB, " << s.session()->frame()->width() << " x " << s.session()->frame()->height();
        }
        else {
            oss << s.path() << std::endl;
            oss << "Child session (" << numsource << "), RGB" << std::endl;
            oss << s.session()->frame()->width() << " x " << s.session()->frame()->height();
        }

        current_id_ = s.id();
    }
    else {
        oss << s.path() << std::endl << std::endl << "Failed to load.";
    }

    information_ = oss.str();
}

void InfoVisitor::visit (SessionGroupSource& s)
{
    if (current_id_ == s.id())
        return;

    std::ostringstream oss;

    if (s.session() && s.session()->frame()){

        if (!brief_) oss << "Bundle of ";
        uint N = s.session()->size();
        oss << N << " source" << (N>1 ? "s" : "");
        uint T = s.session()->numSources();
        if (T>N)
            oss << " (" << std::to_string(T) << " total)";
        oss << std::endl;

        if (brief_) {
            oss << (s.session()->frame()->flags() & FrameBuffer::FrameBuffer_alpha ? "RGBA, " : "RGB, ");
            oss << s.session()->frame()->width() << " x " << s.session()->frame()->height();
        }
        else {
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
    if (current_id_ == s.id())
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
    if (current_id_ == s.id())
        return;

    std::ostringstream oss;

    if (!s.failed() && s.frame()){
        if (brief_) {
            oss << (s.frame()->flags() & FrameBuffer::FrameBuffer_alpha ? "RGBA, " : "RGB, ");
            oss << s.frame()->width() << " x " << s.frame()->height();
        }
        else {
            if (s.origin())
                oss << "Clone of '" << s.origin()->name() << "' " << std::endl;
            oss << (s.frame()->flags() & FrameBuffer::FrameBuffer_alpha ? "RGBA, " : "RGB, ");
            oss << std::get<2>(FrameBufferFilter::Types[s.filter()->type()]) << " filter" << std::endl;
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
    if (current_id_ == s.id())
        return;

    std::ostringstream oss;

    Pattern *ptn = s.pattern();
    if (ptn) {
        if (ptn->failed())
            oss << ptn->description() << std::endl << ptn->log();
        else {
            if (brief_) {
                oss << "RGBA, " << ptn->width() << " x " << ptn->height();
            }
            else {
                oss << Pattern::get(ptn->type()).label << " pattern" << std::endl;
                oss << "RGBA" << std::endl;
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
    if (current_id_ == s.id())
        return;

    std::ostringstream oss;

    GstToolkit::PipelineConfigSet confs = Device::manager().config( Device::manager().index(s.device()));
    if ( !confs.empty()) {
        GstToolkit::PipelineConfig best = *confs.rbegin();
        float fps = static_cast<float>(best.fps_numerator) / static_cast<float>(best.fps_denominator);

        if (brief_) {
            oss << best.stream << " " << best.format  << ", ";
            oss << best.width << " x " << best.height << ", ";
            oss << std::fixed << std::setprecision(0) << fps << "fps";
        }
        else {
            oss << s.device() << std::endl;
            oss << Device::manager().description( Device::manager().index(s.device()));
            oss << ", " << best.stream << " " << best.format  << std::endl;
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
    if (current_id_ == s.id())
        return;

    std::ostringstream oss;

    GstToolkit::PipelineConfigSet confs = ScreenCapture::manager().config( ScreenCapture::manager().index(s.window()));
    if ( !confs.empty()) {
        GstToolkit::PipelineConfig best = *confs.rbegin();
        float fps = static_cast<float>(best.fps_numerator) / static_cast<float>(best.fps_denominator);

        if (brief_) {
            oss << best.stream << " " << best.format  << ", ";
            oss << best.width << " x " << best.height << ", ";
            oss << std::fixed << std::setprecision(0) << fps << "fps";
        }
        else {
            oss << s.window() << std::endl;
            oss << ScreenCapture::manager().description( ScreenCapture::manager().index(s.window()));
            oss << ", " << best.stream << " " << best.format  << std::endl;
            oss << best.width << " x " << best.height << ", ";
            oss << std::fixed << std::setprecision(1) << fps << " fps";
        }
    }
    else {
        oss << s.window() << " not available.";
    }

    information_ = oss.str();
    current_id_ = s.id();
}


void InfoVisitor::visit (NetworkSource& s)
{
    if (current_id_ == s.id())
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
                oss << "IP " << ns->serverAddress() << std::endl;
                oss << ns->resolution().x << " x " << ns->resolution().y;
            }
            else {
                oss << s.connection() << std::endl;
                oss << NetworkToolkit::stream_protocol_label[ns->protocol()];
                oss << " shared from IP " << ns->serverAddress() << std::endl;
                oss << ns->resolution().x << " x " << ns->resolution().y;
            }
        }
    }
    else
        oss << "Undefined";

    information_ = oss.str();
    current_id_ = s.id();
}


void InfoVisitor::visit (MultiFileSource& s)
{
    if (current_id_ == s.id())
        return;

    std::ostringstream oss;

    if (s.stream()) {
        if (s.stream()->failed()) {
            oss << s.sequence().location << std::endl << s.stream()->log();
        }
        else {
            if (brief_) {
                oss << s.sequence().max - s.sequence().min + 1 << " images [";
                oss << s.sequence().min << " - " << s.sequence().max << "]" << std::endl;
                oss << s.sequence().codec << ", ";
                oss << s.sequence().width << " x " << s.sequence().height;
            }
            else {
                oss << s.sequence().location << " [";
                oss << s.sequence().min << " - " << s.sequence().max << "]" << std::endl;
                oss << s.sequence().max - s.sequence().min + 1 << " ";
                oss << s.sequence().codec << " images" << std::endl;
                oss << s.sequence().width << " x " << s.sequence().height << ", " << s.framerate() << " fps";
            }
        }
    }

    information_ = oss.str();
    current_id_ = s.id();
}

void InfoVisitor::visit (GenericStreamSource& s)
{
    if (current_id_ == s.id())
        return;

    std::ostringstream oss;

    Stream *stm = s.stream();
    if (stm) {
        if (stm->failed()) {
            oss << stm->log();
        }
        else {
            std::string src_element = s.gstElements().front();

            if (brief_) {
                src_element = src_element.substr(0, src_element.find(" "));
                oss << "gstreamer '" <<  src_element << "'" << std::endl;
                oss << "RGBA, " << stm->width() << " x " << stm->height();
            }
            else {
                oss << "gstreamer '" <<  src_element << "'" << std::endl;
                oss << "RGBA" << std::endl;
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
    if (current_id_ == s.id())
        return;

    std::ostringstream oss;

    Stream *stm = s.stream();
    if (stm) {
        if (stm->failed()) {
            oss << s.uri() << std::endl << stm->log();
        }
        else if (stm->isOpen()) {
            if (brief_) {
                oss << s.uri() << std::endl;
                oss << "Connected." << std::endl;
            }
            else {
                oss << "SRT Receiver " << s.uri() << std::endl;
                oss << "H264 (" << stm->decoderName() << ")" ;
                oss << stm->width() << " x " << stm->height();
            }
        }
        else {
            if (brief_) {
                oss << s.uri() << std::endl;
                oss << "Trying to connect..." << std::endl;
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
    if (current_id_ == s.id())
        return;

    std::ostringstream oss;

    TextContents *ptn = s.contents();
    if (ptn) {
        if (ptn->failed())
            oss << ptn->description() << std::endl << ptn->log();
        else {
            if (brief_) {
                oss << "RGBA, " << ptn->width() << " x " << ptn->height();
            }
            else {
                oss << (ptn->isSubtitle() ? "Subtitle " : "Free text") << std::endl;
                oss << "RGBA" << std::endl;
                oss << ptn->width() << " x " << ptn->height();
            }
        }
    }
    else
        oss << "Undefined";

    information_ = oss.str();
    current_id_ = s.id();
}
