#include "defines.h"
#include "Scene.h"
#include "Shader.h"
#include "Primitives.h"
#include "Visitor.h"
#include "Log.h"

#include <glad/glad.h>

#include <glm/gtc/type_ptr.hpp>
#include <glm/ext/vector_float3.hpp>
#include <glm/ext/vector_float4.hpp>
#include <glm/ext/matrix_float4x4.hpp>
#include <glm/gtc/matrix_access.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include <chrono>
#include <ctime>
#include <algorithm>


glm::mat4 transform(glm::vec3 translation, glm::vec3 rotation, glm::vec3 scale)
{
    glm::mat4 View = glm::translate(glm::identity<glm::mat4>(), translation);
    View = glm::rotate(View, rotation.x, glm::vec3(1.f, 0.f, 0.f));
    View = glm::rotate(View, rotation.y, glm::vec3(0.f, 1.f, 0.f));
    View = glm::rotate(View, rotation.z, glm::vec3(0.f, 0.f, 1.f));
    glm::mat4 Model = glm::scale(glm::identity<glm::mat4>(), scale);
    return View * Model;
}

// Node
Node::Node() : initialized_(false), parent_(nullptr), visible_(true)
{
    // create unique id
    auto duration = std::chrono::system_clock::now().time_since_epoch();
    id_ = std::chrono::duration_cast<std::chrono::nanoseconds>(duration).count() % 100000000;

    worldToLocal_ = glm::identity<glm::mat4>();
    localToWorld_ = glm::identity<glm::mat4>();
    transform_ = glm::identity<glm::mat4>();
    scale_ = glm::vec3(1.f);
    rotation_ = glm::vec3(0.f);
    translation_ = glm::vec3(0.f);
}

void Node::update( float dt )
{
    transform_ = transform(translation_, rotation_, scale_);
    if ( parent_ ) {
        localToWorld_ = dynamic_cast<Group*>(parent_)->getLocalToWorldMatrix() * transform_;
        worldToLocal_ = glm::inverse(transform_) * dynamic_cast<Group*>(parent_)->getWorldToLocalMatrix();
    }
    else {
        localToWorld_ = transform_;
        worldToLocal_ = glm::inverse(transform_);
    }
}


void Node::accept(Visitor& v) {
    v.visit(*this);
}

// Primitive

Primitive::~Primitive()
{
    deleteGLBuffers_();

    points_.clear();
    colors_.clear();
    texCoords_.clear();
    indices_.clear();

}

void Primitive::init()
{
    deleteGLBuffers_();

    // Vertex Array
    glGenVertexArrays( 1, &vao_ );
    // Create and initialize buffer objects
    uint arrayBuffer_;
    uint elementBuffer_;
    glGenBuffers( 1, &arrayBuffer_ );
    glGenBuffers( 1, &elementBuffer_);
    glBindVertexArray( vao_ );

    // compute the memory needs for points normals and indicies
    std::size_t sizeofPoints = sizeof(glm::vec3)*points_.size();
    std::size_t sizeofColors = sizeof(glm::vec3)*colors_.size();
    std::size_t sizeofTexCoords = sizeof(glm::vec2) * texCoords_.size();

    // setup the array buffers for vertices
    glBindBuffer( GL_ARRAY_BUFFER, arrayBuffer_ );
    glBufferData(GL_ARRAY_BUFFER, sizeofPoints + sizeofColors + sizeofTexCoords, NULL, GL_STATIC_DRAW);
    glBufferSubData( GL_ARRAY_BUFFER, 0, sizeofPoints, &points_[0] );
    glBufferSubData( GL_ARRAY_BUFFER, sizeofPoints, sizeofColors, &colors_[0] );
    if ( sizeofTexCoords )
        glBufferSubData( GL_ARRAY_BUFFER, sizeofPoints + sizeofColors, sizeofTexCoords, &texCoords_[0] );

    // setup the element array for the triangle indices
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, elementBuffer_);
    int sizeofIndices = indices_.size()*sizeof(uint);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeofIndices, &(indices_[0]), GL_STATIC_DRAW);

    // explain how to read attributes 0, 1 and 2 (for point, color and textcoord respectively)
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(glm::vec3), (void *)0 );
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, sizeof(glm::vec3), (void *)(sizeofPoints) );
    glEnableVertexAttribArray(1);
    if ( sizeofTexCoords ) {
        glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, sizeof(glm::vec2), (void *)(sizeofPoints + sizeofColors) );
        glEnableVertexAttribArray(2);
    }

    // done
    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindVertexArray(0);

    // delete temporary buffers
    if ( arrayBuffer_ )  glDeleteBuffers ( 1, &arrayBuffer_);
    if ( elementBuffer_ ) glDeleteBuffers ( 1, &elementBuffer_);

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
            shader_->modelview = modelview * transform_;
            shader_->use();
        }
        //
        // draw vertex array object
        //
        glBindVertexArray( vao_ );
        glDrawElements( drawingPrimitive_, indices_.size(), GL_UNSIGNED_INT, 0  );
        glBindVertexArray(0);
    }
}

void Primitive::accept(Visitor& v)
{
    Node::accept(v);
    v.visit(*this);
}


void Primitive::deleteGLBuffers_()
{
    if ( vao_ )
        glDeleteVertexArrays ( 1, &vao_);
}

// Group

Group::~Group()
{
    children_.clear();
}

void Group::init()
{
    for (NodeSet::iterator node = children_.begin();
         node != children_.end(); node++) {
        (*node)->init();
    }

    Node::init();
}

void Group::update( float dt )
{
    Node::update(dt);

    // update every child node
    for (NodeSet::iterator node = children_.begin();
         node != children_.end(); node++) {
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
             node != children_.end(); node++) {
            (*node)->draw ( ctm, projection );
        }
    }
}

void Group::accept(Visitor& v)
{
    Node::accept(v);
    v.visit(*this);
}

void Group::addChild(Node *child)
{
    children_.insert(child);
    child->parent_ = this;
}

void Group::removeChild(Node *child)
{
    NodeSet::iterator it = std::find_if(children_.begin(), children_.end(), hasId(child->id()));

    if ( it != children_.end())
    {
        children_.erase(it);
        child->parent_ = nullptr;
    }
}

NodeSet::iterator Group::begin()
{
    return children_.begin();
}

NodeSet::iterator Group::end()
{
    return children_.end();
}

uint Group::numChildren() const
{
    return children_.size();
}

void Switch::update( float dt )
{
    Node::update(dt);

    // update active child node
    if (active_ != children_.end())
        (*active_)->update( dt );
}

void Switch::draw(glm::mat4 modelview, glm::mat4 projection)
{
    if ( !initialized() )
        init();

    if ( visible_ ) {
        // append the instance transform to the ctm
        glm::mat4 ctm = modelview * transform_;

        // draw current child        
        if (active_ != children_.end())
            (*active_)->draw(ctm, projection);
    }
}

void Switch::accept(Visitor& v)
{
    Group::accept(v);
    v.visit(*this);
}

void Switch::addChild(Node *child)
{
    Group::addChild(child);
    setActiveChild(child);
}

void Switch::removeChild(Node *child)
{
    Group::removeChild(child);
    active_ = children_.begin();
}


void Switch::setActiveChild(Node *child)
{
    setActiveChild( std::find_if(children_.begin(), children_.end(), hasId(child->id())) );
}

void Switch::setActiveChild(NodeSet::iterator n)
{
    if ( n != children_.end())
        active_ = n;
    else
        active_ = children_.begin();
}

NodeSet::iterator Switch::activeChild() const
{
    return active_;
}

void Switch::setIndexActiveChild(int index)
{
    int i = 0;
    for (NodeSet::iterator node = children_.begin();
         node != children_.end(); node++, i++) {
        if ( i == index ) {
            active_ = node;
            break;
        }
    }
}

int Switch::getIndexActiveChild() const
{
    int index = 0;
    for (NodeSet::iterator node = children_.begin();
         node != children_.end(); node++, index++) {
        if (node == active_)
            break;
    }
    return index;
}


void Scene::accept(Visitor& v)
{
    v.visit(*this);
}
