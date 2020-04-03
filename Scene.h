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
    Primitive() : Node(), shader_(nullptr), vao_(0), drawingPrimitive_(0) {}
    virtual ~Primitive();

    virtual void init () override;
    virtual void draw ( glm::mat4 modelview, glm::mat4 projection) override;
    virtual void accept(Visitor& v) override;

    Shader *getShader() { return shader_; }
    void setShader( Shader* e ) { shader_ = e; }

protected:
    Shader*   shader_;
    uint vao_;
    uint drawingPrimitive_;
    std::vector<glm::vec3>     points_;
    std::vector<glm::vec3>     colors_;
    std::vector<glm::vec2>     texCoords_;
    std::vector<uint>  indices_;
    virtual void deleteGLBuffers_();
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

    void addChild ( Node *child );
    Node* getChild ( uint i );
    uint numChildren() const;

protected:
    std::vector< Node* > children_;
};

class Switch : public Group {

public:
    Switch() : Group(), active_(0) {}

    virtual void update ( float dt ) override;
    virtual void draw ( glm::mat4 modelview, glm::mat4 projection) override;
    virtual void accept(Visitor& v) override;

    void setActiveIndex(uint index);
    uint activeIndex() const { return active_; }
    Node* activeNode();

protected:
    uint active_;
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
