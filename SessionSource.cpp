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
#include "Mixer.h"


void SessionSource::loadSession(const std::string& filename, SessionSource *source)
{
    source->loadFinished_ = false;

    // actual loading of xml file
    SessionCreator creator( source->session_ );

    if (creator.load(filename)) {
        // all ok, validate session filename
        source->session_->setFilename(filename);
    }
    else {
        // error loading
        Log::Notify("Failed to load Session file %s.", filename.c_str());
        source->loadFailed_ = true;
    }

    // end thread
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
    // delete surface
    delete sessionsurface_;

    // delete session
    if (session_)
        delete session_;
}

void SessionSource::load(const std::string &p)
{
    path_ = p;

    // launch a thread to load the session
    std::thread ( SessionSource::loadSession, path_, this).detach();

    Log::Notify("Opening %s", p.c_str());
}

Session *SessionSource::detach()
{
    // remember pointer to give away
    Session *giveaway = session_;

    // work on a new session
    session_ = new Session;

    // make disabled
    initialized_ = false;
    loadFailed_ = true;

    return giveaway;
}

bool SessionSource::failed() const
{
    return loadFailed_;
}

uint SessionSource::texture() const
{
    return session_->frame()->texture();
}

void SessionSource::init()
{
    if ( loadFinished_ && !loadFailed_ ) {
        loadFinished_ = false;

        // set resolution
        session_->setResolution( session_->config(View::RENDERING)->scale_ );

        // update once
        session_->update(dt_);

        // get the texture index from framebuffer of session, apply it to the surface
        sessionsurface_->setTextureIndex( session_->frame()->texture() );

        // create Frame buffer matching size of session
        FrameBuffer *renderbuffer = new FrameBuffer( session_->frame()->resolution());

        // set the renderbuffer of the source and attach rendering nodes
        attach(renderbuffer);

        // icon in mixing view
        overlays_[View::MIXING]->attach( new Mesh("mesh/icon_vimix.ply") );

        // done init
        initialized_ = true;

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

        // render the sesion into frame buffer
        static glm::mat4 projection = glm::ortho(-1.f, 1.f, -1.f, 1.f, -1.f, 1.f);
        renderbuffer_->begin();
        sessionsurface_->draw(glm::identity<glm::mat4>(), projection);
        renderbuffer_->end();
    }
}

void SessionSource::accept(Visitor& v)
{
    Source::accept(v);
    v.visit(*this);
}


RenderSource::RenderSource() : Source()
{
    // create  surface:
    sessionsurface_ = new Surface(rendershader_);
}

RenderSource::~RenderSource()
{
    // delete surface
    delete sessionsurface_;
}

bool RenderSource::failed() const
{
    return Mixer::manager().session() == nullptr;
}

uint RenderSource::texture() const
{
    return Mixer::manager().session()->frame()->texture();
}

void RenderSource::init()
{
    Session *session = Mixer::manager().session();
    if (session && session->frame()->texture() != Resource::getTextureBlack()) {

        // get the texture index from framebuffer of view, apply it to the surface
        sessionsurface_->setTextureIndex( session->frame()->texture() );

        // create Frame buffer matching size of output session
        FrameBuffer *renderbuffer = new FrameBuffer( session->frame()->resolution());

        // set the renderbuffer of the source and attach rendering nodes
        attach(renderbuffer);

        // icon in mixing view
        overlays_[View::MIXING]->attach( new Mesh("mesh/icon_render.ply") );

        // done init
        initialized_ = true;

        Log::Info("Source Render linked to session (%d x %d).", int(session->frame()->resolution().x), int(session->frame()->resolution().y) );
    }
}

void RenderSource::render()
{
    if (!initialized_)
        init();
    else {
        // render the view into frame buffer
        static glm::mat4 projection = glm::ortho(-1.f, 1.f, -1.f, 1.f, -1.f, 1.f);
        renderbuffer_->begin();
        sessionsurface_->draw(glm::identity<glm::mat4>(), projection);
        renderbuffer_->end();
    }
}


void RenderSource::accept(Visitor& v)
{
    Source::accept(v);
    v.visit(*this);
}
