
#include <algorithm>
#include <glm/gtc/matrix_transform.hpp>

#include "Source.h"

#include "defines.h"
#include "FrameBuffer.h"
#include "Decorations.h"
#include "Resource.h"
#include "Session.h"
#include "SearchVisitor.h"
#include "ImageShader.h"
#include "ImageProcessingShader.h"
#include "Log.h"
#include "Mixer.h"

Source::Source() : initialized_(false), active_(true), need_update_(true), symbol_(nullptr)
{
    // create unique id
    id_ = GlmToolkit::uniqueId();

    sprintf(initials_, "__");
    name_ = "Source";
    mode_ = Source::UNINITIALIZED;

    // create groups and overlays for each view

    // default rendering node
    groups_[View::RENDERING] = new Group;
    groups_[View::RENDERING]->visible_ = false;

    // default mixing nodes
    groups_[View::MIXING] = new Group;
    groups_[View::MIXING]->visible_ = false;
    groups_[View::MIXING]->scale_ = glm::vec3(0.15f, 0.15f, 1.f);
    groups_[View::MIXING]->translation_ = glm::vec3(DEFAULT_MIXING_TRANSLATION, 0.f);

    frames_[View::MIXING] = new Switch;
    Frame *frame = new Frame(Frame::ROUND, Frame::THIN, Frame::DROP);
    frame->translation_.z = 0.1;
    frame->color = glm::vec4( COLOR_DEFAULT_SOURCE, 0.9f);
    frames_[View::MIXING]->attach(frame);
    frame = new Frame(Frame::ROUND, Frame::LARGE, Frame::DROP);
    frame->translation_.z = 0.01;
    frame->color = glm::vec4( COLOR_HIGHLIGHT_SOURCE, 1.f);
    frames_[View::MIXING]->attach(frame);
    groups_[View::MIXING]->attach(frames_[View::MIXING]);

    overlays_[View::MIXING] = new Group;
    overlays_[View::MIXING]->translation_.z = 0.1;
    overlays_[View::MIXING]->visible_ = false;
    Symbol *center = new Symbol(Symbol::CIRCLE_POINT, glm::vec3(0.f, 0.f, 0.1f));
    overlays_[View::MIXING]->attach(center);
    groups_[View::MIXING]->attach(overlays_[View::MIXING]);

    // default geometry nodes
    groups_[View::GEOMETRY] = new Group;
    groups_[View::GEOMETRY]->visible_ = false;

    frames_[View::GEOMETRY] = new Switch;
    frame = new Frame(Frame::SHARP, Frame::THIN, Frame::NONE);
    frame->translation_.z = 0.1;
    frame->color = glm::vec4( COLOR_DEFAULT_SOURCE, 0.7f);
    frames_[View::GEOMETRY]->attach(frame);
    frame = new Frame(Frame::SHARP, Frame::LARGE, Frame::GLOW);
    frame->translation_.z = 0.1;
    frame->color = glm::vec4( COLOR_HIGHLIGHT_SOURCE, 1.f);
    frames_[View::GEOMETRY]->attach(frame);
    groups_[View::GEOMETRY]->attach(frames_[View::GEOMETRY]);

    overlays_[View::GEOMETRY] = new Group;
    overlays_[View::GEOMETRY]->translation_.z = 0.15;
    overlays_[View::GEOMETRY]->visible_ = false;
    handles_[View::GEOMETRY][Handles::RESIZE] = new Handles(Handles::RESIZE);
    handles_[View::GEOMETRY][Handles::RESIZE]->color = glm::vec4( COLOR_HIGHLIGHT_SOURCE, 1.f);
    handles_[View::GEOMETRY][Handles::RESIZE]->translation_.z = 0.1;
    overlays_[View::GEOMETRY]->attach(handles_[View::GEOMETRY][Handles::RESIZE]);
    handles_[View::GEOMETRY][Handles::RESIZE_H] = new Handles(Handles::RESIZE_H);
    handles_[View::GEOMETRY][Handles::RESIZE_H]->color = glm::vec4( COLOR_HIGHLIGHT_SOURCE, 1.f);
    handles_[View::GEOMETRY][Handles::RESIZE_H]->translation_.z = 0.1;
    overlays_[View::GEOMETRY]->attach(handles_[View::GEOMETRY][Handles::RESIZE_H]);
    handles_[View::GEOMETRY][Handles::RESIZE_V] = new Handles(Handles::RESIZE_V);
    handles_[View::GEOMETRY][Handles::RESIZE_V]->color = glm::vec4( COLOR_HIGHLIGHT_SOURCE, 1.f);
    handles_[View::GEOMETRY][Handles::RESIZE_V]->translation_.z = 0.1;
    overlays_[View::GEOMETRY]->attach(handles_[View::GEOMETRY][Handles::RESIZE_V]);
    handles_[View::GEOMETRY][Handles::ROTATE] = new Handles(Handles::ROTATE);
    handles_[View::GEOMETRY][Handles::ROTATE]->color = glm::vec4( COLOR_HIGHLIGHT_SOURCE, 1.f);
    handles_[View::GEOMETRY][Handles::ROTATE]->translation_.z = 0.1;
    overlays_[View::GEOMETRY]->attach(handles_[View::GEOMETRY][Handles::ROTATE]);
    handles_[View::GEOMETRY][Handles::SCALE] = new Handles(Handles::SCALE);
    handles_[View::GEOMETRY][Handles::SCALE]->color = glm::vec4( COLOR_HIGHLIGHT_SOURCE, 1.f);
    handles_[View::GEOMETRY][Handles::SCALE]->translation_.z = 0.1;
    overlays_[View::GEOMETRY]->attach(handles_[View::GEOMETRY][Handles::SCALE]);    
    handles_[View::GEOMETRY][Handles::MENU] = new Handles(Handles::MENU);
    handles_[View::GEOMETRY][Handles::MENU]->color = glm::vec4( COLOR_HIGHLIGHT_SOURCE, 1.f);
    handles_[View::GEOMETRY][Handles::MENU]->translation_.z = 0.1;
    overlays_[View::GEOMETRY]->attach(handles_[View::GEOMETRY][Handles::MENU]);

    frame = new Frame(Frame::SHARP, Frame::THIN, Frame::NONE);
    frame->translation_.z = 0.1;
    frame->color = glm::vec4( COLOR_HIGHLIGHT_SOURCE, 0.7f);
    overlays_[View::GEOMETRY]->attach(frame);
    groups_[View::GEOMETRY]->attach(overlays_[View::GEOMETRY]);

    // default layer nodes
    groups_[View::LAYER] = new Group;
    groups_[View::LAYER]->visible_ = false;
    groups_[View::LAYER]->translation_.z = -1.f;

    frames_[View::LAYER] = new Switch;
    frame = new Frame(Frame::ROUND, Frame::THIN, Frame::PERSPECTIVE);
    frame->translation_.z = 0.1;
    frame->color = glm::vec4( COLOR_DEFAULT_SOURCE, 0.8f);    
    frames_[View::LAYER]->attach(frame);
    frame = new Frame(Frame::ROUND, Frame::LARGE, Frame::PERSPECTIVE);
    frame->translation_.z = 0.1;
    frame->color = glm::vec4( COLOR_HIGHLIGHT_SOURCE, 1.f);
    frames_[View::LAYER]->attach(frame);
    groups_[View::LAYER]->attach(frames_[View::LAYER]);

    overlays_[View::LAYER] = new Group;
    overlays_[View::LAYER]->translation_.z = 0.15;
    overlays_[View::LAYER]->visible_ = false;
    groups_[View::LAYER]->attach(overlays_[View::LAYER]);

    // default appearance node
    groups_[View::APPEARANCE] = new Group;
    groups_[View::APPEARANCE]->visible_ = false;

    frames_[View::APPEARANCE] = new Switch;
    frame = new Frame(Frame::SHARP, Frame::THIN, Frame::NONE);
    frame->translation_.z = 0.1;
    frame->color = glm::vec4( COLOR_APPEARANCE_SOURCE, 0.7f);
    frames_[View::APPEARANCE]->attach(frame);
    frame = new Frame(Frame::SHARP, Frame::LARGE, Frame::NONE);
    frame->translation_.z = 0.1;
    frame->color = glm::vec4( COLOR_APPEARANCE_SOURCE, 1.f);
    frames_[View::APPEARANCE]->attach(frame);
    groups_[View::APPEARANCE]->attach(frames_[View::APPEARANCE]);

    overlays_[View::APPEARANCE] = new Group;
    overlays_[View::APPEARANCE]->translation_.z = 0.1;
    overlays_[View::APPEARANCE]->visible_ = false;
    handles_[View::APPEARANCE][Handles::RESIZE] = new Handles(Handles::RESIZE);
    handles_[View::APPEARANCE][Handles::RESIZE]->color = glm::vec4( COLOR_APPEARANCE_SOURCE, 1.f);
    handles_[View::APPEARANCE][Handles::RESIZE]->translation_.z = 0.1;
    overlays_[View::APPEARANCE]->attach(handles_[View::APPEARANCE][Handles::RESIZE]);
    handles_[View::APPEARANCE][Handles::RESIZE_H] = new Handles(Handles::RESIZE_H);
    handles_[View::APPEARANCE][Handles::RESIZE_H]->color = glm::vec4( COLOR_APPEARANCE_SOURCE, 1.f);
    handles_[View::APPEARANCE][Handles::RESIZE_H]->translation_.z = 0.1;
    overlays_[View::APPEARANCE]->attach(handles_[View::APPEARANCE][Handles::RESIZE_H]);
    handles_[View::APPEARANCE][Handles::RESIZE_V] = new Handles(Handles::RESIZE_V);
    handles_[View::APPEARANCE][Handles::RESIZE_V]->color = glm::vec4( COLOR_APPEARANCE_SOURCE, 1.f);
    handles_[View::APPEARANCE][Handles::RESIZE_V]->translation_.z = 0.1;
    overlays_[View::APPEARANCE]->attach(handles_[View::APPEARANCE][Handles::RESIZE_V]);
//    handles_[View::APPEARANCE][Handles::ROTATE] = new Handles(Handles::ROTATE);
//    handles_[View::APPEARANCE][Handles::ROTATE]->color = glm::vec4( COLOR_APPEARANCE_SOURCE, 1.f);
//    handles_[View::APPEARANCE][Handles::ROTATE]->translation_.z = 0.1;
//    overlays_[View::APPEARANCE]->attach(handles_[View::APPEARANCE][Handles::ROTATE]);
    handles_[View::APPEARANCE][Handles::SCALE] = new Handles(Handles::SCALE);
    handles_[View::APPEARANCE][Handles::SCALE]->color = glm::vec4( COLOR_APPEARANCE_SOURCE, 1.f);
    handles_[View::APPEARANCE][Handles::SCALE]->translation_.z = 0.1;
    overlays_[View::APPEARANCE]->attach(handles_[View::APPEARANCE][Handles::SCALE]);
    handles_[View::APPEARANCE][Handles::MENU] = new Handles(Handles::MENU);
    handles_[View::APPEARANCE][Handles::MENU]->color = glm::vec4( COLOR_APPEARANCE_SOURCE, 1.f);
    handles_[View::APPEARANCE][Handles::MENU]->translation_.z = 0.1;
    overlays_[View::APPEARANCE]->attach(handles_[View::APPEARANCE][Handles::MENU]);
    groups_[View::APPEARANCE]->attach(overlays_[View::APPEARANCE]);

    // empty transition node
    groups_[View::TRANSITION] = new Group;

    // create objects
    stored_status_  = new Group;

    // those will be associated to nodes later
    blendingshader_ = new ImageShader;
    processingshader_   = new ImageProcessingShader;
    // default to image processing enabled
    renderingshader_ = (Shader *) processingshader_;

    // create media surface:
    // - textured with original texture from media player
    // - crop & repeat UV can be managed here
    // - additional custom shader can be associated
    texturesurface_ = new Surface(renderingshader_);

    // will be created at init
    renderbuffer_   = nullptr;
    rendersurface_  = nullptr;

}


Source::~Source()
{
    // inform clones that they lost their origin
    for (auto it = clones_.begin(); it != clones_.end(); it++)
        (*it)->detach();
    clones_.clear();

    // delete objects
    delete stored_status_;
    if (renderbuffer_)
        delete renderbuffer_;

    // all groups and their children are deleted in the scene
    // this includes rendersurface_, overlays, blendingshader_ and rendershader_
    delete groups_[View::RENDERING];
    delete groups_[View::MIXING];
    delete groups_[View::GEOMETRY];
    delete groups_[View::LAYER];
    delete groups_[View::APPEARANCE];
    delete groups_[View::TRANSITION];

    groups_.clear();
    frames_.clear();
    overlays_.clear();

    // don't forget that the processing shader
    // could be created but not used
    if ( renderingshader_ != processingshader_ )
        delete processingshader_;

    delete texturesurface_;
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
    // make visible on first time
    if ( mode_ == Source::UNINITIALIZED ) {
        for (auto g = groups_.begin(); g != groups_.end(); g++)
            (*g).second->visible_ = true;
    }

    // choose frame if selected
    uint index_frame = m == Source::VISIBLE ? 0 : 1;
    for (auto f = frames_.begin(); f != frames_.end(); f++)
        (*f).second->setActive(index_frame);

    // show overlay if current
    bool current = m == Source::CURRENT;
    for (auto o = overlays_.begin(); o != overlays_.end(); o++)
        (*o).second->visible_ = current;

    // show in appearance view if current
    groups_[View::APPEARANCE]->visible_ = m > Source::VISIBLE;

    mode_ = m;
}


void Source::setImageProcessingEnabled (bool on)
{
    // avoid repeating
    if ( on == imageProcessingEnabled() )
        return;

    // set pointer
    if (on) {
        // set the current rendering shader to be the
        // (previously prepared) processing shader
        renderingshader_ = (Shader *) processingshader_;
    }
    else {
        // clone the current Image processing shader
        //  (because the one currently attached to the source
        //   will be deleted in replaceRenderngShader().)
        ImageProcessingShader *tmp = new ImageProcessingShader(*processingshader_);
        // loose reference to current processing shader (to delete)
        // and keep reference to the newly created one
        // and keep it for later
        processingshader_ = tmp;
        // set the current rendering shader to a simple one
        renderingshader_ = (Shader *) new ImageShader;
    }

    // apply to nodes in subclasses
    // this calls replaceShader() on the Primitive and
    // will delete the previously attached shader
    texturesurface_->replaceShader(renderingshader_);
}

bool Source::imageProcessingEnabled()
{
    return ( renderingshader_ == processingshader_ );
}

void Source::render()
{
    if (!initialized_)
        init();
    else {
        // render the view into frame buffer
        renderbuffer_->begin();
        texturesurface_->draw(glm::identity<glm::mat4>(), renderbuffer_->projection());
        renderbuffer_->end();
    }
}


void Source::attach(FrameBuffer *renderbuffer)
{
    renderbuffer_ = renderbuffer;

    // if a symbol is available, add it to icons
    if (symbol_) {
        overlays_[View::MIXING]->attach( symbol_ );
        overlays_[View::LAYER]->attach( symbol_ );
    }

    // create the surfaces to draw the frame buffer in the views
    rendersurface_ = new FrameBufferSurface(renderbuffer_, blendingshader_);
    groups_[View::RENDERING]->attach(rendersurface_);
    groups_[View::GEOMETRY]->attach(rendersurface_);
    groups_[View::MIXING]->attach(rendersurface_);
//    groups_[View::LAYER]->attach(rendersurface_);

    // for mixing and layer views, add another surface to overlay
    // (stippled view on top with transparency)
    Surface *surfacemix = new FrameBufferSurface(renderbuffer_);
    ImageShader *is = static_cast<ImageShader *>(surfacemix->shader());
    if (is)  is->stipple = 1.0;
    groups_[View::MIXING]->attach(surfacemix);
    groups_[View::LAYER]->attach(surfacemix);

    // for appearance view, a dedicated surface without blending
    Surface *surfacetmp = new Surface();
    surfacetmp->setTextureIndex(Resource::getTextureTransparent());
    groups_[View::APPEARANCE]->attach(surfacetmp);

    // Transition group node is optionnal
    if ( groups_[View::TRANSITION]->numChildren() > 0 ) {
        groups_[View::TRANSITION]->attach(rendersurface_);
        groups_[View::TRANSITION]->attach(surfacemix);
    }

    // scale all icon nodes to match aspect ratio
    for (int v = View::MIXING; v < View::INVALID; v++) {
        NodeSet::iterator node;
        for (node = groups_[(View::Mode) v]->begin();
             node != groups_[(View::Mode) v]->end(); node++) {
            (*node)->scale_.x = renderbuffer_->aspectRatio();
        }
    }

    // make the source visible
    if ( mode_ == UNINITIALIZED ) {
        setMode(VISIBLE);
        need_update_ = true;
    }
}


void Source::setActive (bool on)
{
    active_ = on;

    for(auto clone = clones_.begin(); clone != clones_.end(); clone++) {
        if ( (*clone)->active() )
            active_ = true;
    }

    groups_[View::RENDERING]->visible_ = active_;
    groups_[View::GEOMETRY]->visible_ = active_;
    groups_[View::LAYER]->visible_ = active_;

}
// Transfer functions from coordinates to alpha (1 - transparency)
float linear_(float x, float y) {
    return 1.f - CLAMP( sqrt( ( x * x ) + ( y * y ) ), 0.f, 1.f );
}

float quad_(float x, float y) {
    return 1.f - CLAMP( ( x * x ) + ( y * y ), 0.f, 1.f );
}

float sin_quad(float x, float y) {
    return 0.5f + 0.5f * cos( M_PI * CLAMP( ( ( x * x ) + ( y * y ) ), 0.f, 1.f ) );
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
        // use the prefered transfer function
        blendingshader_->color.a = sin_quad( dist.x, dist.y );

        // CHANGE update status based on limbo
        bool a = glm::length(dist) < 1.3f;
        setActive( a );
        groups_[View::MIXING]->scale_ = glm::vec3(MIXING_ICON_SCALE) - ( a ? glm::vec3(0.f, 0.f, 0.f) : glm::vec3(0.03f, 0.03f, 0.f) );

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

        // MODIFY texture projection based on APPEARANCE node
        glm::vec3 center = groups_[View::APPEARANCE]->translation_;
        if (renderbuffer_)
            center.x /= renderbuffer_->aspectRatio();
        // convert upper left (UL [-1, 1]) and bottom right (BR [1, -1]) corners to UV [0 0 1 1]
        glm::vec3 UL = glm::vec3(-1.f, 1.f, 0.f) - center;
        glm::vec3 BR = glm::vec3(1.f, -1.f, 0.f) - center;
        UL /= groups_[View::APPEARANCE]->scale_;
        BR /= groups_[View::APPEARANCE]->scale_;
        glm::vec4 uv;
        uv.x = UL.x * 0.5f + 0.5f; // recenter
        uv.y = UL.y * -0.5f + 0.5f;
        uv.z = BR.x * 0.5f + 0.5f;
        uv.w = BR.y * -0.5f + 0.5f;
        // apply UV
        texturesurface_->setTextureUV(uv);

        // MODIFY CROP
        if (renderbuffer_) {
            glm::vec2 crop = renderbuffer_->projectionArea();
            groups_[View::MIXING]->scale_.x *= crop.x / crop.y;
            groups_[View::LAYER]->scale_.x = crop.x / crop.y;
        }

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


bool Source::hasNode::operator()(const Source* elem) const
{
    if (_n && elem)
    {
        // quick case (most frequent and easy to answer)
        if (elem->rendersurface_ == _n)
            return true;

        // general case: traverse tree of all Groups recursively using a SearchVisitor
        SearchVisitor sv(_n);
        // search in groups for all views
        for (auto g = elem->groups_.begin(); g != elem->groups_.end(); g++) {
            (*g).second->accept(sv);
            if (sv.found())
                return true;
        }
        // search in overlays for all views
        for (auto g = elem->overlays_.begin(); g != elem->overlays_.end(); g++) {
            (*g).second->accept(sv);
            if (sv.found())
                return true;
        }
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
    // set symbol
    symbol_ = new Symbol(Symbol::CLONE, glm::vec3(0.8f, 0.8f, 0.01f));
}

CloneSource::~CloneSource()
{
    if (origin_)
        origin_->clones_.remove(this);
}

CloneSource *CloneSource::clone()
{
    // do not clone a clone : clone the original instead
    if (origin_)
        return origin_->clone();
    else
        return nullptr;
}

void CloneSource::init()
{
    if (origin_ && origin_->ready()) {

        // get the texture index from framebuffer of view, apply it to the surface
        texturesurface_->setTextureIndex( origin_->texture() );

        // create Frame buffer matching size of session
        FrameBuffer *renderbuffer = new FrameBuffer( origin_->frame()->resolution(), true);

        // set the renderbuffer of the source and attach rendering nodes
        attach(renderbuffer);

        // done init
        initialized_ = true;

        Log::Info("Source %s cloning source %s.", name().c_str(), origin_->name().c_str() );
    }
}

void CloneSource::setActive (bool on)
{
    active_ = on;

    groups_[View::RENDERING]->visible_ = active_;
    groups_[View::GEOMETRY]->visible_ = active_;
    groups_[View::LAYER]->visible_ = active_;

    if (initialized_ && origin_ != nullptr)
        origin_->touch();
}


uint CloneSource::texture() const
{
    if (initialized_ && origin_ != nullptr)
        return origin_->texture();
    else
        return Resource::getTextureBlack();
}

void CloneSource::accept(Visitor& v)
{
    Source::accept(v);
    if (!failed())
        v.visit(*this);
}

