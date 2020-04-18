#ifndef MIXER_H
#define MIXER_H

#include <vector>

//  GStreamer
#include <gst/gst.h>


#include "View.h"
#include "Source.h"

class Mixer
{
    // Private Constructor
    Mixer();
    Mixer(Rendering const& copy);            // Not Implemented
    Mixer& operator=(Rendering const& copy); // Not Implemented

public:

    static Mixer& manager()
    {
        // The only instance
        static Mixer _instance;
        return _instance;
    }

    // update all sources and all views
    void update();

    // draw render view and current view
    void draw();

    // manangement of sources
    void createSourceMedia(std::string uri);

    void setCurrentSource(std::string namesource);
    void setcurrentSource(Source *s);
    Source *currentSource();

    // management of view
    typedef enum {RENDERING = 0, MIXING=1, GEOMETRY=2, LAYER=3, INVALID=4 } viewMode;
    View *getView(viewMode m);
    void setCurrentView(viewMode m);
    View *currentView();

    inline FrameBuffer *frame() const { return render_.frameBuffer(); }

protected:
    RenderView render_;
    MixingView mixing_;
    View *current_view_;
    gint64 update_time_;

    SourceList::iterator current_source_;
};

#endif // MIXER_H
