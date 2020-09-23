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

//    std::string desc = "v4l2src ! video/x-raw,width=320,height=240,framerate=30/1 ! videoconvert";
//    std::string desc = "v4l2src ! jpegdec ! videoconvert";
    std::string desc = "v4l2src ! image/jpeg,width=640,height=480,framerate=30/1 ! jpegdec ! videoscale ! videoconvert";

//        std::string desc = "v4l2src ! jpegdec ! videoscale ! videoconvert";

//    std::string desc = "videotestsrc pattern=snow is-live=true ";
//    std::string desc = "ximagesrc endx=800 endy=600 ! video/x-raw,framerate=15/1 ! videoscale ! videoconvert";

    // (private) open stream
    Stream::open(desc);
}

DeviceSource::DeviceSource() : StreamSource()
{
    // create stream
    stream_ = (Stream *) new Device();

    // set icons TODO
    overlays_[View::MIXING]->attach( new Symbol(Symbol::EMPTY, glm::vec3(0.8f, 0.8f, 0.01f)) );
    overlays_[View::LAYER]->attach( new Symbol(Symbol::EMPTY, glm::vec3(0.8f, 0.8f, 0.01f)) );
}

void DeviceSource::setDevice(int id)
{
    Log::Notify("Creating Source with device '%d'", id);

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
