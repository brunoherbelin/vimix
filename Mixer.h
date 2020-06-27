#ifndef MIXER_H
#define MIXER_H


//  GStreamer
#include <gst/gst.h>


#include "View.h"
#include "Session.h"
#include "Selection.h"

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

    static Selection& selection()
    {
        // The only instance
        static Selection _selection;
        return _selection;
    }

    // update session and all views
    void update();

    // draw session and current view
    void draw();
    inline float dt() const { return dt_;}

    // creation of sources
    Source * createSourceFile   (std::string path);
    Source * createSourceClone  (std::string namesource = "");
    Source * createSourceRender ();

    // operations on sources
    void addSource    (Source *s);
    void deleteSource (Source *s);
    void renameSource (Source *s, const std::string &newname);

    // current source
    void setCurrentSource (Source *s);
    void setCurrentSource (std::string namesource);
    void setCurrentSource (Node *node);
    void setCurrentSource (int index);
    void setCurrentNext ();
    void unsetCurrentSource ();
    int  indexCurrentSource ();
    Source * currentSource ();

    // browsing into sources
    Source * findSource (Node *node);
    Source * findSource (std::string name);

    // management of view
    View *view   (View::Mode m = View::INVALID);
    void setView (View::Mode m);

    // manipulate, load and save sessions
    inline Session *session () const { return session_; }
    void clear  ();
    void save   ();
    void saveas (const std::string& filename);
    void open   (const std::string& filename);
    void import (const std::string& filename);
    void merge  (Session *s);
    void set    (Session *s);

protected:

    Session *session_;
    Session *back_session_;
    void swap();

    SourceList candidate_sources_;
    void insertSource(Source *s, bool makecurrent = true);

    void setCurrentSource(SourceList::iterator it);
    SourceList::iterator current_source_;
    int current_source_index_;

    MixingView mixing_;
    GeometryView geometry_;
    LayerView layer_;
    View *current_view_;

    gint64 update_time_;
    float dt_;
};

#endif // MIXER_H
