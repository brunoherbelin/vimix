#include <sstream>
#include <glm/gtc/matrix_transform.hpp>

#include "DeviceSource.h"

#include "defines.h"
#include "ImageShader.h"
#include "ImageProcessingShader.h"
#include "Resource.h"
#include "Primitives.h"
#include "Stream.h"
#include "Visitor.h"
#include "Log.h"

Device::Device() : Stream()
{

}

glm::ivec2 Device::resolution()
{
    return glm::ivec2( width_, height_);
}


void Device::open( uint device )
{
    device_ = CLAMP(device, 0, 2);

    live_ = true;

//    std::string desc = "v4l2src ! video/x-raw,width=320,height=240,framerate=30/1 ! videoconvert";
//    std::string desc = "v4l2src ! jpegdec ! videoconvert";
//    std::string desc = "v4l2src ! image/jpeg,width=640,height=480,framerate=30/1 ! jpegdec ! videoconvert";

//    std::string desc = "videotestsrc pattern=snow is-live=true ";
    std::string desc = "ximagesrc endx=640 endy=480 ! video/x-raw,framerate=5/1 ! videoconvert ! queue";

    // (private) open stream
    Stream::open(desc);
}

DeviceSource::DeviceSource() : StreamSource()
{
    // create stream
    stream_ = (Stream *) new Device();

    // icon in mixing view
    overlays_[View::MIXING]->attach( new Symbol(Symbol::EMPTY, glm::vec3(0.8f, 0.8f, 0.01f)) );
    overlays_[View::LAYER]->attach( new Symbol(Symbol::EMPTY, glm::vec3(0.8f, 0.8f, 0.01f)) );
}

void DeviceSource::setDevice(int id)
{
    Log::Notify("Openning device %d", id);

    device()->open(id);
    stream_->play(true);
}

void DeviceSource::accept(Visitor& v)
{
    Source::accept(v);
    v.visit(*this);
}

Device *DeviceSource::device() const
{
    return dynamic_cast<Device *>(stream_);
}
