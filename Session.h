#ifndef SESSION_H
#define SESSION_H

#include <mutex>

#include "RenderView.h"
#include "Source.h"

class FrameGrabber;
class MixingGroup;

struct SessionNote
{
    std::string label;
    std::string text;
    bool large;
    int stick;
    glm::vec2 pos;
    glm::vec2 size;

    SessionNote(const std::string &t = "", bool l = false, int s = 0);
};

class Session
{
public:
    Session();
    ~Session();

    static Session *load(const std::string& filename, uint recursion = 0);

    // add given source into the session
    SourceList::iterator addSource (Source *s);

    // delete the source s from the session
    SourceList::iterator deleteSource (Source *s);

    // remove this source from the session
    // Does not delete the source
    void removeSource(Source *s);

    // get ptr to front most source and remove it from the session
    // Does not delete the source
    Source *popSource();

    // management of list of sources
    bool empty() const;
    uint numSource() const;
    SourceList::iterator begin ();
    SourceList::iterator end ();
    SourceList::iterator find (Source *s);
    SourceList::iterator find (std::string name);
    SourceList::iterator find (Node *node);
    SourceList::iterator find (float depth_from, float depth_to);
    SourceList getDepthSortedList () const;

    SourceList::iterator find (uint64_t id);
    SourceIdList getIdList() const;

    SourceList::iterator at (int index);
    int index (SourceList::iterator it) const;    
    void move (int current_index, int target_index);

    // update all sources and mark sources which failed
    void update (float dt);

    // update mode (active or not)
    void setActive (bool on);
    inline bool active () { return active_; }

    // return the last source which failed
    Source *failedSource () { return failedSource_; }

    // get frame result of render
    inline FrameBuffer *frame () const { return render_.frame(); }

    // configure rendering resolution
    void setResolution (glm::vec3 resolution, bool useAlpha = false);

    // manipulate fading of output
    void setFading (float f, bool forcenow = false);
    inline float fading () const { return fading_target_; }

    // configuration for group nodes of views
    inline Group *config (View::Mode m) const { return config_.at(m); }

    // name of file containing this session (for transfer)
    void setFilename (const std::string &filename) { filename_ = filename; }
    std::string filename () const { return filename_; }

    // get the list of notes
    void addNote(SessionNote note = SessionNote());
    std::list<SessionNote>::iterator beginNotes ();
    std::list<SessionNote>::iterator endNotes ();
    std::list<SessionNote>::iterator deleteNote (std::list<SessionNote>::iterator n);

    // get the list of sources in mixing groups
    std::list<SourceList> getMixingGroups () const;
    // returns true if something can be done to create a mixing group
    bool canlink (SourceList sources);
    // try to link sources of the given list:
    // can either create a new mixing group or extend an existing group
    void link (SourceList sources, Group *parent = nullptr);
    // try to unlink sources of the given list:
    // can either delete an entire mixing group, or remove the given sources from existing groups
    void unlink (SourceList sources);
    // iterators for looping over mixing groups
    std::list<MixingGroup *>::iterator beginMixingGroup ();
    std::list<MixingGroup *>::iterator endMixingGroup ();
    std::list<MixingGroup *>::iterator deleteMixingGroup (std::list<MixingGroup *>::iterator g);

    // lock and unlock access (e.g. while saving)
    void lock ();
    void unlock ();

protected:
    RenderView render_;
    std::string filename_;
    Source *failedSource_;
    SourceList sources_;
    void validate(SourceList &sources);
    std::list<SessionNote> notes_;
    std::list<MixingGroup *> mixing_groups_;
    std::map<View::Mode, Group*> config_;
    bool active_;
    std::list<FrameGrabber *> grabbers_;
    float fading_target_;
    std::mutex access_;
};


#endif // SESSION_H
