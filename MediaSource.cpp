#include <glm/gtc/matrix_transform.hpp>

#include "MediaSource.h"

#include "defines.h"
#include "ImageShader.h"
#include "ImageProcessingShader.h"
#include "Resource.h"
#include "Decorations.h"
#include "MediaPlayer.h"
#include "Visitor.h"
#include "Log.h"

MediaSource::MediaSource() : Source(), path_("")
{
    // create media player
    mediaplayer_ = new MediaPlayer;

    // create media surface:
    // - textured with original texture from media player
    // - crop & repeat UV can be managed here
    // - additional custom shader can be associated
    mediasurface_ = new Surface(renderingshader_);

}

MediaSource::~MediaSource()
{
    // delete media surface & player
    delete mediasurface_;
    delete mediaplayer_;
}

void MediaSource::setPath(const std::string &p)
{
    Log::Notify("Creating Source with media '%s'", p.c_str());

    path_ = p;
    mediaplayer_->open(path_);
    mediaplayer_->play(true);
}

std::string MediaSource::path() const
{
    return path_;
}

MediaPlayer *MediaSource::mediaplayer() const
{
    return mediaplayer_;
}

bool MediaSource::failed() const
{
    return mediaplayer_->failed();
}

uint MediaSource::texture() const
{
    return mediaplayer_->texture();
}

void MediaSource::replaceRenderingShader()
{
    mediasurface_->replaceShader(renderingshader_);
}

void MediaSource::init()
{
    if ( mediaplayer_->isOpen() ) {

        // update video
        mediaplayer_->update();

        // once the texture of media player is created
        if (mediaplayer_->texture() != Resource::getTextureBlack()) {

            // get the texture index from media player, apply it to the media surface
            mediasurface_->setTextureIndex( mediaplayer_->texture() );

            // create Frame buffer matching size of media player
            float height = float(mediaplayer_->width()) / mediaplayer_->aspectRatio();
            FrameBuffer *renderbuffer = new FrameBuffer(mediaplayer_->width(), (uint)height, true);

            // set the renderbuffer of the source and attach rendering nodes
            attach(renderbuffer);

            // icon in mixing view
            if (mediaplayer_->isImage()) {
                overlays_[View::MIXING]->attach( new Symbol(Symbol::IMAGE, glm::vec3(0.8f, 0.8f, 0.01f)) );
                overlays_[View::LAYER]->attach( new Symbol(Symbol::IMAGE, glm::vec3(0.8f, 0.8f, 0.01f)) );
            }
            else {
                overlays_[View::MIXING]->attach( new Symbol(Symbol::VIDEO, glm::vec3(0.8f, 0.8f, 0.01f)) );
                overlays_[View::LAYER]->attach( new Symbol(Symbol::VIDEO, glm::vec3(0.8f, 0.8f, 0.01f)) );
            }

            // done init
            initialized_ = true;
            Log::Info("Source '%s' linked to Media %s.", name().c_str(), mediaplayer()->uri().c_str());

            // force update of activation mode
            active_ = true;
            touch();
        }
    }

}

void MediaSource::setActive (bool on)
{
    bool was_active = active_;

    Source::setActive(on);

    // change status of media player (only if status changed)
    if ( active_ != was_active ) {
        mediaplayer_->enable(active_);
    }
}

void MediaSource::update(float dt)
{
    Source::update(dt);

    // update video
    mediaplayer_->update();
}

void MediaSource::render()
{
    if (!initialized_)
        init();
    else {
        // render the media player into frame buffer
        static glm::mat4 projection = glm::ortho(-1.f, 1.f, 1.f, -1.f, -1.f, 1.f);
        renderbuffer_->begin();
        mediasurface_->shader()->color.a = mediaplayer_->currentTimelineFading();
        mediasurface_->draw(glm::identity<glm::mat4>(), projection);
        renderbuffer_->end();
    }
}

void MediaSource::accept(Visitor& v)
{
    Source::accept(v);
    v.visit(*this);
}
