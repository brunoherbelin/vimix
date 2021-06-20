#include "InfoVisitor.h"

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


InfoVisitor::InfoVisitor() : brief_(true), current_id_(0)
{
}

void InfoVisitor::visit(Node &n)
{
}

void InfoVisitor::visit(Group &n)
{
}

void InfoVisitor::visit(Switch &n)
{
}

void InfoVisitor::visit(Scene &n)
{
}

void InfoVisitor::visit(Primitive &n)
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

    }
    else {

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
        oss << s.session()->frame()->width() << " x " << s.session()->frame()->height() << ", ";
        oss << "RGB";
    }
    else {
        oss << s.path() << std::endl;
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
    if (brief_) {
        oss << s.session()->numSource() << " sources in group" << std::endl;
        oss << s.session()->frame()->width() << " x " << s.session()->frame()->height() << ", ";
        oss << "RGB";
    }
    else {
        oss << s.session()->numSource() << " sources in group" << std::endl;
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

    information_ = "Rendering Output";
    current_id_ = s.id();
}

void InfoVisitor::visit (CloneSource& s)
{
    if (current_id_ == s.id())
        return;

    information_ = "Clone of " + s.origin()->name();
    current_id_ = s.id();
}

void InfoVisitor::visit (PatternSource& s)
{
    if (current_id_ == s.id())
        return;

    std::ostringstream oss;
    if (brief_) {
        oss << s.pattern()->width() << " x " << s.pattern()->height();
        oss << ", RGB";
    }
    else {
        oss << Pattern::pattern_types[s.pattern()->type()] << std::endl;
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
