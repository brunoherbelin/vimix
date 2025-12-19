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

#include <glm/gtc/type_ptr.hpp>
#include <glm/gtc/matrix_access.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/random.hpp>

#include <glad/glad.h>

#include <algorithm>

#include "defines.h"
#include "Shader.h"
#include "Visitor/Visitor.h"
#include "Toolkit/BaseToolkit.h"
#include "Toolkit/GlmToolkit.h"

#include "Scene.h"

#define DEBUG_SCENE 0
#if DEBUG_SCENE
static int num_nodes_ = 0;
#endif

// Node
Node::Node() : initialized_(false), visible_(true), refcount_(0)
{
    // create unique id
    id_ = BaseToolkit::uniqueId();

    transform_ = glm::identity<glm::mat4>();
    scale_ = glm::vec3(1.f);
    rotation_ = glm::vec3(0.f);
    translation_ = glm::vec3(0.f);
    crop_ = glm::vec4(-1.f, 1.f, 1.f, -1.f);
    data_ = glm::zero<glm::mat4>();
#if DEBUG_SCENE
    num_nodes_++;
#endif
}

Node::~Node ()
{
    clearCallbacks();
#if DEBUG_SCENE
    num_nodes_--;
#endif
}

void Node::clearCallbacks()
{
    std::list<UpdateCallback *>::iterator iter;
    for (iter=update_callbacks_.begin(); iter != update_callbacks_.end(); )
    {
        UpdateCallback *callback = *iter;
        iter = update_callbacks_.erase(iter);
        delete callback;
    }
}

void Node::copyTransform(const Node *other)
{
    if (!other)
        return;
    transform_ = other->transform_;
    scale_ = other->scale_;
    rotation_ = other->rotation_;
    translation_ = other->translation_;
    crop_ = other->crop_;
    data_ = other->data_;
}

void Node::update( float dt)
{
    for (auto iter=update_callbacks_.begin(); iter != update_callbacks_.end(); )
    {
        UpdateCallback *callback = *iter;

        callback->update(this, dt);

        if (callback->finished()) {
            iter = update_callbacks_.erase(iter);
            delete callback;
        }
        else
            ++iter;
    }

    // update transform matrix from attributes
    transform_ = GlmToolkit::transform(translation_, rotation_, scale_);
}

void Node::accept(Visitor& v)
{
    v.visit(*this);
}

//
// Primitive
//

Primitive::~Primitive()
{
    if ( vao_ )
        glDeleteVertexArrays ( 1, &vao_);
    if (shader_)
        delete shader_;
}

void Primitive::init()
{
    if ( vao_ )
        glDeleteVertexArrays ( 1, &vao_);

    // Vertex Array
    glGenVertexArrays( 1, &vao_ );
    // Create and initialize buffer objects
    uint arrayBuffer_;
    uint elementBuffer_;
    glGenBuffers( 1, &arrayBuffer_ );
    glGenBuffers( 1, &elementBuffer_);
    glBindVertexArray( vao_ );

    // compute the memory needs for points
    std::size_t sizeofPoints = sizeof(glm::fvec3) * points_.size();
    std::size_t sizeofColors = sizeof(glm::fvec4) * colors_.size();
    std::size_t sizeofTexCoords = sizeof(glm::fvec2) * texCoords_.size();

    // setup the array buffers for vertices
    glBindBuffer( GL_ARRAY_BUFFER, arrayBuffer_ );
    glBufferData( GL_ARRAY_BUFFER, sizeofPoints + sizeofColors + sizeofTexCoords, NULL, GL_STATIC_DRAW);
    glBufferSubData( GL_ARRAY_BUFFER, 0, sizeofPoints, &points_[0] );
    glBufferSubData( GL_ARRAY_BUFFER, sizeofPoints, sizeofColors, &colors_[0] );
    if ( sizeofTexCoords )
        glBufferSubData( GL_ARRAY_BUFFER, sizeofPoints + sizeofColors, sizeofTexCoords, &texCoords_[0] );

    // setup the element array for the triangle indices
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, elementBuffer_);
    std::size_t sizeofIndices = indices_.size() * sizeof(GLuint);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeofIndices, &(indices_[0]), GL_STATIC_DRAW);

    // explain how to read attributes 0, 1 and 2 (for point, color and textcoord respectively)
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(glm::fvec3), (void *)0 );
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(1, 4, GL_FLOAT, GL_FALSE, sizeof(glm::fvec4), (void *)(sizeofPoints) );
    glEnableVertexAttribArray(1);
    if ( sizeofTexCoords ) {
        glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, sizeof(glm::fvec2), (void *)(sizeofPoints + sizeofColors) );
        glEnableVertexAttribArray(2);
    }

    // done
    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindVertexArray(0);

    // drawing indications
    drawCount_ = indices_.size();

    // delete temporary buffers
    if ( arrayBuffer_ )
        glDeleteBuffers ( 1, &arrayBuffer_);
    if ( elementBuffer_ )
        glDeleteBuffers ( 1, &elementBuffer_);

    // compute AxisAlignedBoundingBox
    bbox_.extend(points_);

    // arrays of vertices are not needed anymore (STATIC DRAW of vertex object)
    points_.clear();
    colors_.clear();
    texCoords_.clear();
    indices_.clear();

    Node::init();
}

void Primitive::draw(glm::mat4 modelview, glm::mat4 projection)
{
    if ( !initialized() )
        init();

    if ( visible_ ) {
        //
        // prepare and use shader
        //
        if (shader_) {
            shader_->projection = projection;
            shader_->modelview  = modelview * transform_;
            shader_->use();
        }
        //
        // draw vertex array object
        //
        if (vao_) {
            glBindVertexArray( vao_ );
            glDrawElements( drawMode_, drawCount_, GL_UNSIGNED_INT, 0  );
            glBindVertexArray(0);
        }
    }
}

void Primitive::accept(Visitor& v)
{
    Node::accept(v);
    v.visit(*this);
}

void Primitive::replaceShader( Shader *newshader )
{
    // keep ref to previous shader
    Shader *previousshader = shader_;

    // apply new shader to Primitive, even if nullptr
    shader_ = newshader;

    if (shader_) {
        // get new attribs...
        glm::mat4 iTransform = newshader->iTransform;
        glm::vec4 color = newshader->color;
        // ...or use previous shader attribs, if existed
        if (previousshader) {
            iTransform = previousshader->iTransform;
            color = previousshader->color;
            // shader is swapped, previous can be deleted
            delete previousshader;
        }
        // Apply attribs
        shader_->iTransform = iTransform;
        shader_->color = color;
    }

}

//
// Group
//

Group::~Group()
{
    clear();
}

void Group::clear()
{
    for(NodeSet::iterator it = children_.begin(); it != children_.end(); ) {
        // one less ref to this node
        (*it)->refcount_--;
        // if this group was the only remaining parent
        if ( (*it)->refcount_ < 1 ) {
            // delete
            delete (*it);
        }
        // erase this iterator from the list
        it = children_.erase(it);
    }
}

void Group::attach(Node *child)
{
    if (child != nullptr) {
        children_.insert(child);
        child->refcount_++;
    }
}

void Group::sort()
{
    // reorder list of nodes
    NodeSet ordered_children;
    for(auto it = children_.begin(); it != children_.end(); it++)
        ordered_children.insert(*it);
    children_.swap(ordered_children);
}

void Group::detach(Node *child)
{
    if (child != nullptr) {
        // find the node with this id, and erase it out of the list of children
        // NB: do NOT delete with remove : this takes all nodes with same depth (i.e. equal depth in set)
        NodeSet::iterator it = std::find_if(children_.begin(), children_.end(), hasId(child->id()));
        if ( it != children_.end())  {
            // detatch child from group parent
            children_.erase(it);
            child->refcount_--;
        }
    }
}

void Group::update( float dt )
{
    Node::update(dt);

    // update every child node
    for (NodeSet::iterator node = children_.begin();
         node != children_.end(); ++node) {
        (*node)->update ( dt );
    }
}

void Group::draw(glm::mat4 modelview, glm::mat4 projection)
{
    if ( !initialized() )
        init();

    if ( visible_ ) {

        // append the instance transform to the ctm
        glm::mat4 ctm = modelview * transform_;

        // draw every child node
        for (NodeSet::iterator node = children_.begin();
             node != children_.end(); ++node) {
            (*node)->draw ( ctm, projection );
        }
    }
}

void Group::accept(Visitor& v)
{
    Node::accept(v);
    v.visit(*this);
}

NodeSet::iterator Group::begin()
{
    return children_.begin();
}

NodeSet::iterator Group::end()
{
    return children_.end();
}


Node *Group::front() const
{
    if (!children_.empty())
        return *children_.crbegin();
    return nullptr;
}

Node *Group::back() const
{
    if (!children_.empty())
        return *children_.cbegin();
    return nullptr;
}

//
// Switch node
//

Switch::~Switch()
{
    clear();
}

void Switch::clear()
{
    for(std::vector<Node *>::iterator it = children_.begin(); it != children_.end(); ) {
        // one less ref to this node
        (*it)->refcount_--;
        // if this group was the only remaining parent
        if ( (*it)->refcount_ < 1 ) {
            // delete
            delete (*it);
        }
        // erase this iterator from the list
        it = children_.erase(it);
    }

    // reset active
    active_ = 0;
}


void Switch::update( float dt )
{
    Node::update(dt);

    // update active child node
    if (!children_.empty())
        (children_[active_])->update( dt );
}

void Switch::draw(glm::mat4 modelview, glm::mat4 projection)
{
    if ( !initialized() )
        init();

    if ( visible_ ) {
        // draw current child        
        if (!children_.empty())
            (children_[active_])->draw( modelview * transform_, projection);
    }
}

void Switch::accept(Visitor& v)
{
    Node::accept(v);
    v.visit(*this);
}

void Switch::setActive (uint index)
{
    active_ = MINI(index, children_.size() - 1);
}

Node *Switch::child(uint index) const
{
    if (!children_.empty()) {
        uint i = MINI(index, children_.size() - 1);
        return children_.at(i);
    }
    return nullptr;
}

uint Switch::attach(Node *child)
{
    children_.push_back(child);
    child->refcount_++;

    // make new child active
    active_ = children_.size() - 1;
    return active_;
}

void Switch::detatch(Node *child)
{
    // if removing the active child, it cannot be active anymore
    if ( children_.at(active_) == child )
        active_ = 0;

    // find the node with this id, and erase it out of the list of children
    std::vector<Node *>::iterator it = std::find_if(children_.begin(), children_.end(), hasId(child->id()));
    if ( it != children_.end())  {
        // detatch child from group parent
        children_.erase(it);
        child->refcount_--;
    }
}

//
// Scene
//

Scene::Scene()
{
    root_ = new Group;

    background_ = new Group;
    background_->translation_.z = 0.f;
    root_->attach(background_);

    workspace_  = new Group;
    workspace_->translation_.z  = 1.f;
    root_->attach(workspace_);

    foreground_ = new Group;
    foreground_->translation_.z = SCENE_DEPTH -0.1f;
    root_->attach(foreground_);
}

Scene::~Scene()
{
    clear();
    // bg and fg are deleted as children of root
    delete root_;
#if DEBUG_SCENE
    Log::Info("Total scene nodes %d", num_nodes_);
#endif
}


void Scene::clear()
{
    clearForeground();
    clearWorkspace();
    clearBackground();
}

void Scene::clearForeground()
{
    foreground_->clear();
}

void Scene::clearWorkspace()
{
    workspace_->clear();
}

void Scene::clearBackground()
{
    background_->clear();
}

void Scene::update(float dt)
{
    root_->update( dt );
}

void Scene::accept(Visitor& v)
{
    v.visit(*this);
}
