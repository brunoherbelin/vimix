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
    if (session_ == nullptr)
        return Resource::getTextureBlack();
    return session_->frame()->texture();
}

void SessionSource::init()
{
    Log::Info("SessionSource::init");

    if ( loadFinished_ && !loadFailed_ && session_ != nullptr) {
        loadFinished_ = false;

        // set resolution
        session_->setResolution( session_->config(View::RENDERING)->scale_ );

        // update once with update of the depth
        View::need_deep_update_ = true;
        session_->update(dt_);

        // get the texture index from framebuffer of session, apply it to the surface
        sessionsurface_->setTextureIndex( session_->frame()->texture() );

        // create Frame buffer matching size of session
        FrameBuffer *renderbuffer = new FrameBuffer( session_->frame()->resolution());

        // set the renderbuffer of the source and attach rendering nodes
        attach(renderbuffer);

        // icon in mixing view
        overlays_[View::MIXING]->attach( new Icon(Icon::SESSION, glm::vec3(0.8f, 0.8f, 0.01f)) );
        overlays_[View::LAYER]->attach( new Icon(Icon::SESSION, glm::vec3(0.8f, 0.8f, 0.01f)) );

        // done init
        initialized_ = true;
        Log::Info("Source Session %s loading %d sources.", path_.c_str(), session_->numSource());

        // force update of activation mode
        active_ = true;
        touch();
    }
}

void SessionSource::setActive (bool on)
{
//    Log::Info("SessionSource::setActive %d", on);

    bool was_active = active_;

    Source::setActive(on);

    // change status of media player (only if status changed)
    if ( active_ != was_active ) {
        session_->setActive(active_);
    }
}


void SessionSource::update(float dt)
{
//    Log::Info("SessionSource::update %f", dt);
    Source::update(dt);

    // update content
    session_->update(dt);

    // delete a source which failed
    if (session()->failedSource() != nullptr)
        session()->deleteSource(session()->failedSource());
}

void SessionSource::render()
{
    if (!initialized_)
        init();
    else {
        // render the sesion into frame buffer
        static glm::mat4 projection = glm::ortho(-1.f, 1.f, 1.f, -1.f, -1.f, 1.f);
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


RenderSource::RenderSource(Session *session) : Source(), session_(session)
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
    return session_ == nullptr;
}

uint RenderSource::texture() const
{
    if (session_ == nullptr)
        return Resource::getTextureBlack();
    return session_->frame()->texture();
}

void RenderSource::init()
{
    if (session_ == nullptr)
        session_ = Mixer::manager().session();

    if (session_ && session_->frame()->texture() != Resource::getTextureBlack()) {

        FrameBuffer *fb = session_->frame();

        // get the texture index from framebuffer of view, apply it to the surface
        sessionsurface_->setTextureIndex( fb->texture() );

        // create Frame buffer matching size of output session
        FrameBuffer *renderbuffer = new FrameBuffer( fb->resolution());

        // set the renderbuffer of the source and attach rendering nodes
        attach(renderbuffer);

        // icon in mixing view
        overlays_[View::MIXING]->attach( new Icon(Icon::RENDER, glm::vec3(0.8f, 0.8f, 0.01f)) );
        overlays_[View::LAYER]->attach( new Icon(Icon::RENDER, glm::vec3(0.8f, 0.8f, 0.01f)) );

        // done init
        initialized_ = true;

        Log::Info("Source Render linked to session (%d x %d).", int(fb->resolution().x), int(fb->resolution().y) );
    }
}

void RenderSource::render()
{
    if (!initialized_)
        init();
    else {
        // render the view into frame buffer
        static glm::mat4 projection = glm::ortho(-1.f, 1.f, 1.f, -1.f, -1.f, 1.f);
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
