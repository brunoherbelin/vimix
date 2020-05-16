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
class Frame;
class Handles;
class MediaPlayer;
class Shader;
class ImageShader;
class ImageProcessingShader;
class Source;
class MediaSource;

// Declares the interface for the visitors
class Visitor {

public:
    // Need to declare overloads for basic kind of Nodes to visit
    virtual void visit (Scene&) = 0;
    virtual void visit (Node&) = 0;
    virtual void visit (Primitive&) = 0;
    virtual void visit (Group&) = 0;

    // not mandatory for all others
    virtual void visit (Switch&) {}
    virtual void visit (Animation&) {}
    virtual void visit (Surface&) {}
    virtual void visit (ImageSurface&) {}
    virtual void visit (MediaSurface&) {}
    virtual void visit (FrameBufferSurface&) {}
    virtual void visit (LineStrip&)  {}
    virtual void visit (LineSquare&) {}
    virtual void visit (LineCircle&) {}
    virtual void visit (Mesh&) {}
    virtual void visit (Frame&) {}
    virtual void visit (Handles&) {}
    virtual void visit (MediaPlayer&) {}
    virtual void visit (Shader&) {}
    virtual void visit (ImageShader&) {}
    virtual void visit (ImageProcessingShader&) {}

    // utility
    virtual void visit (Source&) {}
    virtual void visit (MediaSource&) {}

};


#endif // VISITOR_H
