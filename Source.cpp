
#include <algorithm>
#include <glm/gtc/matrix_transform.hpp>

#include "Source.h"

#include "defines.h"
#include "FrameBuffer.h"
#include "Primitives.h"
#include "Decorations.h"
#include "Mesh.h"
#include "Resource.h"
#include "Session.h"
#include "SearchVisitor.h"
#include "ImageShader.h"
#include "ImageProcessingShader.h"
#include "Log.h"

Source::Source() : initialized_(false), need_update_(true)
{
    sprintf(initials_, "__");
    name_ = "Source";
    mode_ = Source::HIDDEN;

    // create groups and overlays for each view

    // default rendering node
    groups_[View::RENDERING] = new Group;
    groups_[View::RENDERING]->visible_ = false;

    // default mixing nodes
    groups_[View::MIXING] = new Group;
    groups_[View::MIXING]->visible_ = false;
    groups_[View::MIXING]->scale_ = glm::vec3(0.15f, 0.15f, 1.f);
    groups_[View::MIXING]->translation_ = glm::vec3(-1.f, 1.f, 0.f);

    frames_[View::MIXING] = new Switch;
    Frame *frame = new Frame(Frame::ROUND_THIN);
    frame->translation_.z = 0.1;
    frame->color = glm::vec4( COLOR_DEFAULT_SOURCE, 0.7f);
    frames_[View::MIXING]->attach(frame);
    frame = new Frame(Frame::ROUND_LARGE);
    frame->translation_.z = 0.01;
    frame->color = glm::vec4( COLOR_HIGHLIGHT_SOURCE, 1.f);
    frames_[View::MIXING]->attach(frame);
    groups_[View::MIXING]->attach(frames_[View::MIXING]);

    overlays_[View::MIXING] = new Group;
    overlays_[View::MIXING]->translation_.z = 0.1;
    overlays_[View::MIXING]->visible_ = false;
    Icon *center = new Icon(Icon::GENERIC, glm::vec3(0.f, 0.f, 0.1f));
    overlays_[View::MIXING]->attach(center);
    groups_[View::MIXING]->attach(overlays_[View::MIXING]);

    // default geometry nodes
    groups_[View::GEOMETRY] = new Group;
    groups_[View::GEOMETRY]->visible_ = false;

    frames_[View::GEOMETRY] = new Switch;
    frame = new Frame(Frame::SHARP_THIN);
    frame->translation_.z = 0.1;
    frame->color = glm::vec4( COLOR_DEFAULT_SOURCE, 0.7f);
    frames_[View::GEOMETRY]->attach(frame);
    frame = new Frame(Frame::SHARP_LARGE);
    frame->translation_.z = 0.1;
    frame->color = glm::vec4( COLOR_HIGHLIGHT_SOURCE, 1.f);
    frames_[View::GEOMETRY]->attach(frame);
    groups_[View::GEOMETRY]->attach(frames_[View::GEOMETRY]);

    overlays_[View::GEOMETRY] = new Group;
    overlays_[View::GEOMETRY]->translation_.z = 0.15;
    overlays_[View::GEOMETRY]->visible_ = false;
    resize_handle_ = new Handles(Handles::RESIZE);
    resize_handle_->color = glm::vec4( COLOR_HIGHLIGHT_SOURCE, 1.f);
    resize_handle_->translation_.z = 0.1;
    overlays_[View::GEOMETRY]->attach(resize_handle_);
    resize_H_handle_ = new Handles(Handles::RESIZE_H);
    resize_H_handle_->color = glm::vec4( COLOR_HIGHLIGHT_SOURCE, 1.f);
    resize_H_handle_->translation_.z = 0.1;
    overlays_[View::GEOMETRY]->attach(resize_H_handle_);
    resize_V_handle_ = new Handles(Handles::RESIZE_V);
    resize_V_handle_->color = glm::vec4( COLOR_HIGHLIGHT_SOURCE, 1.f);
    resize_V_handle_->translation_.z = 0.1;
    overlays_[View::GEOMETRY]->attach(resize_V_handle_);
    rotate_handle_ = new Handles(Handles::ROTATE);
    rotate_handle_->color = glm::vec4( COLOR_HIGHLIGHT_SOURCE, 1.f);
    rotate_handle_->translation_.z = 0.1;
    overlays_[View::GEOMETRY]->attach(rotate_handle_);
    groups_[View::GEOMETRY]->attach(overlays_[View::GEOMETRY]);

    // default layer nodes
    groups_[View::LAYER] = new Group;
    groups_[View::LAYER]->visible_ = false;

    frames_[View::LAYER] = new Switch;
    frame = new Frame(Frame::ROUND_SHADOW);
    frame->translation_.z = 0.1;
    frame->color = glm::vec4( COLOR_DEFAULT_SOURCE, 0.8f);    
    frames_[View::LAYER]->attach(frame);
    frame = new Frame(Frame::ROUND_LARGE);
    frame->translation_.z = 0.1;
    frame->color = glm::vec4( COLOR_HIGHLIGHT_SOURCE, 1.f);
    frames_[View::LAYER]->attach(frame);
    groups_[View::LAYER]->attach(frames_[View::LAYER]);

    overlays_[View::LAYER] = new Group;
    overlays_[View::LAYER]->translation_.z = 0.15;
    overlays_[View::LAYER]->visible_ = false;
    groups_[View::LAYER]->attach(overlays_[View::LAYER]);

    // will be associated to nodes later
    blendingshader_ = new ImageShader;
    rendershader_ = new ImageProcessingShader;
    renderbuffer_ = nullptr;
    rendersurface_ = nullptr;

}

Source::~Source()
{
    // delete render objects
    if (renderbuffer_)
        delete renderbuffer_;

    // all groups and their children are deleted in the scene
    // this includes rendersurface_, overlays, blendingshader_ and rendershader_
    delete groups_[View::RENDERING];
    delete groups_[View::MIXING];
    delete groups_[View::GEOMETRY];
    delete groups_[View::LAYER];

    groups_.clear();
    frames_.clear();
    overlays_.clear();

    for (auto it = clones_.begin(); it != clones_.end(); it++)
        (*it)->origin_ = nullptr;
}

void Source::setName (const std::string &name)
{
    name_ = name;

    initials_[0] = std::toupper( name_.front() );
    initials_[1] = std::toupper( name_.back() );
}

void Source::accept(Visitor& v)
{
    v.visit(*this);
}


Source::Mode Source::mode() const
{
    return mode_;
}

void Source::setMode(Source::Mode m)
{
    mode_ = m;

    bool visible = mode_ != Source::HIDDEN;
    for (auto g = groups_.begin(); g != groups_.end(); g++)
        (*g).second->visible_ = visible;

    uint index_frame = mode_ == Source::NORMAL ? 0 : 1;
    for (auto f = frames_.begin(); f != frames_.end(); f++)
        (*f).second->setActive(index_frame);

    bool current = mode_ == Source::CURRENT;
    for (auto o = overlays_.begin(); o != overlays_.end(); o++)
        (*o).second->visible_ = current;

}

void fix_ar(Node *n)
{
    n->scale_.y = n->scale_.x;
}

void Source::attach(FrameBuffer *renderbuffer)
{
    renderbuffer_ = renderbuffer;

    // create the surfaces to draw the frame buffer in the views
    // TODO Provide the source custom effect shader
    rendersurface_ = new FrameBufferSurface(renderbuffer_, blendingshader_);
    groups_[View::RENDERING]->attach(rendersurface_);
    groups_[View::GEOMETRY]->attach(rendersurface_);
    groups_[View::MIXING]->attach(rendersurface_);
//    groups_[View::LAYER]->attach(rendersurface_);

    // for mixing view, add another surface to overlay (for stippled view in transparency)
    Surface *surfacemix = new FrameBufferSurface(renderbuffer_);
    ImageShader *is = static_cast<ImageShader *>(surfacemix->shader());
    if (is)  is->stipple = 1.0;
    groups_[View::MIXING]->attach(surfacemix);
    groups_[View::LAYER]->attach(surfacemix);

    // scale all mixing nodes to match aspect ratio of the media
    for (NodeSet::iterator node = groups_[View::MIXING]->begin();
         node != groups_[View::MIXING]->end(); node++) {
        (*node)->scale_.x = renderbuffer_->aspectRatio();
    }
    for (NodeSet::iterator node = groups_[View::GEOMETRY]->begin();
         node != groups_[View::GEOMETRY]->end(); node++) {
        (*node)->scale_.x = renderbuffer_->aspectRatio();
    }
    for (NodeSet::iterator node = groups_[View::LAYER]->begin();
         node != groups_[View::LAYER]->end(); node++) {
        (*node)->scale_.x = renderbuffer_->aspectRatio();
    }


    // test update callback
//    groups_[View::GEOMETRY]->update_callbacks_.push_front(fix_ar);

    setMode(Source::NORMAL);
}

void Source::update(float dt)
{
    // keep delta-t
    dt_ = dt;

    // update nodes if needed
    if (need_update_)
    {
        // ADJUST alpha based on MIXING node
        // read position of the mixing node and interpret this as transparency of render output
        glm::vec2 dist = glm::vec2(groups_[View::MIXING]->translation_);
        float alpha = 1.0 - CLAMP( ( dist.x * dist.x ) + ( dist.y * dist.y ), 0.f, 1.f );
        blendingshader_->color.a = alpha;

        // MODIFY geometry based on GEOMETRY node
        groups_[View::RENDERING]->translation_ = groups_[View::GEOMETRY]->translation_;
        groups_[View::RENDERING]->rotation_ = groups_[View::GEOMETRY]->rotation_;
        // avoid any null scale
        glm::vec3 s = groups_[View::GEOMETRY]->scale_;
        s.x = CLAMP_SCALE(s.x);
        s.y = CLAMP_SCALE(s.y);
        s.z = 1.f;
        groups_[View::GEOMETRY]->scale_ = s;
        groups_[View::RENDERING]->scale_ = s;

        // MODIFY depth based on LAYER node
        groups_[View::MIXING]->translation_.z = groups_[View::LAYER]->translation_.z;
        groups_[View::GEOMETRY]->translation_.z = groups_[View::LAYER]->translation_.z;
        groups_[View::RENDERING]->translation_.z = groups_[View::LAYER]->translation_.z;

        need_update_ = false;
    }
}

FrameBuffer *Source::frame() const
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

bool Source::contains(Node *node) const
{
    if ( node == nullptr )
        return false;

    hasNode tester(node);
    return tester(this);
}


bool hasNode::operator()(const Source* elem) const
{
    if (_n && elem)
    {
        SearchVisitor sv(_n);
        elem->group(View::MIXING)->accept(sv);
        if (sv.found())
            return true;
        elem->group(View::GEOMETRY)->accept(sv);
        if (sv.found())
            return true;
        elem->group(View::LAYER)->accept(sv);
        if (sv.found())
            return true;
        elem->group(View::RENDERING)->accept(sv);
        return sv.found();
    }

    return false;
}

CloneSource *Source::clone()
{
    CloneSource *s = new CloneSource(this);

    clones_.push_back(s);

    return s;
}

CloneSource::CloneSource(Source *origin) : Source(), origin_(origin)
{
    // create surface:
    clonesurface_ = new Surface(rendershader_);
}

CloneSource::~CloneSource()
{
    // delete surface
    delete clonesurface_;
}

CloneSource *CloneSource::clone()
{
    // do not clone a clone : clone the original instead
    return origin_->clone();
}

void CloneSource::init()
{
    if (origin_ && origin_->texture() != Resource::getTextureBlack()) {

        // get the texture index from framebuffer of view, apply it to the surface
        clonesurface_->setTextureIndex( origin_->texture() );

        // create Frame buffer matching size of session
        FrameBuffer *renderbuffer = new FrameBuffer( origin_->frame()->resolution(), true);

        // set the renderbuffer of the source and attach rendering nodes
        attach(renderbuffer);

        // icon in mixing view
        overlays_[View::MIXING]->attach( new Icon(Icon::CLONE, glm::vec3(0.8f, 0.8f, 0.01f)) );
        overlays_[View::LAYER]->attach( new Icon(Icon::CLONE, glm::vec3(0.8f, 0.8f, 0.01f)) );

        // done init
        initialized_ = true;

        Log::Info("Source Clone linked to source %s).", origin_->name().c_str() );
    }
}

void CloneSource::render()
{
    if (!initialized_)
        init();
    else {
        // render the view into frame buffer
        static glm::mat4 projection = glm::ortho(-1.f, 1.f, 1.f, -1.f, -1.f, 1.f);
        renderbuffer_->begin();
        clonesurface_->draw(glm::identity<glm::mat4>(), projection);
        renderbuffer_->end();
    }
}

void CloneSource::accept(Visitor& v)
{
    Source::accept(v);
    v.visit(*this);
}

