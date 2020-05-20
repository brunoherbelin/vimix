#include <glm/gtc/matrix_transform.hpp>
#include <thread>
#include <chrono>

#include "SessionSource.h"

#include "defines.h"
#include "Log.h"
#include "FrameBuffer.h"
#include "ImageShader.h"
#include "ImageProcessingShader.h"
#include "Resource.h"
#include "Primitives.h"
#include "Mesh.h"
#include "SearchVisitor.h"
#include "Session.h"
#include "SessionCreator.h"


void SessionSource::loadSession(const std::string& filename, SessionSource *source)
{
    source->loadFinished_ = false;

    // actual loading of xml file
    SessionCreator creator( source->session_ );

    if (!creator.load(filename)) {
        // error loading
        Log::Notify("Failed to load Session file %s.", filename.c_str());
        source->loadFailed_ = true;
    }

    source->loadFinished_ = true;
}

SessionSource::SessionSource() : Source(), path_("")
{
    loadFailed_ = false;
    loadFinished_ = false;

    session_ = new Session;

    // create surface:
    // - textured with original texture from session
    // - crop & repeat UV can be managed here
    // - additional custom shader can be associated
    sessionsurface_ = new Surface(rendershader_);
}

SessionSource::~SessionSource()
{
    // TODO delete media surface with visitor
    delete sessionsurface_;

    // TODO close and delete session
    delete session_;
}

void SessionSource::setPath(const std::string &p)
{
    path_ = p;

    // launch a thread to load the session
    std::thread ( SessionSource::loadSession, path_, this).detach();

    Log::Notify("Opening %s", p.c_str());
}

std::string SessionSource::path() const
{
    return path_;
}

Session *SessionSource::session() const
{
    return session_;
}

bool SessionSource::failed() const
{
    return loadFailed_;
}

void SessionSource::init()
{
    if ( loadFinished_ && !loadFailed_ ) {
        loadFinished_ = false;

        // set resolution
        session_->setResolution( session_->config(View::RENDERING)->scale_ );

        // get the texture index from media player, apply it to the media surface
        sessionsurface_->setTextureIndex( session_->frame()->texture() );

        // create Frame buffer matching size of media player
        renderbuffer_ = new FrameBuffer(session_->frame()->resolution());

        // create the surfaces to draw the frame buffer in the views
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
        // TODO icon session
//            overlays_[View::MIXING]->attach( new Mesh("mesh/icon_video.ply") );

        // scale all mixing nodes to match aspect ratio of the media
        for (NodeSet::iterator node = groups_[View::MIXING]->begin();
             node != groups_[View::MIXING]->end(); node++) {
            (*node)->scale_.x = session_->frame()->aspectRatio();
        }
        for (NodeSet::iterator node = groups_[View::GEOMETRY]->begin();
             node != groups_[View::GEOMETRY]->end(); node++) {
            (*node)->scale_.x = session_->frame()->aspectRatio();
        }
        for (NodeSet::iterator node = groups_[View::LAYER]->begin();
             node != groups_[View::LAYER]->end(); node++) {
            (*node)->scale_.x = session_->frame()->aspectRatio();
        }

        // done init once and for all
        initialized_ = true;

        // make visible
        groups_[View::RENDERING]->visible_ = true;
        groups_[View::MIXING]->visible_ = true;
        groups_[View::GEOMETRY]->visible_ = true;
        groups_[View::LAYER]->visible_ = true;

        Log::Info("Source Session %s loading %d sources.", path_.c_str(), session_->numSource());
    }
}

void SessionSource::render()
{
    if (!initialized_)
        init();
    else {
        // update session
        session_->update(dt_);

        sessionsurface_->setTextureIndex( session_->frame()->texture() );

        // render the media player into frame buffer
        static glm::mat4 projection = glm::ortho(-1.f, 1.f, -1.f, 1.f, -1.f, 1.f);
        renderbuffer_->begin();
        sessionsurface_->draw(glm::identity<glm::mat4>(), projection);
        renderbuffer_->end();
    }
}

FrameBuffer *SessionSource::frame() const
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

void SessionSource::accept(Visitor& v)
{
    Source::accept(v);
    v.visit(*this);
}

