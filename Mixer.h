#ifndef MIXER_H
#define MIXER_H

#include "View.h"
#include "Session.h"
#include "Selection.h"

class SessionSource;

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
    inline float fps() const { return fps_;}

    // draw session and current view
    void draw();

    // creation of sources
    Source * createSourceFile   (const std::string &path);
    Source * createSourceClone  (const std::string &namesource = "");
    Source * createSourceRender ();
    Source * createSourceStream (const std::string &gstreamerpipeline);
    Source * createSourcePattern(uint pattern, glm::ivec2 res);
    Source * createSourceDevice (const std::string &namedevice);
    Source * createSourceNetwork(const std::string &nameconnection);
    Source * createSourceGroup  ();

    // operations on sources
    void addSource    (Source *s);
    void deleteSource (Source *s, bool withundo=true);
    void renameSource (Source *s, const std::string &newname);
    void attach       (Source *s);
    void detach       (Source *s);
    void deselect     (Source *s);
    void deleteSelection();
    void groupSelection();

    // current source
    Source * currentSource ();
    void setCurrentSource (Source *s);
    void setCurrentSource (std::string namesource);
    void setCurrentSource (Node *node);
    void setCurrentSource (uint64_t id);
    void setCurrentNext ();
    void setCurrentPrevious ();
    void unsetCurrentSource ();

    void setCurrentIndex (int index);
    int  indexCurrentSource ();

    // browsing into sources
    Source * findSource (Node *node);
    Source * findSource (std::string name);
    Source * findSource (uint64_t id);

    // management of view
    View *view   (View::Mode m = View::INVALID);
    void setView (View::Mode m);

    void conceal  (Source *s);
    void uncover  (Source *s);
    bool concealed(Source *s);

    // manipulate, load and save sessions
    inline Session *session () const { return session_; }
    void clear  ();
    void save   ();
    void saveas (const std::string& filename);
    void load   (const std::string& filename);
    void import (const std::string& filename);
    void import (SessionSource *source);
    void merge  (Session *session);
    void merge  (SessionSource *source);
    void set    (Session *session);

    // operations depending on transition mode
    void close  ();
    void open   (const std::string& filename);

    // create sources if clipboard contains well-formed xml text
    void paste  (const std::string& clipboard);

protected:

    Session *session_;
    Session *back_session_;
    std::list<Session *> garbage_;
    bool sessionSwapRequested_;
    void swap();

    SourceList candidate_sources_;
    SourceList stash_;
    void insertSource  (Source *s, View::Mode m = View::INVALID);
    bool replaceSource (Source *from, Source *to);
    bool recreateSource(Source *s);

    void setCurrentSource(SourceList::iterator it);
    SourceList::iterator current_source_;
    int current_source_index_;

    View *current_view_;
    MixingView mixing_;
    GeometryView geometry_;
    LayerView layer_;
    AppearanceView appearance_;
    TransitionView transition_;

    float dt_;
    float fps_;
};

#endif // MIXER_H
