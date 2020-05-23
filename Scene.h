#ifndef SCENE_H
#define SCENE_H

#define INVALID_ID -1

#ifdef __APPLE__
#include <sys/types.h>
#endif

#include <set>
#include <list>
#include <vector>

#include "GlmToolkit.h"

// Forward declare classes referenced
class Shader;
class Visitor;
class Group;

/**
 * @brief The Node class is the base virtual class for all Node types
 *
 * Every Node is given a unique id at instanciation
 *
 * Every Node has geometric operations for translation,
 * scale and rotation. The update() function computes the
 * transform_ matrix from these components.
 *
 * draw() shall be defined by the subclass.
 * The visible flag can be used to show/hide a Node.
 * To apply geometrical transformation, draw() can multiplying the
 * modelview by the transform_ matrix.
 *
 * init() shall be called on the first call of draw():
 *     if ( !initialized() )
 *        init();
 *
 * accept() allows visitors to parse the graph.
 */
class Node {

    int       id_;
    bool      initialized_;

public:
    Node ();
    virtual ~Node ();

    // unique identifyer generated at instanciation
    inline int id () const { return id_; }

    // must initialize the node before draw
    virtual void init() { initialized_ = true; }
    virtual bool initialized () { return initialized_; }

    // pure virtual draw : to be instanciated to define node behavior
    virtual void draw (glm::mat4 modelview, glm::mat4 projection) = 0;

    // update every frame
    virtual void update (float);

    // accept all kind of visitors
    virtual void accept (Visitor& v);

    void copyTransform(Node *other);

    // public members, to manipulate with care
    bool      visible_;
    uint      refcount_;
    glm::mat4 transform_;
    glm::vec3 scale_, rotation_, translation_;

};



/**
 * @brief The Primitive class is a leaf Node that can be rendered
 *
 * Primitive generates a STATIC vertex array object from
 * a list of points, colors and texture coordinates.
 * The vertex array is deleted in the Primitive destructor.
 *
 * Primitive class itself does not define any geometry: subclasses
 * should fill the points, colors and texture coordinates
 * in their constructor.
 *
 * Primitive can be given a shader that is used during draw.
 *
 */
class Primitive : public Node {

public:
    Primitive(Shader *s = nullptr) : Node(), shader_(s), vao_(0), drawMode_(0), drawCount_(0) {}
    virtual ~Primitive();

    virtual void init () override;
    virtual void accept (Visitor& v) override;
    virtual void draw (glm::mat4 modelview, glm::mat4 projection) override;

    inline Shader *shader () const { return shader_; }
    void replaceShader (Shader* newshader);

    GlmToolkit::AxisAlignedBoundingBox bbox() const { return bbox_; }

protected:
    Shader*   shader_;
    uint vao_, drawMode_, drawCount_;
    std::vector<glm::vec3>     points_;
    std::vector<glm::vec4>     colors_;
    std::vector<glm::vec2>     texCoords_;
    std::vector<uint>          indices_;
    GlmToolkit::AxisAlignedBoundingBox     bbox_;
};

//
// Other Nodes establish hierarchy with a set of nodes
//
struct z_comparator
{
    inline bool operator () (const Node *a, const Node *b) const
    {
        //Sort Furthest to Closest
        return (a && b && a->translation_.z < b->translation_.z);
    }
};
typedef std::multiset<Node*, z_comparator> NodeSet;

//typedef std::list<Node*> NodeSet;

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


/**
 * @brief The Group class contains a list of pointers to Nodes.
 *
 * A Group defines the hierarchy in the scene graph.
 *
 * The list of Nodes* is a NodeSet, a depth-sorted set
 * accepting multiple nodes at the same depth (multiset)
 *
 * update() will update all children
 * draw() will draw all children
 *
 * When a group is deleted, the children are NOT deleted.
 */
class Group : public Node {

public:
    Group() : Node() {}
    virtual ~Group();

    virtual void update (float dt) override;
    virtual void accept (Visitor& v) override;
    virtual void draw (glm::mat4 modelview, glm::mat4 projection) override;

    virtual void clear();
    virtual void attach (Node *child);
    virtual void detatch (Node *child);
    virtual void sort();

    NodeSet::iterator begin();
    NodeSet::iterator end();
    Node *front();
    Node *back();
    uint numChildren() const;

protected:
    NodeSet children_;

};

/**
 * @brief The Switch class is a Group to selectively update & draw ONE selected child
 *
 * update() will update only the active child
 * draw() will draw only the active child
 *
 */
class Switch : public Group {

public:
    Switch() : Group(), active_(end()) {}

    virtual void update (float dt) override;
    virtual void accept (Visitor& v) override;
    virtual void draw (glm::mat4 modelview, glm::mat4 projection) override;

    void attach (Node *child) override;
    void detatch (Node *child) override;

    void unsetActiveChild ();
    void setActiveChild (Node *child);
    void setActiveChild (NodeSet::iterator n);
    NodeSet::iterator activeChild () const;

    void setIndexActiveChild (int index);
    int getIndexActiveChild () const;

protected:
    NodeSet::iterator active_;
};

/**
 * @brief The Animation class is a Group with an update() function
 *
 * The update() computes a transformation of the Group to apply a movement
 * which is applied to all children.
 *
 */
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
    glm::mat4 animation_;
};


/**
 * @brief A Scene holds a root node with 3 children; a background, a workspace and a foreground
 *
 * The update() is called on the root
 *
 */
class Scene  {

    Group *root_;
    Group *background_;
    Group *workspace_;
    Group *foreground_;

public:
    Scene();
    ~Scene();

    void accept (Visitor& v);
    void update(float dt);

    void clear();
    void clearBackground();
    void clearWorkspace();
    void clearForeground();

    Group *root() { return root_; }
    Group *bg() { return background_; }
    Group *ws() { return workspace_; }
    Group *fg() { return foreground_; }

};



#endif // SCENE_H
