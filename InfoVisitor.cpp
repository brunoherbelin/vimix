/*
 * This file is part of vimix - video live mixer
 *
 * **Copyright** (C) 2019-2022 Bruno Herbelin <bruno.herbelin@gmail.com>
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
#include <algorithm>
#include <sstream>
#include <iomanip>

#include <glm/glm.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <glm/gtc/constants.hpp>
#include <glm/gtc/matrix_access.hpp>

#include <tinyxml2.h>
#include "tinyxml2Toolkit.h"

#include "defines.h"
#include "Log.h"
#include "Scene.h"
#include "Primitives.h"
#include "ImageShader.h"
#include "ImageProcessingShader.h"
#include "MediaPlayer.h"
#include "MediaSource.h"
#include "CloneSource.h"
#include "RenderSource.h"
#include "SessionSource.h"
#include "PatternSource.h"
#include "DeviceSource.h"
#include "NetworkSource.h"
#include "SrtReceiverSource.h"
#include "MultiFileSource.h"
#include "SessionCreator.h"
#include "SessionVisitor.h"
#include "Settings.h"
#include "Mixer.h"
#include "ActionManager.h"
#include "BaseToolkit.h"
#include "UserInterfaceManager.h"
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

    if (!brief_) {
        oss << n.crop_.x << ", " << n.crop_.y << " Crop" << std::endl;
    }

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
    if (brief_) {
        oss << SystemToolkit::filename(mp.filename()) << std::endl;
        oss << mp.media().codec_name.substr(0, mp.media().codec_name.find_first_of(" (,")) << ", ";
        oss << mp.width() << " x " << mp.height();
        if (!mp.isImage())
            oss << ", " << std::fixed << std::setprecision(1) << mp.frameRate() << " fps";
    }
    else {
        oss << mp.filename() << std::endl;
        oss << mp.media().codec_name << std::endl;
        oss << mp.width() << " x " << mp.height() ;
        if (!mp.isImage())
            oss << ", " << std::fixed << std::setprecision(1) << mp.frameRate() << " fps";
    }

    information_ = oss.str();

    // remember (except if codec was not identified yet)
    if ( !mp.media().codec_name.empty() )
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

void InfoVisitor::visit (SessionFileSource& s)
{
    if (current_id_ == s.id() || s.session() == nullptr)
        return;

    std::ostringstream oss;
    if (s.session()->frame()){        
        std::string numsource = std::to_string(s.session()->numSource()) + " source" + (s.session()->numSource()>1 ? "s" : "");
        if (brief_) {
            oss << SystemToolkit::filename(s.path()) << std::endl;
            oss << numsource << ", " ;
            oss << "RGB, " << s.session()->frame()->width() << " x " << s.session()->frame()->height();
        }
        else {
            oss << s.path() << std::endl;
            oss << "MIX session (" << numsource << "), RGB" << std::endl;
            oss << s.session()->frame()->width() << " x " << s.session()->frame()->height();
        }
    }

    information_ = oss.str();
    current_id_ = s.id();
}

void InfoVisitor::visit (SessionGroupSource& s)
{
    if (current_id_ == s.id() || s.session() == nullptr)
        return;

    std::ostringstream oss;
    if (s.session()->frame()){
        std::string numsource = std::to_string(s.session()->numSource()) + " source" + (s.session()->numSource()>1 ? "s" : "");
        if (brief_) {
            oss << numsource << ", " ;
            oss << "RGB, " << s.session()->frame()->width() << " x " << s.session()->frame()->height();
        }
        else {
            oss << "Group of " << numsource << std::endl;
            oss << "RGB" << std::endl;
            oss << s.session()->frame()->width() << " x " << s.session()->frame()->height();
        }
    }

    information_ = oss.str();
    current_id_ = s.id();
}

void InfoVisitor::visit (RenderSource& s)
{
    if (current_id_ == s.id())
        return;

    std::ostringstream oss;

    if (s.frame()){
        if (brief_) {
            oss << (s.frame()->use_alpha() ? "RGBA, " : "RGB, ");
            oss << s.frame()->width() << " x " << s.frame()->height();
        }
        else {
            oss << "Rendering Output (";
            oss << RenderSource::rendering_provenance_label[s.renderingProvenance()] << ") " << std::endl;
            oss << (s.frame()->use_alpha() ? "RGBA" : "RGB") << std::endl;
            oss << s.frame()->width() << " x " << s.frame()->height();
        }
    }

    information_ = oss.str();
    current_id_ = s.id();
}

void InfoVisitor::visit (CloneSource& s)
{
    if (current_id_ == s.id() || s.origin() == nullptr)
        return;

    std::ostringstream oss;

    if (s.frame()){
        if (brief_) {
            oss << (s.frame()->use_alpha() ? "RGBA, " : "RGB, ");
            oss << s.frame()->width() << " x " << s.frame()->height();
        }
        else {
            if (s.origin())
                oss << "Clone of '" << s.origin()->name() << "' ";
            oss << CloneSource::cloning_provenance_label[s.cloningProvenance()] << std::endl;
            oss << (s.frame()->use_alpha() ? "RGBA, " : "RGB, ");
            oss << (int)(s.delay()*1000.0) << " ms delay " << std::endl;
            oss << s.frame()->width() << " x " << s.frame()->height();
        }
    }

    information_ = oss.str();
    current_id_ = s.id();
}

void InfoVisitor::visit (PatternSource& s)
{
    if (current_id_ == s.id())
        return;

    std::ostringstream oss;
    if (s.pattern()) {
        if (brief_) {
            oss << "RGBA, " << s.pattern()->width() << " x " << s.pattern()->height();
        }
        else {
            oss << Pattern::get(s.pattern()->type()).label << " pattern" << std::endl;
            oss << "RGBA" << std::endl;
            oss << s.pattern()->width() << " x " << s.pattern()->height();
        }
    }

    information_ = oss.str();
    current_id_ = s.id();
}

void InfoVisitor::visit (DeviceSource& s)
{
    if (current_id_ == s.id())
        return;

    std::ostringstream oss;

    DeviceConfigSet confs = Device::manager().config( Device::manager().index(s.device()));
    if ( !confs.empty()) {
        DeviceConfig best = *confs.rbegin();
        float fps = static_cast<float>(best.fps_numerator) / static_cast<float>(best.fps_denominator);

        if (brief_) {
            oss << best.stream << " " << best.format  << ", ";
            oss << best.width << " x " << best.height << ", ";
            oss << std::fixed << std::setprecision(1) << fps << " fps";
        }
        else {
            oss << s.device() << std::endl;
            oss << Device::manager().description( Device::manager().index(s.device()));
            oss << ", " << best.stream << " " << best.format  << std::endl;
            oss << best.width << " x " << best.height << ", ";
            oss << std::fixed << std::setprecision(1) << fps << " fps";
        }
    }

    information_ = oss.str();
    current_id_ = s.id();
}

void InfoVisitor::visit (NetworkSource& s)
{
    if (current_id_ == s.id())
        return;

    NetworkStream *ns = s.networkStream();

    std::ostringstream oss;
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

    information_ = oss.str();
    current_id_ = s.id();
}


void InfoVisitor::visit (MultiFileSource& s)
{
    if (current_id_ == s.id())
        return;

    std::ostringstream oss;
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

    information_ = oss.str();
    current_id_ = s.id();
}

void InfoVisitor::visit (GenericStreamSource& s)
{
    if (current_id_ == s.id())
        return;

    std::ostringstream oss;
    if (s.stream()) {
        std::string src_element = s.gstElements().front();

        if (brief_) {
            src_element = src_element.substr(0, src_element.find(" "));
            oss << "gstreamer '" <<  src_element << "'" << std::endl;
            oss << "RGBA, " << s.stream()->width() << " x " << s.stream()->height();
        }
        else {
            oss << "gstreamer '" <<  src_element << "'" << std::endl;
            oss << "RGBA" << std::endl;
            oss << s.stream()->width() << " x " << s.stream()->height();
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

    if (s.stream()) {
        if (brief_)
            oss << s.uri() << std::endl;
        else {
            oss << "SRT Receiver " << s.uri() << std::endl;
            oss << "H264 (" << s.stream()->decoderName() << ")" << std::endl;
            oss << s.stream()->width() << " x " << s.stream()->height();
        }
    }
    else
        oss << "Undefined";

    information_ = oss.str();
    current_id_ = s.id();
}
