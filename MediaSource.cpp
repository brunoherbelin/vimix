#include <glm/gtc/matrix_transform.hpp>

#include "MediaSource.h"

#include "defines.h"
#include "ImageShader.h"
#include "ImageProcessingShader.h"
#include "Resource.h"
#include "Primitives.h"
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
    mediasurface_ = new Surface(rendershader_);

}

MediaSource::~MediaSource()
{
    // TODO delete media surface with visitor
    delete mediasurface_;

    delete mediaplayer_;
    // TODO verify that all surfaces and node is deleted in Source destructor
}

void MediaSource::setPath(const std::string &p)
{
    path_ = p;
    mediaplayer_->open(path_);
    mediaplayer_->play(true);

    Log::Notify("Opening %s", p.c_str());
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
            float height = float(mediaplayer()->width()) / mediaplayer()->aspectRatio();
            renderbuffer_ = new FrameBuffer(mediaplayer()->width(), (uint)height);

            // create the surfaces to draw the frame buffer in the views
            // TODO Provide the source custom effect shader
            rendersurface_ = new FrameBufferSurface(renderbuffer_, blendingshader_);
            groups_[View::RENDERING]->attach(rendersurface_);
            groups_[View::GEOMETRY]->attach(rendersurface_);
            groups_[View::MIXING]->attach(rendersurface_);
            groups_[View::LAYER]->attach(rendersurface_);

            // for mixing view, add another surface to overlay (for stippled view in transparency)
            Surface *surfacemix = new FrameBufferSurface(renderbuffer_);
            ImageShader *is = static_cast<ImageShader *>(surfacemix->shader());
            if (is)  is->stipple = 1.0;
            groups_[View::MIXING]->attach(surfacemix);
            groups_[View::LAYER]->attach(surfacemix);
            if (mediaplayer_->duration() == GST_CLOCK_TIME_NONE)
                overlays_[View::MIXING]->attach( new Mesh("mesh/icon_image.ply") );
            else
                overlays_[View::MIXING]->attach( new Mesh("mesh/icon_video.ply") );

            // scale all mixing nodes to match aspect ratio of the media
            for (NodeSet::iterator node = groups_[View::MIXING]->begin();
                 node != groups_[View::MIXING]->end(); node++) {
                (*node)->scale_.x = mediaplayer_->aspectRatio();
            }
            for (NodeSet::iterator node = groups_[View::GEOMETRY]->begin();
                 node != groups_[View::GEOMETRY]->end(); node++) {
                (*node)->scale_.x = mediaplayer_->aspectRatio();
            }
            for (NodeSet::iterator node = groups_[View::LAYER]->begin();
                 node != groups_[View::LAYER]->end(); node++) {
                (*node)->scale_.x = mediaplayer_->aspectRatio();
            }

            // done init once and for all
            initialized_ = true;
            // make visible
            groups_[View::RENDERING]->visible_ = true;
            groups_[View::MIXING]->visible_ = true;
            groups_[View::GEOMETRY]->visible_ = true;
            groups_[View::LAYER]->visible_ = true;
        }
    }

}

void MediaSource::render()
{
    if (!initialized_)
        init();
    else {
        // update video
        mediaplayer_->update();

        // render the media player into frame buffer
        static glm::mat4 projection = glm::ortho(-1.f, 1.f, 1.f, -1.f, -1.f, 1.f);
        renderbuffer_->begin();
        mediasurface_->draw(glm::identity<glm::mat4>(), projection);
        renderbuffer_->end();
    }
}

FrameBuffer *MediaSource::frame() const
{
    if (initialized_ && renderbuffer_)
    {
        return renderbuffer_;
    }
    else {
        static FrameBuffer *black = new FrameBuffer(640,480);
        return black;
    }

}

void MediaSource::accept(Visitor& v)
{
    Source::accept(v);
    v.visit(*this);
}
