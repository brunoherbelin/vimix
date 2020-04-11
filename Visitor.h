#ifndef VISITOR_H
#define VISITOR_H

#include <string>

// Forward declare different kind of Node
class Node;
class Group;
class Switch;
class Primitive;
class Scene;
class ImageSurface;
class MediaSurface;
class LineStrip;
class LineSquare;
class LineCircle;
class ObjModel;
class MediaPlayer;
class Shader;
class ImageShader;

// Declares the interface for the visitors
class Visitor {

public:
    // Declare overloads for each kind of Node to visit
    virtual void visit(Scene& n) = 0;
    virtual void visit(Node& n) = 0;
    virtual void visit(Group& n) = 0;
    virtual void visit(Switch& n) = 0;
    virtual void visit(Primitive& n) = 0;
    virtual void visit(ImageSurface& n) = 0;
    virtual void visit(MediaSurface& n) = 0;
    virtual void visit(LineStrip& n) = 0;
    virtual void visit(LineSquare& n) = 0;
    virtual void visit(LineCircle& n) = 0;
    virtual void visit(ObjModel& n) = 0;
    virtual void visit(MediaPlayer& n) = 0;
    virtual void visit(Shader& n) = 0;
    virtual void visit(ImageShader& n) = 0;

};


#endif // VISITOR_H
