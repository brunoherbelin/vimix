#ifndef VISITOR_H
#define VISITOR_H

#include <string>

// Declares the interface for the visitors
class Visitor {

public:
    // Need to declare overloads for basic kind of Nodes to visit
    virtual void visit (class Scene&) = 0;
    virtual void visit (class Node&) = 0;
    virtual void visit (class Primitive&) = 0;
    virtual void visit (class Group&) = 0;
    virtual void visit (class Switch&) = 0;

    // not mandatory for all others
    virtual void visit (class Surface&) {}
    virtual void visit (class ImageSurface&) {}
    virtual void visit (class FrameBufferSurface&) {}
    virtual void visit (class LineStrip&)  {}
    virtual void visit (class LineSquare&) {}
    virtual void visit (class Mesh&) {}
    virtual void visit (class Frame&) {}
    virtual void visit (class Handles&) {}
    virtual void visit (class Symbol&) {}
    virtual void visit (class Disk&) {}
    virtual void visit (class Character&) {}
    virtual void visit (class Shader&) {}
    virtual void visit (class ImageShader&) {}
    virtual void visit (class MaskShader&) {}
    virtual void visit (class ImageProcessingShader&) {}

    // utility
    virtual void visit (class Stream&) {}
    virtual void visit (class MediaPlayer&) {}
    virtual void visit (class MixingGroup&) {}
    virtual void visit (class Source&) {}
    virtual void visit (class MediaSource&) {}
    virtual void visit (class StreamSource&) {}
    virtual void visit (class NetworkSource&) {}
    virtual void visit (class SrtReceiverSource&) {}
    virtual void visit (class GenericStreamSource&) {}
    virtual void visit (class DeviceSource&) {}
    virtual void visit (class ScreenCaptureSource&) {}
    virtual void visit (class PatternSource&) {}
    virtual void visit (class SessionFileSource&) {}
    virtual void visit (class SessionGroupSource&) {}
    virtual void visit (class RenderSource&) {}
    virtual void visit (class CloneSource&) {}
    virtual void visit (class MultiFileSource&) {}
    virtual void visit (class TextSource&) {}
    virtual void visit (class ShaderSource&) {}

    virtual void visit (class FrameBufferFilter&) {}
    virtual void visit (class PassthroughFilter&) {}
    virtual void visit (class DelayFilter&) {}
    virtual void visit (class ResampleFilter&) {}
    virtual void visit (class BlurFilter&) {}
    virtual void visit (class SharpenFilter&) {}
    virtual void visit (class SmoothFilter&) {}
    virtual void visit (class EdgeFilter&) {}
    virtual void visit (class AlphaFilter&) {}
    virtual void visit (class ImageFilter&) {}

    virtual void visit (class SourceCallback&) {}
    virtual void visit (class ValueSourceCallback&) {}
    virtual void visit (class SetAlpha&) {}
    virtual void visit (class SetDepth&) {}
    virtual void visit (class SetGeometry&) {}
    virtual void visit (class SetGamma&) {}
    virtual void visit (class Loom&) {}
    virtual void visit (class Grab&) {}
    virtual void visit (class Resize&) {}
    virtual void visit (class Turn&) {}
    virtual void visit (class Play&) {}
    virtual void visit (class PlayFastForward&) {}
    virtual void visit (class Seek&) {}
};


#endif // VISITOR_H
