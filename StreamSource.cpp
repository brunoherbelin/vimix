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

    // set symbol
    symbol_ = new Symbol(Symbol::EMPTY, glm::vec3(0.75f, 0.75f, 0.01f));
    symbol_->scale_.y = 1.5f;
}

void GenericStreamSource::setDescription(const std::string &desc)
{
    Log::Notify("Creating Source with Stream description '%s'", desc.c_str());

    // open gstreamer
    stream_->open(desc);
    stream_->play(true);

    // will be ready after init and one frame rendered
    ready_ = false;
}

void GenericStreamSource::accept(Visitor& v)
{
    Source::accept(v);
    if (!failed())
        v.visit(*this);
}

StreamSource::StreamSource(uint64_t id) : Source(id), stream_(nullptr)
{
}

StreamSource::~StreamSource()
{
    // delete stream
    if (stream_)
        delete stream_;
}

bool StreamSource::failed() const
{
    return (stream_ != nullptr &&  stream_->failed() );
}

uint StreamSource::texture() const
{
    if (stream_ == nullptr)
        return Resource::getTextureBlack();
    else
        return stream_->texture();
}

void StreamSource::init()
{
    if ( stream_ && stream_->isOpen() ) {

        // update video
        stream_->update();

        // once the texture of media player is created
        if (stream_->texture() != Resource::getTextureBlack()) {

            // get the texture index from media player, apply it to the media surface
            texturesurface_->setTextureIndex( stream_->texture() );

            // create Frame buffer matching size of media player
            float height = float(stream_->width()) / stream_->aspectRatio();
            FrameBuffer *renderbuffer = new FrameBuffer(stream_->width(), (uint)height, true);

            // set the renderbuffer of the source and attach rendering nodes
            attach(renderbuffer);

            // force update of activation mode
            active_ = true;

            // deep update to reorder
            ++View::need_deep_update_;

            // done init
            Log::Info("Source '%s' linked to Stream %s", name().c_str(), std::to_string(stream_->id()).c_str());
        }
    }

}

void StreamSource::setActive (bool on)
{
    bool was_active = active_;

    // try to activate (may fail if source is cloned)
    Source::setActive(on);

    // change status of stream (only if status changed)
    if ( stream_ && active_ != was_active )
        stream_->enable(active_);

}


bool StreamSource::playing () const
{
    if ( stream_ )
        return stream_->isPlaying();
    return false;
}

void StreamSource::play (bool on)
{
    if ( stream_ )
        stream_->play(on);
}


bool StreamSource::playable () const
{
    if ( stream_ )
        return !stream_->singleFrame();
    return false;
}

void StreamSource::replay ()
{
    if ( stream_ )
        stream_->rewind();
}

guint64 StreamSource::playtime () const
{
    if ( stream_ )
        return stream_->position();
    else
        return 0;
}

void StreamSource::update(float dt)
{
    Source::update(dt);

    // update stream
    if (stream_)
        stream_->update();
}
