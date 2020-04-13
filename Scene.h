#ifndef SCENE_H
#define SCENE_H

#define INVALID_ID -1

#ifdef __APPLE__
#include <sys/types.h>
#endif
#include <set>
#include <vector>
#include <glm/glm.hpp>

glm::mat4 transform(glm::vec3 translation, glm::vec3 rotation, glm::vec3 scale);

// Forward declare classes referenced
class Shader;
class Visitor;

// Base virtual class for all Node types
// Manages modelview transformations and culling
class Node {

    int       id_;
    bool      initialized_;

public:
    Node();
    virtual ~Node () {}

    // unique identifyer generated at instanciation
    inline int id () const { return id_; }

    // must initialize the node before draw
    virtual void init() { initialized_ = true; }
    virtual bool initialized () { return initialized_; }

    // pure virtual draw : to be instanciated to define node behavior
    virtual void draw (glm::mat4 modelview, glm::mat4 projection) = 0;

    // update every frame
    virtual void update (float dt);

    // accept all kind of visitors
    virtual void accept (Visitor& v);

    // public members, to manipulate with care
    Node*     parent_;
    bool      visible_;

    glm::mat4 transform_;
    glm::vec3 scale_, rotation_, translation_;
};

// Leaf Nodes are primitives that can be rendered
class Primitive : public Node {

public:
    Primitive() : Node(), shader_(nullptr), vao_(0), drawMode_(0), drawCount_(0) {}
    virtual ~Primitive();

    virtual void init () override;
    virtual void accept (Visitor& v) override;
    virtual void draw (glm::mat4 modelview, glm::mat4 projection) override;

    inline Shader *getShader() const { return shader_; }
    inline void setShader( Shader* e ) { shader_ = e; }

protected:
    Shader*   shader_;
    uint vao_, drawMode_, drawCount_;
    std::vector<glm::vec3>     points_;
    std::vector<glm::vec4>     colors_;
    std::vector<glm::vec2>     texCoords_;
    std::vector<uint>  indices_;
    virtual void deleteGLBuffers_();
};

// Other Nodes establish hierarchy with a group of nodes

struct z_comparator
{
    inline bool operator () (const Node *a, const Node *b) const
    {
        //Sort Furthest to Closest
        return (a && b && a->translation_.z < b->translation_.z);
    }
};
struct hasId: public std::unary_function<Node*, bool>
{
    inline bool operator()(const Node* e) const
    {
       return (e && e->id() == _id);
    }
    hasId(int id) : _id(id) { }
private:
    int _id;
};
typedef std::multiset<Node*, z_comparator> NodeSet;


class Group : public Node {

public:
    Group() : Node() {}
    virtual ~Group();

    virtual void update (float dt) override;
    virtual void accept (Visitor& v) override;
    virtual void draw (glm::mat4 modelview, glm::mat4 projection) override;

    virtual void addChild (Node *child);
    virtual void removeChild (Node *child);

    NodeSet::iterator begin();
    NodeSet::iterator end();
    uint numChildren() const;

protected:
    NodeSet children_;
};

class Switch : public Group {

public:
    Switch() : Group(), active_(end()) {}

    virtual void update (float dt) override;
    virtual void accept (Visitor& v) override;
    virtual void draw (glm::mat4 modelview, glm::mat4 projection) override;

    void addChild (Node *child) override;
    void removeChild (Node *child) override;

    void setActiveChild (Node *child);
    void setActiveChild (NodeSet::iterator n);
    NodeSet::iterator activeChild () const;

    void setIndexActiveChild (int index);
    int getIndexActiveChild () const;

protected:
    NodeSet::iterator active_;
};

// Animation Nodes
class Animation : public Group {

public:
    Animation() : Group(), axis_(glm::vec3(0.f, 0.f, 1.f)), speed_(0.f), radius_(1.f) { }

    virtual void init () override;
    virtual void update (float dt) override;
    virtual void accept (Visitor& v) override;

    // circular path
    glm::vec3 axis_;
    float speed_;
    float radius_;

protected:
    Node *child_;
    glm::mat4 animation_;
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
