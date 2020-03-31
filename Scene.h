#ifndef SCENE_H
#define SCENE_H

#include <vector>

#include <glm/glm.hpp>

#include "Visitor.h"

// Forward declare classes referenced
class Shader;

// Base virtual class for all Node types
// Manages modelview transformations and culling
class Node {

public:
    Node();
    virtual ~Node() {}

    virtual void init() = 0;
    virtual void draw( glm::mat4 modelview,  glm::mat4 projection) = 0;
    virtual void accept(Visitor& v) = 0;
    virtual void update( float dt );

    virtual glm::mat4 getWorldToLocalMatrix() {return worldToLocal_;}
    virtual glm::mat4 getLocalToWorldMatrix() {return localToWorld_;}

    Node*     parent_;
    bool      visible_;
    glm::mat4 worldToLocal_;
    glm::mat4 localToWorld_;
    glm::mat4 transform_;
};


// Leaf Nodes are primitives that can be rendered
class Primitive : public Node {

public:
    Primitive() : Node(), shader_(nullptr), vao_(0), arrayBuffer_(0), elementBuffer_(0), drawingPrimitive_(0) {}
    virtual ~Primitive();

    virtual void init ();
    virtual void draw ( glm::mat4 modelview, glm::mat4 projection);
    virtual void accept(Visitor& v) override { v.visit(*this); }

    Shader *getShader() { return shader_; }
    void setShader( Shader* e ) { shader_ = e; }

protected:
    Shader*   shader_;
    unsigned int vao_;
    unsigned int arrayBuffer_;
    unsigned int elementBuffer_;
    unsigned int drawingPrimitive_;
    std::vector<glm::vec3>     points_;
    std::vector<glm::vec3>     colors_;
    std::vector<glm::vec2>     texCoords_;
    std::vector<unsigned int>  indices_;
    void   deleteGLBuffers_();
};

// Other Nodes establish hierarchy with a group of nodes
class Group : public Node {

public:
    Group() : Node() {}
    virtual ~Group();

    virtual void init ();
    virtual void update ( float dt );
    virtual void draw ( glm::mat4 modelview, glm::mat4 projection);
    virtual void accept(Visitor& v) override { v.visit(*this); }

    virtual void addChild ( Node *child );
    virtual Node* getChild ( int i );
    virtual int numChildren();

protected:
    std::vector< Node* > children_;
};

// A scene contains a root node and gives a simplified API to add nodes
class Scene  {

public:
    Scene() {}

    Group *getRoot() { return &root_; }

    void accept(Visitor& v) { v.visit(*this); }

    Group root_;

};



#endif // SCENE_H
