
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

// Node
Node::Node() : parent_(nullptr), visible_(false)
{
    worldToLocal_ = glm::identity<glm::mat4>();
    localToWorld_ = glm::identity<glm::mat4>();
    transform_ = glm::identity<glm::mat4>();
}

void Node::update( float dt )
{
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
    int sizeofIndices = indices_.size()*sizeof(unsigned int);
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

    drawingPrimitive_ = GL_TRIANGLE_STRIP;
    visible_ = true;
}

void Primitive::draw(glm::mat4 modelview, glm::mat4 projection)
{
    if ( visible_ ) {

      if (shader_) {
          shader_->projection = projection;
          shader_->modelview = modelview * transform_;
          shader_->use();
      }

      //
      // draw
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
    if ( arrayBuffer_ ) glDeleteBuffers ( 1, &arrayBuffer_);
    if ( elementBuffer_ ) glDeleteBuffers ( 1, &elementBuffer_);
    if ( vao_ ) glDeleteVertexArrays ( 1, &vao_);
}

// Group

Group::~Group()
{
    children_.clear();
}

void Group::accept(Visitor& v)
{
    Node::accept(v);
    v.visit(*this);
}

void Group::init()
{
    visible_ = true;
}

void Group::update( float dt )
{
    Node::update(dt);

    // update every child node
    for (std::vector<Node*>::iterator node = children_.begin();
         node != children_.end(); node++) {
        (*node)->update ( dt );
    }
}

void Group::draw(glm::mat4 modelview, glm::mat4 projection)
{
    if ( visible_ ) {

        // append the instance transform to the ctm
        glm::mat4 ctm = modelview * transform_;

        // draw every child node
        for (std::vector<Node*>::iterator node = children_.begin();
             node != children_.end(); node++) {
            (*node)->draw ( ctm, projection );
        }

    }
}


void Group::addChild(Node *child)
{
    children_.push_back( child );
    child->parent_ = this;
}

Node *Group::getChild(int i)
{
    if ( i >= 0 && i < children_.size() )
      return children_[i];
    else
      return nullptr;
}

int Group::numChildren()
{
    return children_.size();
}


void Scene::accept(Visitor& v) {
    v.visit(*this);
}
