#ifndef MIXER_H
#define MIXER_H


//  GStreamer
#include <gst/gst.h>


#include "View.h"
#include "Session.h"
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

    // update session and all views
    void update();

    // draw session and current view
    void draw();

    // manangement of sources
    void createSourceFile(std::string path);
    void createSourceRender();
    void createSourceClone(std::string namesource);

    // operations on sources
    void renameSource(Source *s, const std::string &newname);
    void deleteSource(Source *s);

    // current source
    void setCurrentSource(std::string namesource);
    void setCurrentSource(Node *node);
    void setCurrentSource(int index);
    void setCurrentSource(Source *s);
    void unsetCurrentSource();
    void deleteCurrentSource();
    Source *currentSource();
    int indexCurrentSource();

    // management of view
    View *view(View::Mode m);
    void setCurrentView(View::Mode m);
    View *currentView();

    // manipulate, load and save sessions
    inline Session *session() const { return session_; }
    void clear();
    void save();
    void saveas(const std::string& filename);
    void open(const std::string& filename);
    void import(const std::string& filename) {}
    void import(Session *s) {}
    void set(Session *s);

protected:

    Session *session_;
    Session *back_session_;
    void swap();

    void insertSource(Source *s);
    void setCurrentSource(SourceList::iterator it);
    SourceList::iterator current_source_;
    int current_source_index_;

    MixingView mixing_;
    GeometryView geometry_;
    LayerView layer_;
    View *current_view_;

    gint64 update_time_;

};

#endif // MIXER_H
