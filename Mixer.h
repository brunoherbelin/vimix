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
    Mixer(Mixer const& copy);            // Not Implemented
    Mixer& operator=(Mixer const& copy); // Not Implemented

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
    void setCurrentSource(Node *node);
    void setCurrentSource(Source *s);
    void unsetCurrentSource();

    Source *currentSource();

    // management of view
    View *getView(View::Mode m);
    void setCurrentView(View::Mode m);
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
