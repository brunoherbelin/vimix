/*
 * This file is part of vimix - video live mixer
 *
 * **Copyright** (C) 2019-2023 Bruno Herbelin <bruno.herbelin@gmail.com>
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program. If not, see <https://www.gnu.org/licenses/>.
**/

#include <algorithm>
#include <locale>
#include <tinyxml2.h>
#include <glm/gtc/matrix_access.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include "defines.h"
#include "FrameBuffer.h"
#include "Decorations.h"
#include "Resource.h"
#include "SearchVisitor.h"
#include "ImageShader.h"
#include "ImageProcessingShader.h"
#include "BaseToolkit.h"
#include "MixingGroup.h"
#include "SourceCallback.h"

#include "CloneSource.h"
#include "Source.h"

SourceCore::SourceCore()
{
    // default nodes
    groups_[View::RENDERING] = new Group;
    groups_[View::RENDERING]->visible_ = false;
    groups_[View::MIXING] = new Group;
    groups_[View::MIXING]->visible_ = false;
    groups_[View::GEOMETRY] = new Group;
    groups_[View::GEOMETRY]->visible_ = false;
    groups_[View::LAYER] = new Group;
    groups_[View::LAYER]->visible_ = false;
    groups_[View::TEXTURE] = new Group;
    groups_[View::TEXTURE]->visible_ = false;
    groups_[View::TRANSITION] = new Group;
    groups_[View::TRANSITION]->visible_ = false;
    // temp node
    stored_status_  = new Group;

    // filtered image shader (with texturing and processing) for rendering
    processingshader_ = new ImageProcessingShader;
    // default rendering with image processing disabled
    renderingshader_ = static_cast<Shader *>(new ImageShader);
}

SourceCore::SourceCore(SourceCore const& other) : SourceCore()
{
    copy(other);
}

SourceCore::~SourceCore()
{
    // all groups and their children are deleted in the scene
    // this deletes renderingshader_ (and all source-attached nodes
    // e.g. rendersurface_, overlays, blendingshader_, etc.)
    delete groups_[View::RENDERING];
    delete groups_[View::MIXING];
    delete groups_[View::GEOMETRY];
    delete groups_[View::LAYER];
    delete groups_[View::TEXTURE];
    delete groups_[View::TRANSITION];
    delete stored_status_;

    // don't forget that the processing shader
    // could be created but not used and not deleted above
    if ( renderingshader_ != processingshader_ )
        delete processingshader_;

    groups_.clear();
}

void SourceCore::copy(SourceCore const& other)
{
    // copy groups properties
//    groups_[View::RENDERING]->copyTransform( other.group(View::RENDERING) );
    groups_[View::MIXING]->copyTransform( other.group(View::MIXING) );
    groups_[View::GEOMETRY]->copyTransform( other.group(View::GEOMETRY) );
    groups_[View::LAYER]->copyTransform( other.group(View::LAYER) );
    groups_[View::TEXTURE]->copyTransform( other.group(View::TEXTURE) );
    groups_[View::TRANSITION]->copyTransform( other.group(View::TRANSITION) );
    stored_status_->copyTransform( other.stored_status_ );

    // copy shader properties
    processingshader_->copy(*other.processingshader_);
    renderingshader_->copy(*other.renderingshader_);
}

void SourceCore::store (View::Mode m)
{
    stored_status_->copyTransform(groups_[m]);
}

SourceCore& SourceCore::operator= (SourceCore const& other)
{
    if (this != &other)   // no self assignment
        copy(other);
    return *this;
}


Source::Source(uint64_t id) : SourceCore(), id_(id), ready_(false), symbol_(nullptr),
    active_(true), locked_(false), need_update_(SourceUpdate_None), dt_(16.f), workspace_(WORKSPACE_CENTRAL)
{
    // create unique id
    if (id_ == 0)
        id_ = BaseToolkit::uniqueId();

    snprintf(initials_, 3, "__");
    name_ = "Source";
    mode_ = Source::UNINITIALIZED;

    // create groups and overlays for each view

    // default mixing nodes
    groups_[View::MIXING]->scale_ = glm::vec3(MIXING_ICON_SCALE);
    groups_[View::MIXING]->translation_ = glm::vec3(DEFAULT_MIXING_TRANSLATION, 0.f);

    frames_[View::MIXING] = new Switch;
    Frame *frame = new Frame(Frame::ROUND, Frame::THIN, Frame::DROP); // visible
    frame->translation_.z = 0.1;
    frame->color = glm::vec4( COLOR_DEFAULT_SOURCE, 0.85f);
    frames_[View::MIXING]->attach(frame);
    frame = new Frame(Frame::ROUND, Frame::THIN, Frame::DROP);        // selected
    frame->translation_.z = 0.1;
    frame->color = glm::vec4( COLOR_HIGHLIGHT_SOURCE, 0.95f);
    frames_[View::MIXING]->attach(frame);
    frame = new Frame(Frame::ROUND, Frame::LARGE, Frame::DROP);       // current
    frame->translation_.z = 0.01;
    frame->color = glm::vec4( COLOR_HIGHLIGHT_SOURCE, 1.f);
    frames_[View::MIXING]->attach(frame);
    groups_[View::MIXING]->attach(frames_[View::MIXING]);

    // Glyphs show letters from the intials, with Font index 4 (LARGE)
    initial_0_ = new Character(4);
    initial_0_->translation_ = glm::vec3(0.2f, 0.8f, 0.1f);
    initial_0_->scale_.y =  0.2f;
    groups_[View::MIXING]->attach(initial_0_);
    initial_1_ = new Character(4);
    initial_1_->translation_ = glm::vec3(0.4f, 0.8f, 0.1f);
    initial_1_->scale_.y = 0.2f;
    groups_[View::MIXING]->attach(initial_1_);

    overlays_[View::MIXING] = new Group;
    overlays_[View::MIXING]->translation_.z = 0.1;
    overlays_[View::MIXING]->visible_ = false;
    Symbol *center = new Symbol(Symbol::CIRCLE_POINT, glm::vec3(0.f, 0.f, 0.1f));
    overlays_[View::MIXING]->attach(center);
    groups_[View::MIXING]->attach(overlays_[View::MIXING]);

    overlay_mixinggroup_ = new Switch;
    overlay_mixinggroup_->translation_.z = 0.1;
    center = new Symbol(Symbol::CIRCLE_POINT, glm::vec3(0.f, 0.f, 0.1f));
    center->scale_= glm::vec3(1.6f, 1.6f, 1.f);
    center->color = glm::vec4( COLOR_MIXING_GROUP, 0.96f);
    overlay_mixinggroup_->attach(center);
    rotation_mixingroup_ = new Symbol(Symbol::ROTATION, glm::vec3(0.f, 0.f, 0.1f));
    rotation_mixingroup_->color = glm::vec4( COLOR_MIXING_GROUP, 0.94f);
    rotation_mixingroup_->scale_ = glm::vec3(3.f, 3.f, 1.f);
    overlay_mixinggroup_->attach(rotation_mixingroup_);
    groups_[View::MIXING]->attach(overlay_mixinggroup_);

    // default geometry nodes
    frames_[View::GEOMETRY] = new Switch;
    frame = new Frame(Frame::SHARP, Frame::THIN, Frame::NONE); // visible
    frame->translation_.z = 0.1;
    frame->color = glm::vec4( COLOR_DEFAULT_SOURCE, 0.85f);
    frames_[View::GEOMETRY]->attach(frame);
    frame = new Frame(Frame::SHARP, Frame::THIN, Frame::NONE); // selected
    frame->translation_.z = 0.1;
    frame->color = glm::vec4( COLOR_HIGHLIGHT_SOURCE, 0.95f);
    frames_[View::GEOMETRY]->attach(frame);
    frame = new Frame(Frame::SHARP, Frame::LARGE, Frame::GLOW); // current
    frame->translation_.z = 0.1;
    frame->color = glm::vec4( COLOR_HIGHLIGHT_SOURCE, 1.f);
    frames_[View::GEOMETRY]->attach(frame);
    groups_[View::GEOMETRY]->attach(frames_[View::GEOMETRY]);

    overlays_[View::GEOMETRY] = new Group;
    overlays_[View::GEOMETRY]->translation_.z = 0.15;
    overlays_[View::GEOMETRY]->visible_ = false;
    // menu and manipulation switch
    handles_[View::GEOMETRY][Handles::MENU] = new Handles(Handles::MENU);
    handles_[View::GEOMETRY][Handles::MENU]->color = glm::vec4(COLOR_HIGHLIGHT_SOURCE, 1.f);
    handles_[View::GEOMETRY][Handles::MENU]->translation_.z = 0.1;
    overlays_[View::GEOMETRY]->attach(handles_[View::GEOMETRY][Handles::MENU]);

    manipulator_ = new Switch;
    overlays_[View::GEOMETRY]->attach(manipulator_);

    Group *transform_manipulator = new Group;
    manipulator_->attach(transform_manipulator);
    handles_[View::GEOMETRY][Handles::RESIZE] = new Handles(Handles::RESIZE);
    handles_[View::GEOMETRY][Handles::RESIZE]->color = glm::vec4(COLOR_HIGHLIGHT_SOURCE, 1.f);
    handles_[View::GEOMETRY][Handles::RESIZE]->translation_.z = 0.1;
    transform_manipulator->attach(handles_[View::GEOMETRY][Handles::RESIZE]);
    handles_[View::GEOMETRY][Handles::RESIZE_H] = new Handles(Handles::RESIZE_H);
    handles_[View::GEOMETRY][Handles::RESIZE_H]->color = glm::vec4(COLOR_HIGHLIGHT_SOURCE, 1.f);
    handles_[View::GEOMETRY][Handles::RESIZE_H]->translation_.z = 0.1;
    transform_manipulator->attach(handles_[View::GEOMETRY][Handles::RESIZE_H]);
    handles_[View::GEOMETRY][Handles::RESIZE_V] = new Handles(Handles::RESIZE_V);
    handles_[View::GEOMETRY][Handles::RESIZE_V]->color = glm::vec4(COLOR_HIGHLIGHT_SOURCE, 1.f);
    handles_[View::GEOMETRY][Handles::RESIZE_V]->translation_.z = 0.1;
    transform_manipulator->attach(handles_[View::GEOMETRY][Handles::RESIZE_V]);
    handles_[View::GEOMETRY][Handles::ROTATE] = new Handles(Handles::ROTATE);
    handles_[View::GEOMETRY][Handles::ROTATE]->color = glm::vec4(COLOR_HIGHLIGHT_SOURCE, 1.f);
    handles_[View::GEOMETRY][Handles::ROTATE]->translation_.z = 0.1;
    transform_manipulator->attach(handles_[View::GEOMETRY][Handles::ROTATE]);
    handles_[View::GEOMETRY][Handles::SCALE] = new Handles(Handles::SCALE);
    handles_[View::GEOMETRY][Handles::SCALE]->color = glm::vec4(COLOR_HIGHLIGHT_SOURCE, 1.f);
    handles_[View::GEOMETRY][Handles::SCALE]->translation_.z = 0.1;
    transform_manipulator->attach(handles_[View::GEOMETRY][Handles::SCALE]);
    handles_[View::GEOMETRY][Handles::EDIT_SHAPE] = new Handles(Handles::EDIT_SHAPE);
    handles_[View::GEOMETRY][Handles::EDIT_SHAPE]->color = glm::vec4(COLOR_HIGHLIGHT_SOURCE, 0.6f);
    handles_[View::GEOMETRY][Handles::EDIT_SHAPE]->translation_.z = 0.1;
    transform_manipulator->attach(handles_[View::GEOMETRY][Handles::EDIT_SHAPE]);

    Group *node_manipulator = new Group;
    manipulator_->attach(node_manipulator);
    handles_[View::GEOMETRY][Handles::NODE_LOWER_LEFT] = new Handles(Handles::NODE_LOWER_LEFT);
    handles_[View::GEOMETRY][Handles::NODE_LOWER_LEFT]->color = glm::vec4(COLOR_HIGHLIGHT_SOURCE, 1.f);
    handles_[View::GEOMETRY][Handles::NODE_LOWER_LEFT]->translation_.z = 0.1;
    node_manipulator->attach(handles_[View::GEOMETRY][Handles::NODE_LOWER_LEFT]);
    handles_[View::GEOMETRY][Handles::NODE_UPPER_LEFT] = new Handles(Handles::NODE_UPPER_LEFT);
    handles_[View::GEOMETRY][Handles::NODE_UPPER_LEFT]->color = glm::vec4(COLOR_HIGHLIGHT_SOURCE,  1.f);
    handles_[View::GEOMETRY][Handles::NODE_UPPER_LEFT]->translation_.z = 0.1;
    node_manipulator->attach(handles_[View::GEOMETRY][Handles::NODE_UPPER_LEFT]);
    handles_[View::GEOMETRY][Handles::NODE_LOWER_RIGHT] = new Handles(Handles::NODE_LOWER_RIGHT);
    handles_[View::GEOMETRY][Handles::NODE_LOWER_RIGHT]->color = glm::vec4(COLOR_HIGHLIGHT_SOURCE,  1.f);
    handles_[View::GEOMETRY][Handles::NODE_LOWER_RIGHT]->translation_.z = 0.1;
    node_manipulator->attach(handles_[View::GEOMETRY][Handles::NODE_LOWER_RIGHT]);
    handles_[View::GEOMETRY][Handles::NODE_UPPER_RIGHT] = new Handles(Handles::NODE_UPPER_RIGHT);
    handles_[View::GEOMETRY][Handles::NODE_UPPER_RIGHT]->color = glm::vec4(COLOR_HIGHLIGHT_SOURCE,  1.f);
    handles_[View::GEOMETRY][Handles::NODE_UPPER_RIGHT]->translation_.z = 0.1;
    node_manipulator->attach(handles_[View::GEOMETRY][Handles::NODE_UPPER_RIGHT]);
    handles_[View::GEOMETRY][Handles::CROP_H] = new Handles(Handles::CROP_H);
    handles_[View::GEOMETRY][Handles::CROP_H]->color = glm::vec4(COLOR_HIGHLIGHT_SOURCE, 1.f);
    handles_[View::GEOMETRY][Handles::CROP_H]->translation_.z = 0.1;
    node_manipulator->attach(handles_[View::GEOMETRY][Handles::CROP_H]);
    handles_[View::GEOMETRY][Handles::CROP_V] = new Handles(Handles::CROP_V);
    handles_[View::GEOMETRY][Handles::CROP_V]->color = glm::vec4(COLOR_HIGHLIGHT_SOURCE, 1.f);
    handles_[View::GEOMETRY][Handles::CROP_V]->translation_.z = 0.1;
    node_manipulator->attach(handles_[View::GEOMETRY][Handles::CROP_V]);
    handles_[View::GEOMETRY][Handles::ROUNDING] = new Handles(Handles::ROUNDING);
    handles_[View::GEOMETRY][Handles::ROUNDING]->color = glm::vec4(COLOR_HIGHLIGHT_SOURCE, 1.f);
    handles_[View::GEOMETRY][Handles::ROUNDING]->translation_.z = 0.1;
    node_manipulator->attach(handles_[View::GEOMETRY][Handles::ROUNDING]);
    handles_[View::GEOMETRY][Handles::EDIT_CROP] = new Handles(Handles::EDIT_CROP);
    handles_[View::GEOMETRY][Handles::EDIT_CROP]->color = glm::vec4(COLOR_HIGHLIGHT_SOURCE, 0.6f);
    handles_[View::GEOMETRY][Handles::EDIT_CROP]->translation_.z = 0.1;
    node_manipulator->attach(handles_[View::GEOMETRY][Handles::EDIT_CROP]);

    manipulator_->setActive(0);

    frame = new Frame(Frame::SHARP, Frame::THIN, Frame::NONE);
    frame->translation_.z = 0.1;
    frame->color = glm::vec4( COLOR_HIGHLIGHT_SOURCE, 0.7f);
    overlays_[View::GEOMETRY]->attach(frame);
    groups_[View::GEOMETRY]->attach(overlays_[View::GEOMETRY]);

    // default layer nodes
    groups_[View::LAYER]->translation_.z = -1.f;

    frames_[View::LAYER] = new Switch;
    frame = new Frame(Frame::ROUND, Frame::THIN, Frame::PERSPECTIVE);  // visible
    frame->translation_.z = 0.1;
    frame->color = glm::vec4( COLOR_DEFAULT_SOURCE, 0.85f);
    frames_[View::LAYER]->attach(frame);
    frame = new Frame(Frame::ROUND, Frame::THIN, Frame::PERSPECTIVE);  // selected
    frame->translation_.z = 0.1;
    frame->color = glm::vec4( COLOR_HIGHLIGHT_SOURCE, 0.95f);
    frames_[View::LAYER]->attach(frame);
    frame = new Frame(Frame::ROUND, Frame::LARGE, Frame::PERSPECTIVE); // current
    frame->translation_.z = 0.1;
    frame->color = glm::vec4( COLOR_HIGHLIGHT_SOURCE, 1.f);
    frames_[View::LAYER]->attach(frame);
    groups_[View::LAYER]->attach(frames_[View::LAYER]);

    groups_[View::LAYER]->attach(initial_0_);
    groups_[View::LAYER]->attach(initial_1_);

    overlays_[View::LAYER] = new Group;
    overlays_[View::LAYER]->translation_.z = 0.15;
    overlays_[View::LAYER]->visible_ = false;
    groups_[View::LAYER]->attach(overlays_[View::LAYER]);

    // blending change icon
    blendmode_ = new Switch;
    blendmode_->translation_ = glm::vec3(0.0f, 1.2f, 0.1f);
    blendmode_->scale_ = glm::vec3(1.2f, 1.2f, 1.f);
    groups_[View::LAYER]->attach(blendmode_);
    for (uint B = Symbol::BLEND_NORMAL; B < Symbol::EMPTY; ++B) {
        Symbol *blend_icon = new Symbol( (Symbol::Type) B);
        blend_icon->color = glm::vec4(COLOR_HIGHLIGHT_SOURCE, 0.6f);
        blend_icon->translation_.z = 0.1;
        blendmode_->attach(blend_icon);
    }
    blendmode_->setActive(0);

    // default appearance node
    frames_[View::TEXTURE] = new Switch;
    frame = new Frame(Frame::SHARP, Frame::THIN, Frame::NONE);  // visible
    frame->translation_.z = 0.1;
    frame->color = glm::vec4( COLOR_APPEARANCE_SOURCE, 0.2f);
    frames_[View::TEXTURE]->attach(frame);
    frame = new Frame(Frame::SHARP, Frame::THIN, Frame::NONE);  // selected
    frame->translation_.z = 0.1;
    frame->color = glm::vec4( COLOR_APPEARANCE_SOURCE, 0.7f);
    frames_[View::TEXTURE]->attach(frame);
    frame = new Frame(Frame::SHARP, Frame::LARGE, Frame::NONE); // current
    frame->translation_.z = 0.1;
    frame->color = glm::vec4( COLOR_APPEARANCE_SOURCE, 1.f);
    frames_[View::TEXTURE]->attach(frame);
    groups_[View::TEXTURE]->attach(frames_[View::TEXTURE]);

    overlays_[View::TEXTURE] = new Group;
    overlays_[View::TEXTURE]->translation_.z = 0.1;
    overlays_[View::TEXTURE]->visible_ = false;
    handles_[View::TEXTURE][Handles::RESIZE] = new Handles(Handles::RESIZE);
    handles_[View::TEXTURE][Handles::RESIZE]->color = glm::vec4( COLOR_APPEARANCE_SOURCE, 1.f);
    handles_[View::TEXTURE][Handles::RESIZE]->translation_.z = 0.1;
    overlays_[View::TEXTURE]->attach(handles_[View::TEXTURE][Handles::RESIZE]);
    handles_[View::TEXTURE][Handles::RESIZE_H] = new Handles(Handles::RESIZE_H);
    handles_[View::TEXTURE][Handles::RESIZE_H]->color = glm::vec4( COLOR_APPEARANCE_SOURCE, 1.f);
    handles_[View::TEXTURE][Handles::RESIZE_H]->translation_.z = 0.1;
    overlays_[View::TEXTURE]->attach(handles_[View::TEXTURE][Handles::RESIZE_H]);
    handles_[View::TEXTURE][Handles::RESIZE_V] = new Handles(Handles::RESIZE_V);
    handles_[View::TEXTURE][Handles::RESIZE_V]->color = glm::vec4( COLOR_APPEARANCE_SOURCE, 1.f);
    handles_[View::TEXTURE][Handles::RESIZE_V]->translation_.z = 0.1;
    overlays_[View::TEXTURE]->attach(handles_[View::TEXTURE][Handles::RESIZE_V]);
    handles_[View::TEXTURE][Handles::ROTATE] = new Handles(Handles::ROTATE);
    handles_[View::TEXTURE][Handles::ROTATE]->color = glm::vec4( COLOR_APPEARANCE_SOURCE, 1.f);
    handles_[View::TEXTURE][Handles::ROTATE]->translation_.z = 0.1;
    overlays_[View::TEXTURE]->attach(handles_[View::TEXTURE][Handles::ROTATE]);
    handles_[View::TEXTURE][Handles::SCALE] = new Handles(Handles::SCALE);
    handles_[View::TEXTURE][Handles::SCALE]->color = glm::vec4( COLOR_APPEARANCE_SOURCE, 1.f);
    handles_[View::TEXTURE][Handles::SCALE]->translation_.z = 0.1;
    overlays_[View::TEXTURE]->attach(handles_[View::TEXTURE][Handles::SCALE]);
    handles_[View::TEXTURE][Handles::MENU] = new Handles(Handles::MENU);
    handles_[View::TEXTURE][Handles::MENU]->color = glm::vec4( COLOR_APPEARANCE_SOURCE, 1.f);
    handles_[View::TEXTURE][Handles::MENU]->translation_.z = 0.1;
    overlays_[View::TEXTURE]->attach(handles_[View::TEXTURE][Handles::MENU]);
    groups_[View::TEXTURE]->attach(overlays_[View::TEXTURE]);

    // locker switch button : locked / unlocked icons
    locker_  = new Switch;
    lock_ = new Handles(Handles::LOCKED);
    lock_->color.a = 0.6;
    locker_->attach(lock_);
    unlock_ = new Handles(Handles::UNLOCKED);
    unlock_->color.a = 0.6;
    locker_->attach(unlock_);

    // simple image shader (with texturing) for blending
    blendingshader_ = new ImageShader;
    // mask produced by dedicated shader
    maskshader_ = new MaskShader;
    masksurface_ = new Surface(maskshader_);

    // for drawing in mixing view
    mixingshader_ = new ImageShader;
    mixingshader_->stipple = 1.0f;
    mixinggroup_ = nullptr;

    // create media surface:
    // - textured with original texture from media player
    // - crop & repeat UV can be managed here
    // - additional custom shader can be associated
    texturesurface_ = new Surface(renderingshader_);

    // will be created at init
    renderbuffer_   = nullptr;
    rendersurface_  = nullptr;
    mixingsurface_  = nullptr;
    activesurface_  = nullptr;
    maskbuffer_     = nullptr;
    maskimage_      = nullptr;
    masksource_     = new SourceLink;

    // default audio
    audio_flags_ = Audio_none;
    audio_volume_mix_ = Volume_mult_parent | Volume_mult_session;
    audio_volume_[0] = 0.f;
    audio_volume_[1] = 1.f;
    audio_volume_[2] = 1.f;
    audio_volume_[3] = 1.f;
}


Source::~Source()
{
    // inform links that they lost their target
    while ( !links_.empty() )
        links_.front()->disconnect();

    // inform clones that they lost their origin
    for (auto it = clones_.begin(); it != clones_.end(); ++it)
        (*it)->detach();
    clones_.clear();

    // delete objects
    if (renderbuffer_)
        delete renderbuffer_;
    if (maskbuffer_)
        delete maskbuffer_;
    if (maskimage_)
        delete maskimage_;
    if (masksurface_)
        delete masksurface_; // deletes maskshader_
    delete masksource_;

    delete texturesurface_;

    overlays_.clear();
    frames_.clear();
    handles_.clear();

    // clear and delete callbacks
    access_callbacks_.lock();
    for (auto iter=update_callbacks_.begin(); iter != update_callbacks_.end(); )  {
        SourceCallback *callback = *iter;
        iter = update_callbacks_.erase(iter);
        delete callback;
    }
    access_callbacks_.unlock();
}

void Source::setName (const std::string &name)
{
    if (!name.empty())
        name_ = BaseToolkit::unspace( BaseToolkit::transliterate(name) );

    initials_[0] = std::toupper( name_.front(), std::locale("C") );
    initials_[1] = std::toupper( name_.back(), std::locale("C") );

    initial_0_->setChar(initials_[0]);
    initial_1_->setChar(initials_[1]);
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
        for (auto g = groups_.begin(); g != groups_.end(); ++g)
            (*g).second->visible_ = true;
    }

    // Switch frame between visible, selected and current modes
    const uint index_frame = MAX(m, 1) - 1;
    for (auto f = frames_.begin(); f != frames_.end(); ++f)
        (*f).second->setActive(index_frame);

    // Switch overlay if current
    const bool current = m >= Source::CURRENT;
    for (auto o = overlays_.begin(); o != overlays_.end(); ++o)
        (*o).second->visible_ = (current && !locked_);

    // the opacity of the initials and of blending icon change if current
    initial_0_->color.w = initial_1_->color.w = current ? 1.0 : 0.7;
    static_cast<Symbol *>(blendmode_->activeChild())->color.w = current ? 1.0 : 0.6;

    // the lock icon
    locker_->setActive( locked_ ? 0 : 1);
    locker_->child(1)->visible_ = current;

    // the mixing group overlay
    overlay_mixinggroup_->visible_ = mixinggroup_!= nullptr && !locked_;
    overlay_mixinggroup_->setActive(current);

    // show in texturing view if selected or current
    groups_[View::TEXTURE]->visible_ = m > Source::VISIBLE;

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
        renderingshader_ = static_cast<Shader *>(processingshader_);
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
        renderingshader_ = static_cast<Shader *>(new ImageShader);
    }

    // apply to nodes in subclasses
    // this calls replaceShader() on the Primitive and
    // will delete the previously attached shader
    texturesurface_->replaceShader(renderingshader_);
}

void Source::setTextureMirrored (bool on)
{
    texturesurface_->setMirrorTexture(on);
}

bool Source::textureMirrored ()
{
    return texturesurface_->mirrorTexture();
}

bool Source::imageProcessingEnabled() const
{
    return ( renderingshader_ == processingshader_ );
}

void Source::render()
{
    if ( renderbuffer_ == nullptr )
        init();
    else {
        // render the view into frame buffer
        // NB: this also applies the color correction shader
        renderbuffer_->begin();
        texturesurface_->draw(glm::identity<glm::mat4>(), renderbuffer_->projection());
        renderbuffer_->end();
        ready_ = true;
    }
}

void Source::attach(FrameBuffer *renderbuffer)
{
    // invalid argument
    if (renderbuffer == nullptr)
        return;

    // replace renderbuffer_
    if (renderbuffer_)
        delete renderbuffer_;
    renderbuffer_ = renderbuffer;

    // create rendersurface_ only once
    if ( rendersurface_ == nullptr) {
        // create the surfaces to draw the frame buffer in the views
        rendersurface_ = new FrameBufferMeshSurface(renderbuffer_, blendingshader_);
        groups_[View::RENDERING]->attach(rendersurface_);
        groups_[View::GEOMETRY]->attach(rendersurface_);
    }
    else
        rendersurface_->setFrameBuffer(renderbuffer_);

    // create mixingsurface_ only once
    if ( mixingsurface_ == nullptr) {

        // for mixing and layer views, add another surface to overlay
        // (stippled view on top with transparency)
        mixingsurface_ = new FrameBufferSurface(renderbuffer_, mixingshader_);
        groups_[View::MIXING]->attach(mixingsurface_);
        groups_[View::LAYER]->attach(mixingsurface_);

        // Transition group node is optionnal
        if (groups_[View::TRANSITION]->numChildren() > 0)
            groups_[View::TRANSITION]->attach(mixingsurface_);
    }
    else
        mixingsurface_->setFrameBuffer(renderbuffer_);

    // create activesurface_ only once (and other initializations done once)
    if ( activesurface_ == nullptr) {

        // for views showing a scaled mixing surface, a dedicated transparent surface allows grabbing
        activesurface_ = new Surface;
        activesurface_->setTextureIndex(Resource::getTextureTransparent());
        groups_[View::TEXTURE]->attach(activesurface_);
        groups_[View::MIXING]->attach(activesurface_);
        groups_[View::LAYER]->attach(activesurface_);

        // if a symbol is available, add it to overlay
        if (symbol_) {
            overlays_[View::MIXING]->attach( symbol_ );
            overlays_[View::LAYER]->attach( symbol_ );
        }

        // add lock icon to views (displayed on front)
        groups_[View::LAYER]->attach( locker_ );
        groups_[View::MIXING]->attach( locker_ );
        groups_[View::GEOMETRY]->attach( locker_ );
        groups_[View::TEXTURE]->attach( locker_ );

    }

    float AR = renderbuffer_->aspectRatio();

    // if a symbol is available
    if (symbol_)
        // hack to place the symbols in the corner independently of aspect ratio
        symbol_->translation_.x = (AR - 0.3f) / AR;

    // hack to place the initials in the corner independently of aspect ratio
    initial_0_->translation_.x = 0.2f - AR;
    initial_1_->translation_.x = 0.4f - AR;
    blendmode_->translation_.x = - 0.2f - AR ;

    // scale all icon nodes to match aspect ratio
    for (int v = View::MIXING; v <= View::TRANSITION; v++) {
        NodeSet::iterator node;
        for (node = groups_[(View::Mode) v]->begin();
             node != groups_[(View::Mode) v]->end(); ++node) {
            (*node)->scale_.x = AR;
        }
    }

    // (re) create the masking buffer
    if (maskbuffer_)
        delete maskbuffer_;
    maskbuffer_ = new FrameBuffer( glm::vec3(0.5) * renderbuffer->resolution() );

    // make the source visible
    if ( mode_ == UNINITIALIZED )
        setMode(VISIBLE);

    // request update
    need_update_ |= Source::SourceUpdate_Render;
}

void Source::setActive (bool on)
{
    // do not disactivate if any clone depends on it
    for(auto clone = clones_.begin(); clone != clones_.end(); ++clone) {
        if ( (*clone)->ready() && (*clone)->active() ) {
            on = true;
            break;
        }
    }

    // request update
    if (active_ != on)
        need_update_ |= Source::SourceUpdate_Render;

    // activate
    active_ = on;

    // an inactive source is not rendered
    groups_[View::RENDERING]->visible_ = active_;
}

void Source::setActive (float threshold)
{
    setActive( glm::length( glm::vec2(groups_[View::MIXING]->translation_) ) < threshold );
}

void Source::setLocked (bool on)
{
    locked_ = on;

    setMode(mode_);
}

// Transfer functions from coordinates to alpha (1 - transparency)

//// linear distance
//float linear_(float x, float y) {
//    return 1.f - CLAMP( sqrt( ( x * x ) + ( y * y ) ), 0.f, 1.f );
//}

//// quadratic distance
//float quad_(float x, float y) {
//    return 1.f - CLAMP( ( x * x ) + ( y * y ), 0.f, 1.f );
//}

//// best alpha transfer function: quadratic sinusoidal shape
//float sin_quad_(float x, float y) {
//    float D = sqrt( ( x * x ) + ( y * y ) );
//    return 0.5f + 0.5f * cos( M_PI * CLAMP( D * sqrt(D), 0.f, 1.f ) );
////    return 0.5f + 0.5f * cos( M_PI * CLAMP( ( ( x * x ) + ( y * y ) ), 0.f, 1.f ) );
//}

float SourceCore::alphaFromCordinates(float x, float y)
{
    float D = sqrt( ( x * x ) + ( y * y ) );
    return 0.5f + 0.5f * cos( M_PI * CLAMP( D * sqrt(D), 0.f, 1.f ) );
}


float Source::depth() const
{
    return group(View::RENDERING)->translation_.z;
}


float Source::alpha() const
{
    return blendingShader()->color.a;
}


bool Source::visible() const
{
    return blendingShader()->color.a > 0.f;
}


bool Source::texturePostProcessed () const
{
    if (imageProcessingEnabled())
        return true;

    return frame()->projectionArea() != glm::vec4(-1.f, 1.f, 1.f, -1.f) ||       // cropped
           group(View::TEXTURE)->rotation_.z != 0.f ||          // rotation
           group(View::TEXTURE)->scale_ != glm::vec3(1.f) ||    // scaled
           group(View::TEXTURE)->translation_ != glm::vec3(0.f);// displaced
}

void Source::call(SourceCallback *callback, bool override)
{
    if (callback != nullptr) {

        // lock access to callbacks list
        access_callbacks_.lock();

        // if operation should override previous callbacks of same type
        if (override) {
            // finish all callbacks of the same type
            for (auto iter=update_callbacks_.begin(); iter != update_callbacks_.end(); ++iter) {
                if ( callback->type() == (*iter)->type() )
                    (*iter)->finish();
            }
        }

        // allways add the given callback to list of callbacks
        update_callbacks_.push_back(callback);

        // release access to callbacks list
        access_callbacks_.unlock();
    }

}

void Source::finish(SourceCallback *callback)
{
    if (callback != nullptr) {

        // lock access to callbacks list
        access_callbacks_.lock();

        // make sure the given pointer is a callback listed in this source
        auto cb = std::find(update_callbacks_.begin(), update_callbacks_.end(), callback);
        if (cb != update_callbacks_.end())
            // set found callback to finish state
            (*cb)->finish();

        // release access to callbacks list
        access_callbacks_.unlock();
    }
}

void Source::updateCallbacks(float dt)
{
    // lock access to callbacks list
    access_callbacks_.lock();

    // call callback functions
    for (auto iter=update_callbacks_.begin(); iter != update_callbacks_.end(); )
    {
        SourceCallback *callback = *iter;

        // call update on callbacks
        callback->update(this, dt);
        need_update_ |= Source::SourceUpdate_Render;

        // remove and delete finished callbacks
        if (callback->finished()) {
            iter = update_callbacks_.erase(iter);
            delete callback;
        }
        // iterate
        else
            ++iter;
    }

    // release access to callbacks list
    access_callbacks_.unlock();
}

CloneSource *Source::clone(uint64_t id)
{
    CloneSource *s = new CloneSource(this, id);

    clones_.push_back(s);

    return s;
}

void Source::update(float dt)
{
    // keep delta-t
    dt_ = dt;

    // if update is possible
    if (renderbuffer_ && mixingsurface_ && maskbuffer_)
    {
        // call active callbacks
        updateCallbacks(dt);

        // update nodes if needed
        if (need_update_ & SourceUpdate_Render)
        {
            // do not update next frame
            need_update_ &= ~SourceUpdate_Render;

            // ADJUST alpha based on MIXING node
            // read position of the mixing node and interpret this as transparency of render output
            glm::vec2 dist = glm::vec2(groups_[View::MIXING]->translation_);
            // use the sinusoidal transfer function to compute alpha
            float __a = SourceCore::alphaFromCordinates(dist.x, dist.y);
            // audio update in case if depends on alpha
            setAudioVolumeFactor(Source::VOLUME_ALPHA, __a);
            // apply alpha
            blendingshader_->color = glm::vec4(1.f, 1.f, 1.f, __a);
            mixingshader_->color = blendingshader_->color;

            // adjust scale of mixing icon : smaller if not active
            groups_[View::MIXING]->scale_ = glm::vec3(MIXING_ICON_SCALE) - ( active_ ? glm::vec3(0.f, 0.f, 0.f) : glm::vec3(0.03f, 0.03f, 0.f) );
            // change stippling intensity of source in mixing view to indicate if shown in scene
            mixingshader_->stipple = (blendingshader_->color.a > 0.f) ? 1.f : 0.75f;

            // MODIFY geometry based on GEOMETRY node
            groups_[View::RENDERING]->translation_ = groups_[View::GEOMETRY]->translation_;
            groups_[View::RENDERING]->rotation_ = groups_[View::GEOMETRY]->rotation_;
            glm::vec3 s = groups_[View::GEOMETRY]->scale_;
            // avoid any null scale
            s.x = CLAMP_SCALE(s.x);
            s.y = CLAMP_SCALE(s.y);
            s.z = 1.f;
            groups_[View::GEOMETRY]->scale_ = s;
            groups_[View::RENDERING]->scale_ = s;

            // MODIFY CROP projection based on GEOMETRY crop
            renderbuffer_->setProjectionArea( groups_[View::GEOMETRY]->crop_ );

            // TODO : should it be applied to MIXING view?
            // Mixing and layer icons scaled based on GEOMETRY crop
            mixingsurface_->scale_.y = (groups_[View::GEOMETRY]->crop_[2]
                                        - groups_[View::GEOMETRY]->crop_[3]) * 0.5f;
            mixingsurface_->scale_.x = (groups_[View::GEOMETRY]->crop_[1]
                                        - groups_[View::GEOMETRY]->crop_[0]) * 0.5f * renderbuffer_->aspectRatio();
            mixingsurface_->update(dt_);

            // update nodes distortion
            groups_[View::GEOMETRY]->data_[0] = glm::clamp(groups_[View::GEOMETRY]->data_[0], glm::vec4(0.f), glm::vec4(1.f));
            groups_[View::GEOMETRY]->data_[1] = glm::clamp(groups_[View::GEOMETRY]->data_[1], glm::vec4(0.f, -1.f, 0.f, 0.f), glm::vec4(1.f, 0.f, 1.f, 1.f));
            groups_[View::GEOMETRY]->data_[2] = glm::clamp(groups_[View::GEOMETRY]->data_[2], glm::vec4(-1.f, 0.f, 0.f, 0.f), glm::vec4(0.f, 1.f, 1.f, 1.f));
            groups_[View::GEOMETRY]->data_[3] = glm::clamp(groups_[View::GEOMETRY]->data_[3], glm::vec4(-1.f, -1.f, 0.f, 0.f), glm::vec4(0.f, 0.f, 1.f, 1.f));
            blendingshader_->iNodes = groups_[View::GEOMETRY]->data_;

            handles_[View::GEOMETRY][Handles::NODE_LOWER_LEFT]->translation_.x = groups_[View::GEOMETRY]->data_[0].x;
            handles_[View::GEOMETRY][Handles::NODE_LOWER_LEFT]->translation_.y = groups_[View::GEOMETRY]->data_[0].y;
            handles_[View::GEOMETRY][Handles::NODE_UPPER_LEFT]->translation_.x = groups_[View::GEOMETRY]->data_[1].x;
            handles_[View::GEOMETRY][Handles::NODE_UPPER_LEFT]->translation_.y = groups_[View::GEOMETRY]->data_[1].y;
            handles_[View::GEOMETRY][Handles::NODE_LOWER_RIGHT]->translation_.x = groups_[View::GEOMETRY]->data_[2].x;
            handles_[View::GEOMETRY][Handles::NODE_LOWER_RIGHT]->translation_.y = groups_[View::GEOMETRY]->data_[2].y;
            handles_[View::GEOMETRY][Handles::NODE_UPPER_RIGHT]->translation_.x = groups_[View::GEOMETRY]->data_[3].x;
            handles_[View::GEOMETRY][Handles::NODE_UPPER_RIGHT]->translation_.y = groups_[View::GEOMETRY]->data_[3].y;

//            handles_[View::GEOMETRY][Handles::ROUNDING]->translation_.x = - groups_[View::GEOMETRY]->data_[0].z;
            handles_[View::GEOMETRY][Handles::ROUNDING]->translation_.x = - groups_[View::GEOMETRY]->data_[0].w;

            // Layers icons are displayed in Perspective (diagonal)
            groups_[View::LAYER]->translation_.x = -groups_[View::LAYER]->translation_.z;
            groups_[View::LAYER]->translation_.y = groups_[View::LAYER]->translation_.x / LAYER_PERSPECTIVE;

            // update blending icon
            static_cast<Symbol *>(blendmode_->activeChild())->color.w = 0.6;
            blendmode_->setActive((int) blendingshader_->blending);
            static_cast<Symbol *>(blendmode_->activeChild())->color.w = mode_ >= Source::CURRENT ? 1.0 : 0.6;

            // Update workspace based on depth, and
            // adjust vertical position of icon depending on workspace
            if (groups_[View::LAYER]->translation_.x < -LAYER_FOREGROUND) {
                groups_[View::LAYER]->translation_.y -= 0.3f;
                workspace_ = Source::WORKSPACE_FOREGROUND;
            }
            else if (groups_[View::LAYER]->translation_.x < -LAYER_BACKGROUND) {
                groups_[View::LAYER]->translation_.y -= 0.15f;
                workspace_ = Source::WORKSPACE_CENTRAL;
            }
            else
                workspace_ = Source::WORKSPACE_BACKGROUND;

            // MODIFY depth based on LAYER node
            groups_[View::MIXING]->translation_.z = groups_[View::LAYER]->translation_.z;
            groups_[View::GEOMETRY]->translation_.z = groups_[View::LAYER]->translation_.z;
            groups_[View::RENDERING]->translation_.z = groups_[View::LAYER]->translation_.z;

            // MODIFY texture projection based on APPEARANCE node
            // UV to node coordinates
            static glm::mat4 UVtoScene = GlmToolkit::transform(glm::vec3(1.f, -1.f, 0.f),
                                                               glm::vec3(0.f, 0.f, 0.f),
                                                               glm::vec3(-2.f, 2.f, 1.f));
            // Aspect Ratio correction transform : coordinates of Appearance Frame are scaled by render buffer width
            glm::mat4 Ar = glm::scale(glm::identity<glm::mat4>(), glm::vec3(renderbuffer_->aspectRatio(), 1.f, 1.f) );
            // Translation : same as Appearance Frame (modified by Ar)
            glm::mat4 Tra = glm::translate(glm::identity<glm::mat4>(), groups_[View::TEXTURE]->translation_);
            // Scaling : inverse scaling (larger UV when smaller Appearance Frame)
            glm::vec2 scale =  glm::vec2(groups_[View::TEXTURE]->scale_.x,groups_[View::TEXTURE]->scale_.y);
            scale = glm::sign(scale) * glm::max( glm::vec2(glm::epsilon<float>()), glm::abs(scale));
            glm::mat4 Sca = glm::scale(glm::identity<glm::mat4>(), glm::vec3(scale, 1.f));
            // Rotation : same angle than Appearance Frame, inverted axis
            glm::mat4 Rot = glm::rotate(glm::identity<glm::mat4>(), groups_[View::TEXTURE]->rotation_.z, glm::vec3(0.f, 0.f, -1.f) );
            // Combine transformations (non transitive) in this order:
            // 1. switch to Scene coordinate system
            // 2. Apply the aspect ratio correction
            // 3. Apply the translation
            // 4. Apply the rotation (centered after translation)
            // 5. Revert aspect ration correction
            // 6. Apply the Scaling (independent of aspect ratio)
            // 7. switch back to UV coordinate system
            texturesurface_->shader()->iTransform = glm::inverse(UVtoScene) * glm::inverse(Sca) * glm::inverse(Ar) * Rot * Tra * Ar * UVtoScene;

            // inform mixing group
            if (mixinggroup_)
                mixinggroup_->setAction(MixingGroup::ACTION_UPDATE);
        }

        if (need_update_ & SourceUpdate_Mask_fill) {

            // do not update Mask fill next frame
            need_update_ &= ~SourceUpdate_Mask_fill;

            // fill the mask buffer (once)
            maskbuffer_->fill(maskimage_);
        }

        if (need_update_ & SourceUpdate_Mask) {

            // do not update Mask next frame
            need_update_ &= ~SourceUpdate_Mask;

            // MODIFY Mask based on mask shader mode
            // if MaskShader::SOURCE available, set a source as mask for blending
            if (maskshader_->mode == MaskShader::SOURCE && masksource_->connected()) {
                Source *ref_source = masksource_->source();
                if (ref_source != nullptr) {
                    if (ref_source->ready())
                        // set mask texture to mask source
                        blendingshader_->secondary_texture = ref_source->frame()->texture();
                    else
                        // retry for when source will be ready
                        need_update_ |= SourceUpdate_Mask;
                }
            }
            // all other MaskShader types to update
            else {
                // draw mask in mask frame buffer
                maskbuffer_->begin(false);
                // loopback maskbuffer texture for painting
                masksurface_->setTextureIndex(maskbuffer_->texture());
                // fill surface with mask texture
                masksurface_->draw(glm::identity<glm::mat4>(), maskbuffer_->projection());
                maskbuffer_->end();
                // set mask texture to mask buffer
                blendingshader_->secondary_texture = maskbuffer_->texture();
            }
        }

        // update audio if requested:
        if (need_update_ & SourceUpdate_Audio) {
            // do not update Audio next frame
            need_update_ &= ~SourceUpdate_Audio;
            // implementation depends on subclasses
            updateAudio();
        }

        if (processingshader_link_.connected() && imageProcessingEnabled()) {
            Source *ref_source = processingshader_link_.source();
            if (ref_source!=nullptr) {
                if (ref_source->imageProcessingEnabled())
                    processingshader_->copy( *ref_source->processingShader() );
                else
                    processingshader_link_.disconnect();
            }
        }

    }

}

FrameBuffer *Source::frame() const
{
    if ( mode_ > Source::UNINITIALIZED && renderbuffer_)
    {
        return renderbuffer_;
    }
    else {
        static FrameBuffer *black = new FrameBuffer(64,64);
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

void Source::storeMask(FrameBufferImage *img)
{
    // free the output mask storage
    if (maskimage_ != nullptr) {
        delete maskimage_;
        maskimage_ = nullptr;
    }

    // if no image is provided
    if (img == nullptr) {
        // if ready
        if (maskbuffer_!=nullptr) {
            // get & store image from mask buffer
            maskimage_ = maskbuffer_->image();
        }
    }
    else
        // store the given image
        maskimage_ = img;

    // maskimage_ can now be accessed with Source::getStoredMask
}

void Source::setMask(FrameBufferImage *img)
{
    // if a valid image is given
    if (img != nullptr && img->width>0 && img->height>0) {

        // remember this new image as the current mask
        // NB: will be freed when replaced
        storeMask(img);

        // ask to update the source mask
        touch(Source::SourceUpdate_Mask_fill);
    }
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
        for (auto g = elem->groups_.begin(); g != elem->groups_.end(); ++g) {
            (*g).second->accept(sv);
            if (sv.found())
                return true;
        }
        // search in overlays for all views
        for (auto g = elem->overlays_.begin(); g != elem->overlays_.end(); ++g) {
            (*g).second->accept(sv);
            if (sv.found())
                return true;
        }
    }

    return false;
}


void Source::clearMixingGroup()
{
    mixinggroup_ = nullptr;
    overlay_mixinggroup_->visible_ = false;
}

// set audio factors and mixing mode
void Source::setAudioEnabled(bool on)
{
    if (on)
        audio_flags_ |= Source::Audio_enabled;
    else
        audio_flags_ &= ~Source::Audio_enabled;

    need_update_ |= Source::SourceUpdate_Audio;
}

void Source::setAudioVolumeFactor(AudioVolumeFactor index, float value)
{
    if (ABS_DIFF(audio_volume_[index], value) > EPSILON
        && (index == VOLUME_BASE
            || (index == VOLUME_ALPHA   && audio_volume_mix_ & Volume_mult_alpha  )
            || (index == VOLUME_OPACITY && audio_volume_mix_ & Volume_mult_opacity)
            || (index == VOLUME_PARENT  && audio_volume_mix_ & Volume_mult_parent )
            || (index == VOLUME_SESSION && audio_volume_mix_ & Volume_mult_session) )) {
        need_update_ |= Source::SourceUpdate_Audio;
    }

    audio_volume_[index] = CLAMP(value, 0.f, 1.f);
}

void Source::setAudioVolumeMix(AudioVolumeMixing f)
{
    if ( audio_volume_mix_ != f )
        need_update_ |= Source::SourceUpdate_Audio;

    audio_volume_mix_ = f;
}

glm::vec2 Source::attractor(size_t i) const
{
    glm::vec2 ret(0.f);
    i = CLAMP(i, 0, 3);
    ret.x = blendingshader_->iNodes[i].z;
    ret.y = blendingshader_->iNodes[i].w;
    return ret;
}

void Source::setAttractor(size_t i, glm::vec2 a)
{
    i = CLAMP(i, 0, 3);
    blendingshader_->iNodes[i].z = a.x;
    blendingshader_->iNodes[i].w = a.y;
}

