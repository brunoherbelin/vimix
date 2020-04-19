#ifndef VISITOR_H
#define VISITOR_H

#include <string>

// Forward declare different kind of Node
class Node;
class Group;
class Switch;
class Animation;
class Primitive;
class Scene;
class ImageSurface;
class MediaSurface;
class FrameBufferSurface;
class LineStrip;
class LineSquare;
class LineCircle;
class Mesh;
class MediaPlayer;
class Shader;
class ImageShader;

// Declares the interface for the visitors
class Visitor {

public:
    // Declare overloads for each kind of Node to visit
    virtual void visit (Scene&) = 0;
    virtual void visit (Node&) = 0;
    virtual void visit (Group&) = 0;
    virtual void visit (Switch&) = 0;
    virtual void visit (Animation&) = 0;
    virtual void visit (Primitive&) = 0;
    virtual void visit (ImageSurface&) = 0;
    virtual void visit (MediaSurface&) = 0;
    virtual void visit (FrameBufferSurface&) = 0;
    virtual void visit (LineStrip&) = 0;
    virtual void visit (LineSquare&) = 0;
    virtual void visit (LineCircle&) = 0;
    virtual void visit (Mesh&) = 0;

    virtual void visit (MediaPlayer&) = 0;
    virtual void visit (Shader&) = 0;
    virtual void visit (ImageShader&) = 0;

};


#endif // VISITOR_H
