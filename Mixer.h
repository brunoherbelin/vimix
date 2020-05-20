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

    void deleteSource(Source *s);
    void deleteCurrentSource();

    // operations on sources
    void renameSource(Source *s, const std::string &newname);

    // current source
    void setCurrentSource(std::string namesource);
    void setCurrentSource(Node *node);
    void setCurrentSource(int index);
    void setCurrentSource(Source *s);
    void unsetCurrentSource();
    Source *currentSource();
    int indexCurrentSource();

    // management of view
    View *getView(View::Mode m);
    void setCurrentView(View::Mode m);
    View *currentView();

    void setSession(Session *s) { session_ = s; }
    Session *session() { return session_; }

    // load and save session
    void open(const std::string& filename);
    void saveas(const std::string& filename);
    void save();
    void newSession();

protected:

    Session *session_;
    Session *back_session_;
    void swap();

//    void validateFilename(const std::string& filename);
    std::string sessionFilename_;

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
