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
class Surface;
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
class ImageProcessingShader;

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
    virtual void visit (Surface&) = 0;
    virtual void visit (ImageSurface&) = 0;
    virtual void visit (MediaSurface&) = 0;
    virtual void visit (FrameBufferSurface&) = 0;
    virtual void visit (LineStrip&) = 0;
    virtual void visit (LineSquare&) = 0;
    virtual void visit (LineCircle&) = 0;
    virtual void visit (Mesh&) = 0;

    // not mandatory
    virtual void visit (MediaPlayer&) {}
    virtual void visit (Shader&) {}
    virtual void visit (ImageShader&) {}
    virtual void visit (ImageProcessingShader&) {}

};


#endif // VISITOR_H
