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

void InfoVisitor::visit(Node &)
{
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
        oss << mp.width() << " x " << mp.height() << ", ";
        oss << mp.media().codec_name.substr(0, mp.media().codec_name.find_first_of(" (,"));
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
    if (brief_) {
        oss << SystemToolkit::filename(s.path()) << " (";
        oss << s.session()->numSource() << " sources)" << std::endl;
    }
    else
        oss << s.path() << std::endl;

    if (s.session()->frame()){
        oss << s.session()->frame()->width() << " x " << s.session()->frame()->height() << ", ";
        oss << "RGB";
    }

    information_ = oss.str();
    current_id_ = s.id();
}

void InfoVisitor::visit (SessionGroupSource& s)
{
    if (current_id_ == s.id() || s.session() == nullptr)
        return;

    std::ostringstream oss;
    oss << s.session()->numSource() << " sources in group" << std::endl;
    if (s.session()->frame()){
        oss << s.session()->frame()->width() << " x " << s.session()->frame()->height() << ", ";
        oss << "RGB";
    }

    information_ = oss.str();
    current_id_ = s.id();
}

void InfoVisitor::visit (RenderSource& s)
{
    if (current_id_ == s.id())
        return;

    std::ostringstream oss;
    oss << "Rendering Output (" << RenderSource::render_mode_label[s.renderMode()];
    oss << ") " << std::endl;

    if (s.frame()){
        oss << s.frame()->width() << " x " << s.frame()->height() << ", ";
        if (s.frame()->use_alpha())
            oss << "RGBA";
        else
            oss << "RGB";
    }

    information_ = oss.str();
    current_id_ = s.id();
}

void InfoVisitor::visit (CloneSource& s)
{
    if (current_id_ == s.id() || s.origin() == nullptr)
        return;

    std::ostringstream oss;
    oss << "Clone of '" << s.origin()->name() << "' " << std::endl;

    if (s.frame()){
        oss << s.frame()->width() << " x " << s.frame()->height() << ", ";
        if (s.frame()->use_alpha())
            oss << "RGBA";
        else
            oss << "RGB";
    }

    information_ = oss.str();
    current_id_ = s.id();
}

void InfoVisitor::visit (PatternSource& s)
{
    if (current_id_ == s.id())
        return;

    std::ostringstream oss;
    oss << Pattern::get(s.pattern()->type()).label << std::endl;
    if (s.pattern()) {
        oss << s.pattern()->width() << " x " << s.pattern()->height();
        oss << ", RGB";
    }

    information_ = oss.str();
    current_id_ = s.id();
}

void InfoVisitor::visit (DeviceSource& s)
{
    if (current_id_ == s.id())
        return;

    std::ostringstream oss;

    DeviceConfigSet confs = Device::manager().config( Device::manager().index(s.device().c_str()));
    if ( !confs.empty()) {
        DeviceConfig best = *confs.rbegin();
        float fps = static_cast<float>(best.fps_numerator) / static_cast<float>(best.fps_denominator);

        if (brief_) {
            oss << best.width << " x " << best.height << ", ";
            oss << best.stream << " " << best.format << ", ";
            oss << std::fixed << std::setprecision(1) << fps << " fps";
        }
        else {
            oss << s.device() << std::endl;
            oss << best.width << " x " << best.height << ", ";
            oss << best.stream << " " << best.format << ", ";
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
        oss << ns->resolution().x << " x " << ns->resolution().y << ", ";
        oss << NetworkToolkit::protocol_name[ns->protocol()] << std::endl;
        oss << "IP " << ns->serverAddress();
    }
    else {
        oss << s.connection() << " (IP " << ns->serverAddress() << ")" << std::endl;
        oss << ns->resolution().x << " x " << ns->resolution().y << ", ";
        oss << NetworkToolkit::protocol_name[ns->protocol()];
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
        oss << s.sequence().width << " x " << s.sequence().height << ", ";
        oss << s.sequence().codec << std::endl;
        oss << s.sequence().max - s.sequence().min + 1 << " images [";
        oss << s.sequence().min << " - " << s.sequence().max << "]";
    }
    else {
        oss << s.sequence().location << " [";
        oss << s.sequence().min << " - " << s.sequence().max << "]" << std::endl;
        oss << s.sequence().width << " x " << s.sequence().height << ", ";
        oss << s.sequence().codec << " (";
        oss << s.sequence().max - s.sequence().min + 1 << " images)";
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
        src_element = src_element.substr(0, src_element.find(" "));
        oss << "gstreamer '" <<  src_element << "'" << std::endl;
        oss << s.stream()->width() << " x " << s.stream()->height();
        oss << ", RGB";
    }
    else
        oss << "Undefined";

    information_ = oss.str();
    current_id_ = s.id();
}
