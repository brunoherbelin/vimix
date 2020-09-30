#include <sstream>
#include <glm/gtc/matrix_transform.hpp>

#include "StreamSource.h"

#include "defines.h"
#include "ImageShader.h"
#include "Resource.h"
#include "Decorations.h"
#include "Stream.h"
#include "Visitor.h"
#include "Log.h"


GenericStreamSource::GenericStreamSource() : StreamSource()
{
    // create stream
    stream_ = new Stream;

    // icon in mixing view
    overlays_[View::MIXING]->attach( new Symbol(Symbol::EMPTY, glm::vec3(0.8f, 0.8f, 0.01f)) );
    overlays_[View::LAYER]->attach( new Symbol(Symbol::EMPTY, glm::vec3(0.8f, 0.8f, 0.01f)) );
}

void GenericStreamSource::setDescription(const std::string &desc)
{
    Log::Notify("Creating Source with Stream description '%s'", desc.c_str());

    stream_->open(desc);
    stream_->play(true);
}

void GenericStreamSource::accept(Visitor& v)
{
    Source::accept(v);
    v.visit(*this);
}

StreamSource::StreamSource() : Source()
{
    // create surface
    surface_ = new Surface(renderingshader_);
}

StreamSource::~StreamSource()
{
    // delete media surface & stream
    delete surface_;
    delete stream_;
}

bool StreamSource::failed() const
{
    return stream_->failed();
}

uint StreamSource::texture() const
{
    return stream_->texture();
}

void StreamSource::replaceRenderingShader()
{
    surface_->replaceShader(renderingshader_);
}


void StreamSource::init()
{
    if ( stream_->isOpen() ) {

        // update video
        stream_->update();

        // once the texture of media player is created
        if (stream_->texture() != Resource::getTextureBlack()) {

            // get the texture index from media player, apply it to the media surface
            surface_->setTextureIndex( stream_->texture() );

            // create Frame buffer matching size of media player
            float height = float(stream_->width()) / stream_->aspectRatio();
            FrameBuffer *renderbuffer = new FrameBuffer(stream_->width(), (uint)height, true);

            // set the renderbuffer of the source and attach rendering nodes
            attach(renderbuffer);

            // done init
            initialized_ = true;
            Log::Info("Source '%s' linked to Stream %d.", name().c_str(), stream_->id());

            // force update of activation mode
            active_ = true;
            touch();
        }
    }

}

void StreamSource::setActive (bool on)
{
    bool was_active = active_;

    Source::setActive(on);

    // change status of media player (only if status changed)
    if ( active_ != was_active ) {
        stream_->enable(active_);
    }
}

void StreamSource::update(float dt)
{
    Source::update(dt);

    // update stream
    stream_->update();
}

void StreamSource::render()
{
    if (!initialized_)
        init();
    else {
        // render the media player into frame buffer
        static glm::mat4 projection = glm::ortho(-1.f, 1.f, 1.f, -1.f, -1.f, 1.f);
        renderbuffer_->begin();
        surface_->draw(glm::identity<glm::mat4>(), projection);
        renderbuffer_->end();
    }
}
