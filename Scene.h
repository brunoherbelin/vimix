#ifndef SCENE_H
#define SCENE_H

#define INVALID_ID -1

#ifdef __APPLE__
#include <sys/types.h>
#endif

#include <set>
#include <list>
#include <vector>
#include <map>

#include "UpdateCallback.h"

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

    uint64_t  id_;
    bool      initialized_;

public:
    Node ();
    virtual ~Node ();

    // unique identifyer generated at instanciation
    inline uint64_t id () const { return id_; }

    // must initialize the node before draw
    virtual void init () { initialized_ = true; }
    virtual bool initialized () { return initialized_; }

    // pure virtual draw : to be instanciated to define node behavior
    virtual void draw (glm::mat4 modelview, glm::mat4 projection) = 0;

    // update every frame
    virtual void update (float);

    // accept all kind of visitors
    virtual void accept (Visitor& v);

    void copyTransform (Node *other);

    // public members, to manipulate with care
    bool      visible_;
    uint      refcount_;
    glm::mat4 transform_;
    glm::vec3 scale_, rotation_, translation_, crop_;

    // animation update callbacks
    // list of callbacks to call at each update
    std::list<UpdateCallback *> update_callbacks_;
    void clearCallbacks();
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
 * Primitive can be given a shader th0at is used during draw.
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
    hasId(uint64_t id) : _id(id) { }
private:
    uint64_t _id;
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

    // Node interface
    virtual void update (float dt) override;
    virtual void accept (Visitor& v) override;
    virtual void draw (glm::mat4 modelview, glm::mat4 projection) override;

    // container
    void clear();
    void attach  (Node *child);
    void detach (Node *child);
    inline uint numChildren ()  const { return children_.size(); }

    // Group specific access to its Nodes
    void sort();
    NodeSet::iterator begin();
    NodeSet::iterator end();
    Node *front();
    Node *back();

protected:
    NodeSet children_;

};

/**
 * @brief The Switch class selectively updates & draws ONE selected child
 *
 * update() will update only the active child
 * draw() will draw only the active child
 *
 */
class Switch : public Node {

public:
    Switch() : Node(), active_(0) {}
    virtual ~Switch();

    // Node interface
    virtual void update (float dt) override;
    virtual void accept (Visitor& v) override;
    virtual void draw (glm::mat4 modelview, glm::mat4 projection) override;

    // container
    void clear();
    uint attach  (Node *child);
    void detatch (Node *child);
    inline uint numChildren ()  const { return children_.size(); }

    // Switch specific access to its Nodes
    void setActive (uint index);
    uint active () const { return active_; }
    Node *activeChild () const { return child(active_); }
    Node *child (uint index) const;

protected:
    uint active_;
    std::vector<Node *> children_;
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
