#ifndef DECORATIONS_H
#define DECORATIONS_H


#include <string>
#include <glm/glm.hpp>

#include "Primitives.h"
#include "Mesh.h"

class Frame : public Node
{
public:

    typedef enum { ROUND_THIN = 0, ROUND_LARGE, SHARP_THIN, SHARP_LARGE, ROUND_SHADOW } Type;
    Frame(Type type);
    ~Frame();

    void update (float dt) override;
    void draw (glm::mat4 modelview, glm::mat4 projection) override;
    void accept (Visitor& v) override;

    Type type() const { return type_; }
    Mesh *border() const { return side_; }
    glm::vec4 color;

protected:
    Type type_;
    Mesh *side_;
    Mesh *top_;
    Mesh *shadow_;
    LineSquare *square_;
};

class Handles : public Node
{
public:
    typedef enum { RESIZE = 0, RESIZE_H, RESIZE_V, ROTATE } Type;
    Handles(Type type);
    ~Handles();

    void update (float dt) override;
    void draw (glm::mat4 modelview, glm::mat4 projection) override;
    void accept (Visitor& v) override;

    Type type() const { return type_; }
    Primitive *handle() const { return handle_; }
    glm::vec4 color;

protected:
    Primitive *handle_;
    Type type_;

};

class Icon : public Node
{
public:
    typedef enum { GENERIC = 0, IMAGE, VIDEO, SESSION, CLONE, RENDER, EMPTY } Type;
    Icon(Type type);
    ~Icon();

    void draw (glm::mat4 modelview, glm::mat4 projection) override;
    void accept (Visitor& v) override;

    Type type() const { return type_; }
    glm::vec4 color;

protected:
    Mesh *icon_;
    Type type_;
};

// TODO Shadow mesh with unique vao


#endif // DECORATIONS_H
