#ifndef SCENE_H
#define SCENE_H

#include <vector>
#include <glm/glm.hpp>


// Forward declare classes referenced
class Shader;
class Visitor;

// Base virtual class for all Node types
// Manages modelview transformations and culling
class Node {

public:
    Node();
    virtual ~Node() {}

    virtual void init() = 0;
    virtual void draw( glm::mat4 modelview,  glm::mat4 projection) = 0;
    virtual void accept(Visitor& v);
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

    virtual void init () override;
    virtual void draw ( glm::mat4 modelview, glm::mat4 projection) override;
    virtual void accept(Visitor& v) override;

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

    virtual void init () override;
    virtual void update ( float dt ) override;
    virtual void draw ( glm::mat4 modelview, glm::mat4 projection) override;
    virtual void accept(Visitor& v) override;

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

    void accept(Visitor& v);

    Group root_;

};



#endif // SCENE_H
