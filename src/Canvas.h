#ifndef CANVAS_H
#define CANVAS_H

#include "View/View.h"
#include "FrameBuffer.h"
#include "Source/SourceList.h"

class FrameBufferSurface;

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

    void setFrameBuffer(FrameBuffer *fb);
    void update();

    void addCanvas();
    void removeCanvas();

    Group *canvasScene() const { return canvas_scene_; }
    
    SourceList::iterator canvasBegin ();
    SourceList::iterator canvasEnd ();
    size_t numCanvases () const { return canvases_.size(); }

protected:

    SourceList canvases_;
    Group *canvas_scene_;
    FrameBufferSurface *canvas_surface_;

};

#endif // CANVAS_H
