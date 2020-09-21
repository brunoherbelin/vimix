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

    single_frame_ = false;
    live_ = true;

//    std::string desc = "v4l2src ! video/x-raw,width=320,height=240,framerate=30/1 ! videoconvert";
//    std::string desc = "v4l2src ! jpegdec ! videoconvert";
//    std::string desc = "v4l2src ! image/jpeg,width=640,height=480,framerate=30/1 ! jpegdec ! videoconvert";

//    std::string desc = "videotestsrc pattern=snow is-live=true ";
    std::string desc = "ximagesrc endx=640 endy=480 ! video/x-raw,framerate=5/1 ! videoconvert ! queue";

    // (private) open stream
    open(desc);
}

void Device::open(std::string gstreamer_description)
{
    // set gstreamer pipeline source
    description_ = gstreamer_description;

    // close before re-openning
    if (isOpen())
        close();

    execute_open();
}


DeviceSource::DeviceSource() : Source()
{
    // create stream
    stream_ = new Device();

    // create surface
    devicesurface_ = new Surface(renderingshader_);
}

DeviceSource::~DeviceSource()
{
    // delete media surface & stream
    delete devicesurface_;
    delete stream_;
}

bool DeviceSource::failed() const
{
    return stream_->failed();
}

uint DeviceSource::texture() const
{
    return stream_->texture();
}

void DeviceSource::replaceRenderingShader()
{
    devicesurface_->replaceShader(renderingshader_);
}

void DeviceSource::setDevice(int id)
{
    Log::Notify("Openning device %d", id);

    stream_->open(id);
    stream_->play(true);
}


void DeviceSource::init()
{
    if ( stream_->isOpen() ) {

        // update video
        stream_->update();

        // once the texture of media player is created
        if (stream_->texture() != Resource::getTextureBlack()) {

            // get the texture index from media player, apply it to the media surface
            devicesurface_->setTextureIndex( stream_->texture() );

            // create Frame buffer matching size of media player
            float height = float(stream_->width()) / stream_->aspectRatio();
            FrameBuffer *renderbuffer = new FrameBuffer(stream_->width(), (uint)height, true);

            // set the renderbuffer of the source and attach rendering nodes
            attach(renderbuffer);

            // icon in mixing view
            overlays_[View::MIXING]->attach( new Symbol(Symbol::EMPTY, glm::vec3(0.8f, 0.8f, 0.01f)) );
            overlays_[View::LAYER]->attach( new Symbol(Symbol::EMPTY, glm::vec3(0.8f, 0.8f, 0.01f)) );

            // done init
            initialized_ = true;
            Log::Info("Source Device linked to Stream %d.", stream_->description().c_str());

            // force update of activation mode
            active_ = true;
            touch();
        }
    }

}

void DeviceSource::setActive (bool on)
{
    bool was_active = active_;

    Source::setActive(on);

    // change status of media player (only if status changed)
    if ( active_ != was_active ) {
        stream_->enable(active_);
    }
}

void DeviceSource::update(float dt)
{
    Source::update(dt);

    // update stream
    stream_->update();
}

void DeviceSource::render()
{
    if (!initialized_)
        init();
    else {
        // render the media player into frame buffer
        static glm::mat4 projection = glm::ortho(-1.f, 1.f, 1.f, -1.f, -1.f, 1.f);
        renderbuffer_->begin();
        devicesurface_->draw(glm::identity<glm::mat4>(), projection);
        renderbuffer_->end();
    }
}

void DeviceSource::accept(Visitor& v)
{
    Source::accept(v);
    v.visit(*this);
}
