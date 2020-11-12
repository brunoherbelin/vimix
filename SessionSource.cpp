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
#include "Decorations.h"
#include "SearchVisitor.h"
#include "Session.h"
#include "SessionCreator.h"
#include "Mixer.h"


SessionSource::SessionSource() : Source(), path_("")
{
    // specific node for transition view
    groups_[View::TRANSITION]->visible_ = false;
    groups_[View::TRANSITION]->scale_ = glm::vec3(0.1f, 0.1f, 1.f);
    groups_[View::TRANSITION]->translation_ = glm::vec3(-1.f, 0.f, 0.f);

    frames_[View::TRANSITION] = new Switch;
    Frame *frame = new Frame(Frame::ROUND, Frame::THIN, Frame::DROP);
    frame->translation_.z = 0.1;
    frame->color = glm::vec4( COLOR_DEFAULT_SOURCE, 0.9f);
    frames_[View::TRANSITION]->attach(frame);
    frame = new Frame(Frame::ROUND, Frame::LARGE, Frame::DROP);
    frame->translation_.z = 0.01;
    frame->color = glm::vec4( COLOR_TRANSITION_SOURCE, 1.f);
    frames_[View::TRANSITION]->attach(frame);
    groups_[View::TRANSITION]->attach(frames_[View::TRANSITION]);

    overlays_[View::TRANSITION] = new Group;
    overlays_[View::TRANSITION]->translation_.z = 0.1;
    overlays_[View::TRANSITION]->visible_ = false;

    Symbol *loader = new Symbol(Symbol::DOTS);
    loader->scale_ = glm::vec3(2.f, 2.f, 1.f);
    loader->update_callbacks_.push_back(new InfiniteGlowCallback);
    overlays_[View::TRANSITION]->attach(loader);
    Symbol *center = new Symbol(Symbol::CIRCLE_POINT, glm::vec3(0.f, -1.05f, 0.1f));
    overlays_[View::TRANSITION]->attach(center);
    groups_[View::TRANSITION]->attach(overlays_[View::TRANSITION]);

    failed_ = false;
    wait_for_sources_ = false;
    session_ = nullptr;

    // create surface:
    // - textured with original texture from session
    // - crop & repeat UV can be managed here
    // - additional custom shader can be associated
    surface_ = new Surface(renderingshader_);
}

SessionSource::~SessionSource()
{
    // delete surface
    delete surface_;

    // delete session
    if (session_)
        delete session_;
}

void SessionSource::load(const std::string &p)
{
    path_ = p;

    if ( path_.empty() )
        // empty session
        session_ = new Session;
    else
        // launch a thread to load the session file
        sessionLoader_ = std::async(std::launch::async, Session::load, path_);

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
    failed_ = true;

    return giveaway;
}

bool SessionSource::failed() const
{
    return failed_;
}

uint SessionSource::texture() const
{
    if (session_ == nullptr)
        return Resource::getTextureBlack();
    return session_->frame()->texture();
}

void SessionSource::replaceRenderingShader()
{
    surface_->replaceShader(renderingshader_);
}

void SessionSource::init()
{
    // init is first about getting the loaded session
    if (session_ == nullptr) {
        // did the loader finish ?
        if (sessionLoader_.wait_for(std::chrono::milliseconds(4)) == std::future_status::ready) {
            session_ = sessionLoader_.get();
            if (session_ == nullptr)
                failed_ = true;
        }
    }
    else {

        if (wait_for_sources_) {

            // force update of of all sources
            active_ = true;
            touch();

            // check that every source is ready..
            bool ready = true;
            for (SourceList::iterator iter = session_->begin(); iter != session_->end(); iter++)
            {
                // interrupt if any source is NOT ready
                if ( !(*iter)->ready() ){
                    ready = false;
                    break;
                }
            }
            // if all sources are ready, done with initialization!
            if (ready) {
                // done init
                wait_for_sources_ = false;
                initialized_ = true;
                Log::Info("Source Session %s loaded %d sources.", path_.c_str(), session_->numSource());
            }
        }
        else if ( !failed_ ) {

            // set resolution
            session_->setResolution( session_->config(View::RENDERING)->scale_ );

            // deep update once to draw framebuffer
            View::need_deep_update_ = true;
            session_->update(dt_);

            // get the texture index from framebuffer of session, apply it to the surface
            surface_->setTextureIndex( session_->frame()->texture() );

            // create Frame buffer matching size of session
            FrameBuffer *renderbuffer = new FrameBuffer( session_->frame()->resolution());

            // set the renderbuffer of the source and attach rendering nodes
            attach(renderbuffer);

            // icon in mixing view
            overlays_[View::MIXING]->attach( new Symbol(Symbol::SESSION, glm::vec3(0.8f, 0.8f, 0.01f)) );
            overlays_[View::LAYER]->attach( new Symbol(Symbol::SESSION, glm::vec3(0.8f, 0.8f, 0.01f)) );

            // wait for all sources to init
            if (session_->numSource() > 0)
                wait_for_sources_ = true;
            else {
                initialized_ = true;
                Log::Info("New Session created.");
            }
        }
    }

    if (initialized_){
        // remove the loading icon
        Node *loader = overlays_[View::TRANSITION]->back();
        overlays_[View::TRANSITION]->detatch(loader);
        delete loader;
    }
}

void SessionSource::setActive (bool on)
{
    Source::setActive(on);

    // change status of session (recursive change of internal sources)
    if (session_ != nullptr)
        session_->setActive(active_);
}


void SessionSource::update(float dt)
{
    if (session_ == nullptr)
        return;

    // update content
    if (active_)
        session_->update(dt);

    // delete a source which failed
    if (session_->failedSource() != nullptr) {
        session_->deleteSource(session_->failedSource());
        // fail session if all sources failed
        if ( session_->numSource() < 1)
            failed_ = true;
    }

    Source::update(dt);
}

void SessionSource::render()
{
    if (!initialized_)
        init();
    else {
        // render the sesion into frame buffer
        static glm::mat4 projection = glm::ortho(-1.f, 1.f, 1.f, -1.f, -1.f, 1.f);
        renderbuffer_->begin();
        surface_->draw(glm::identity<glm::mat4>(), projection);
        renderbuffer_->end();
    }
}

void SessionSource::accept(Visitor& v)
{
    Source::accept(v);
    if (!failed())
        v.visit(*this);
}


RenderSource::RenderSource(Session *session) : Source(), session_(session)
{
    // create  surface:
    surface_ = new Surface(processingshader_);
}

RenderSource::~RenderSource()
{
    // delete surface
    delete surface_;
}

bool RenderSource::failed() const
{
    return session_ == nullptr;
}

uint RenderSource::texture() const
{
    if (session_ == nullptr)
        return Resource::getTextureBlack();
    else
        return session_->frame()->texture();
}

void RenderSource::replaceRenderingShader()
{
    surface_->replaceShader(renderingshader_);
}

void RenderSource::init()
{
    if (session_ == nullptr)
        return;

    if (session_ && session_->frame()->texture() != Resource::getTextureBlack()) {

        FrameBuffer *fb = session_->frame();

        // get the texture index from framebuffer of view, apply it to the surface
        surface_->setTextureIndex( fb->texture() );

        // create Frame buffer matching size of output session
        FrameBuffer *renderbuffer = new FrameBuffer( fb->resolution());

        // set the renderbuffer of the source and attach rendering nodes
        attach(renderbuffer);

        // icon in mixing view
        overlays_[View::MIXING]->attach( new Symbol(Symbol::RENDER, glm::vec3(0.8f, 0.8f, 0.01f)) );
        overlays_[View::LAYER]->attach( new Symbol(Symbol::RENDER, glm::vec3(0.8f, 0.8f, 0.01f)) );

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
        surface_->draw(glm::identity<glm::mat4>(), projection);
        renderbuffer_->end();
    }
}


void RenderSource::accept(Visitor& v)
{
    Source::accept(v);
    if (!failed())
        v.visit(*this);
}
