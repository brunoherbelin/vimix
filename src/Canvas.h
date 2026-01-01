#ifndef CANVAS_H
#define CANVAS_H

#include <string>
#include "Source/SourceList.h"

class Group;
class FrameBuffer;
class FrameBufferSurface;
class CanvasSource;

class Canvas
{
    friend class CanvasSource;

    // Private Constructor
    Canvas();
    Canvas(Canvas const& copy) = delete;
    Canvas& operator=(Canvas const& copy) = delete;

public:

    static Canvas& manager()
    {
        // The only instance
        static Canvas _instance;
        return _instance;
    }

    void init();
    void terminate();

    // Framebuffer is passed to Canvas manager by Mixer 
    // each time the session framebuffer is changed
    void setFrameBuffer(FrameBuffer *fb);

    // canvases are updated each frame in Mixer 
    void update();

    // get scene containing all canvases
    // to be attached to GeometryView and DisplayView scenes
    Group *canvasScene() const { return canvas_scene_; }
    
    // to manage canvases
    void add();
    void remove();
    size_t size () const { return canvases_.size(); }

    // for iteration over canvases
    SourceList::iterator begin ();
    SourceList::iterator end ();

    // direct access to source
    CanvasSource *at (size_t index);

    // configuration
    void load(const std::string &filename = std::string());
    void save(const std::string &filename = std::string());
    void setLayout(int rows, int columns);

protected:

    // list of CanvasSource elements
    SourceList canvases_;

    // queried by GeometryView and DisplayView
    Group *canvas_scene_;

    // accessed by CanvasSource
    // canvas_surface_ is shared between all canvases
    // FrameBufferSurface *canvas_surface_;
    FrameBuffer *framebuffer_;

};

#endif // CANVAS_H
