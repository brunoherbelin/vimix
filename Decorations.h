#ifndef DECORATIONS_H
#define DECORATIONS_H


#include <string>
#include <glm/glm.hpp>

#include "Primitives.h"
#include "Mesh.h"

class Frame : public Node
{
public:

    typedef enum { ROUND = 0, SHARP, GROUP } CornerType;
    typedef enum { THIN = 0, LARGE } BorderType;
    typedef enum { NONE = 0, GLOW, DROP, PERSPECTIVE } ShadowType;

    Frame(CornerType corner, BorderType border, ShadowType shadow);
    ~Frame();

    void update (float dt) override;
    void draw (glm::mat4 modelview, glm::mat4 projection) override;
    void accept (Visitor& v) override;

    glm::vec4 color;

protected:
    Mesh *right_;
    Mesh *left_;
    Mesh *top_;
    Mesh *shadow_;
    LineSquare *square_;
};

class Handles : public Node
{
public:
    typedef enum { RESIZE = 0, RESIZE_H, RESIZE_V, ROTATE, SCALE, CROP, MENU, LOCKED, UNLOCKED } Type;
    Handles(Type type);
    ~Handles();

    void update (float dt) override;
    void draw (glm::mat4 modelview, glm::mat4 projection) override;
    void accept (Visitor& v) override;

    Type type() const { return type_; }
    Primitive *handle() const { return handle_; }
    void overlayActiveCorner(glm::vec2 v) {corner_ = v;}

    glm::vec4 color;

protected:
    Mesh *handle_;
    Mesh *shadow_;
    glm::vec2 corner_;
    Type type_;
};

class Symbol : public Node
{
public:
    typedef enum { CIRCLE_POINT = 0, SQUARE_POINT, IMAGE, SEQUENCE, VIDEO, SESSION, CLONE, RENDER, GROUP, PATTERN, CAMERA, CUBE, SHARE,
                   DOTS, BUSY, LOCK, UNLOCK, EYE, EYESLASH, VECTORSLASH, ARROWS, ROTATION, CROP, CIRCLE, SQUARE, CLOCK, CLOCK_H, GRID, CROSS, EMPTY } Type;
    Symbol(Type t, glm::vec3 pos = glm::vec3(0.f));
    ~Symbol();

    void draw (glm::mat4 modelview, glm::mat4 projection) override;
    void accept (Visitor& v) override;

    GlmToolkit::AxisAlignedBoundingBox bbox() const { return symbol_->bbox(); }

    Type type() const { return type_; }
    glm::vec4 color;

protected:
    Mesh *symbol_;
    Mesh *shadow_;
    Type type_;
};

class Disk : public Node
{
public:
    Disk();

    void draw (glm::mat4 modelview, glm::mat4 projection) override;
    void accept (Visitor& v) override;

    glm::vec4 color;

protected:
    static Mesh *disk_;
};

class Glyph : public Node
{
public:
    Glyph(int imgui_font_index = 0);
    void setChar(char c);

    void draw (glm::mat4 modelview, glm::mat4 projection) override;

    glm::vec4 color;

protected:

    char character_;
    int  font_index_;
    float ar_;
    glm::mat4 uvTransform_;
    static Surface *font_;

};


#endif // DECORATIONS_H
