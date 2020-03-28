#ifndef SCENE_H
#define SCENE_H

#include <iostream>
#include <vector>

#include <glm/glm.hpp>
#include <glm/gtc/matrix_access.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

#include "Shader.h"

class Visitor;


// Base virtual class for all Node types
class Node {

public:

    Node() : parent_(), visible_(true), count_(0), localToWorld_(glm::mat4()), worldToLocal_(glm::mat4()) {}
    virtual ~ModelNode() {}

    virtual void init() = 0;
    virtual void update( float dt ) = 0;
    virtual void draw ( glm::mat4 modelview,  glm::mat4 projection) = 0;
    virtual void Accept(Visitor& dispatcher) = 0;

    virtual glm::mat4 getWorldToLocalMatrix() {return worldToLocal_;}
    virtual glm::mat4 getLocalToWorldMatrix() {return localToWorld_;}

protected:
    Node*     parent_;
    bool      visible_;
    glm::mat4 worldToLocal_;
    glm::mat4 localToWorld_;
};

// Forward declare different kind of Node
class Primitive;
class Group;

// Declares the interface for the visitors
class Visitor {
public:
    // Declare overloads for each kind of Node to visit
    virtual void Visit(Primitive& file) = 0;
    virtual void Visit(Group& file) = 0;
};

class Primitive : public Node {

public:
    Primitive() : Node(), effect(nullptr), vao_(0), arrayBuffer_(0), elementBuffer_(0), drawingPrimitive_(0) {}
    virtual ~Primitive();

    virtual void init ();
    virtual void update ( float dt );
    virtual void draw ( glm::mat4 modelview, glm::mat4 projection);
    virtual void Accept(Visitor& dispatcher) override {
        dispatcher.Visit(*this);
    }

    virtual glm::mat4 getWorldToLocalMatrix();
    virtual glm::mat4 getLocalToWorldMatrix();

    virtual void setEffect( Effect* e );
    virtual void setDrawingPrimitive ( GLuint prim );
    virtual void generateAndLoadArrayBuffer();

protected:
    Effect*   effect_;
    unsigned int vao_;
    unsigned int arrayBuffer_;
    unsigned int elementBuffer_;
    unsigned int drawingPrimitive_;
    std::vector<glm::vec3>     points_;
    std::vector<glm::vec3>     normals_;
    std::vector<unsigned int>  indices_;
    std::vector<glm::vec2>     texCoords_;
    std::vector<glm::vec3>     colors_;
    void   deleteGLBuffers_();
};

class Group : public Node {

public:
    Group() : Node() {}
    virtual ~Group();

    virtual void init ();
    virtual void update ( float dt );
    virtual void draw ( glm::mat4 modelview, glm::mat4 projection);
    virtual void Accept(Visitor& dispatcher)override {
        dispatcher.Visit(*this);
    }

    virtual glm::mat4 getWorldToLocalMatrix();
    virtual glm::mat4 getLocalToWorldMatrix();

    virtual void addChild ( Node *child );
    virtual Node* getChild ( int i );
    virtual int   numChildren();

protected:
    std::vector< Node* > children_;
};





#endif // SCENE_H
