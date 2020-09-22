#ifndef MIXER_H
#define MIXER_H

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
    inline float dt() const { return dt_;}

    // draw session and current view
    void draw();

    // creation of sources
    Source * createSourceFile   (const std::string &path);
    Source * createSourceClone  (const std::string &namesource = "");
    Source * createSourceRender ();
    Source * createSourceStream (const std::string &gstreamerpipeline);
    Source * createSourcePattern(int pattern, glm::ivec2 res);
    Source * createSourceDevice (int id);

    // operations on sources
    void addSource    (Source *s);
    void deleteSource (Source *s);
    void renameSource (Source *s, const std::string &newname);
    void deleteSelection();

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
    void load   (const std::string& filename);
    void import (const std::string& filename);
    void merge  (Session *s);
    void set    (Session *s);

    // operations depending on transition mode
    void close  ();
    void open   (const std::string& filename);

protected:

    Session *session_;
    Session *back_session_;
    std::list<Session *> garbage_;
    bool sessionSwapRequested_;
    void swap();

    SourceList candidate_sources_;
    void insertSource(Source *s, View::Mode m = View::INVALID);

    void setCurrentSource(SourceList::iterator it);
    SourceList::iterator current_source_;
    int current_source_index_;

    View *current_view_;
    MixingView mixing_;
    GeometryView geometry_;
    LayerView layer_;
    TransitionView transition_;

    guint64 update_time_;
    float dt_;
};

#endif // MIXER_H
