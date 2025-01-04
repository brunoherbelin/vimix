#ifndef MIXER_H
#define MIXER_H

#include "GeometryView.h"
#include "MixingView.h"
#include "LayerView.h"
#include "TextureView.h"
#include "TransitionView.h"
#include "DisplaysView.h"
#include "Session.h"
#include "Selection.h"

namespace tinyxml2 {
class XMLElement;
}

class SessionSource;

class Mixer
{
    // Private Constructor
    Mixer();
    Mixer(Mixer const& copy) = delete;
    Mixer& operator=(Mixer const& copy) = delete;

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
    void update ();
    inline float dt () const { return dt_; } // in miliseconds
    inline int fps  () const { return int(roundf(1000.f/dt__)); }

    // draw session and current view
    void draw ();

    // creation of sources
    Source * createSourceFile   (const std::string &path, bool disable_hw_decoding = false);
    Source * createSourceMultifile(const std::list<std::string> &list_files, uint fps);
    class CloneSource *createSourceClone(const std::string &namesource = "", bool copy_attributes = true);
    Source * createSourceRender ();
    Source * createSourceStream (const std::string &gstreamerpipeline);
    Source * createSourcePattern(uint pattern, glm::ivec2 res);
    Source * createSourceDevice (const std::string &namedevice);
    Source * createSourceScreen (const std::string &namedevice);
    Source * createSourceNetwork(const std::string &nameconnection);
    Source * createSourceSrt    (const std::string &ip, const std::string &port);
    Source * createSourceText   (const std::string &contents, glm::ivec2 res);
    Source * createSourceGroup  ();

    // operations on sources
    void addSource    (Source *s);
    void deleteSource (Source *s);
    void replaceSource(Source *previous, Source *s);
    void renameSource (Source *s, const std::string &newname = "");
    int  numSource    () const;

    // operations on selection
    void deleteSelection ();
    void groupSelection  ();
    void groupCurrent    ();
    void groupAll (bool only_active = false);
    void ungroupAll ();
    void groupSession ();

    // current source
    Source *currentSource ();
    void setCurrentSource (Source *s);
    void setCurrentSource (std::string namesource);
    void setCurrentSource (Node *node);
    void setCurrentSource (uint64_t id);
    void setCurrentNext   ();
    void setCurrentPrevious ();
    void unsetCurrentSource ();

    // access though indices
    Source *sourceAtIndex (int index);
    void setCurrentIndex  (int index);
    void moveIndex        (int current_index, int target_index);
    int  indexCurrentSource () const;

    // browsing into sources
    Source * findSource    (Node *node);
    Source * findSource    (std::string name);
    Source * findSource    (uint64_t id);
    SourceList findSources (float depth_from, float depth_to);
    SourceList validate (const SourceList &list);

    // management of view
    View * view  (View::Mode m = View::INVALID);
    void setView (View::Mode m);

    void conceal  (Source *s);
    void uncover  (Source *s);
    bool concealed(Source *s);

    // manipulate, load and save sessions
    inline Session * session () const { return session_; }
    void clear  ();
    void save   (bool with_version = false);
    void saveas (const std::string& filename, bool with_version = false);
    void load   (const std::string& filename);
    void import (const std::string& filename);
    void import (SessionSource *source);
    void merge  (Session *session);
    void merge  (SessionSource *source);
    void set    (Session *session);
    void setResolution(glm::vec3 res);
    bool busy   () const { return busy_; }


    // operations depending on transition mode
    void close  (bool smooth = false);
    void open   (const std::string& filename, bool smooth = false);

    // create sources if clipboard contains well-formed xml text
    void paste  (const std::string& clipboard);

    // version and undo management
    void restore(tinyxml2::XMLElement *sessionNode);

protected:

    Session *session_;
    Session *back_session_;
    std::list<Session *> garbage_;
    bool sessionSwapRequested_;
    void swap();

    // temporary buffer of sources to be inserted at next iteration,
    // stored in pair with the source to replace, if provided
    std::list< std::pair<Source *, Source *> > candidate_sources_;
    SourceList stash_;
    void insertSource  (Source *s, View::Mode m = View::INVALID);
    bool recreateSource(Source *s);
    void attachSource  (Source *s);
    void detachSource  (Source *s);
    bool attached      (Source *s) const;
    void group         (SourceList sources);

    void setCurrentSource(SourceList::iterator it);
    SourceList::iterator current_source_;
    int current_source_index_;

    View *current_view_;
    MixingView mixing_;
    GeometryView geometry_;
    LayerView layer_;
    TextureView appearance_;
    TransitionView transition_;
    DisplaysView displays_;

    bool busy_;
    float dt_;
    float dt__;
};

#endif // MIXER_H
