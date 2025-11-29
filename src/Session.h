#ifndef SESSION_H
#define SESSION_H

#include <mutex>

#include "Source/SourceList.h"
#include "View/RenderView.h"
#include "Metronome.h"

namespace tinyxml2 {
class XMLDocument;
}
class FrameGrabber;
class MixingGroup;
class SourceCallback;
class CallbackGenerator;

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

#define SNAPSHOT_NODE(i) std::to_string(i).insert(0,1,'S')

struct SessionSnapshots {

    tinyxml2::XMLDocument *xmlDoc_;
    std::list<uint64_t> keys_;
    std::mutex access_;
};

class Session
{
    friend class RenderSource;

public:
    Session(uint64_t id = 0);
    ~Session();

    // Get unique id
    inline uint64_t id () const { return id_; }

    static Session *load(const std::string& filename, uint level = 0);
    static std::string save(const std::string& filename, Session *session, const std::string& snapshot_name = "");

    // add given source into the session
    SourceList::iterator addSource (Source *s);

    // delete the source s from the session
    SourceList::iterator deleteSource (Source *s);

    // remove this source from the session
    // Does not delete the source
    SourceList::iterator removeSource (Source *s);

    // attach or detach from rendering
    void attachSource  (Source *s);
    void detachSource  (Source *s);
    bool attached      (Source *s) const;

    // get ptr to front most source and remove it from the session
    // Does not delete the source
    Source *popSource ();

    // management of list of sources
    bool empty() const;
    uint size() const;
    SourceList getDepthSortedList () const;
    SourceIdList getIdList() const;
    std::list<std::string> getNameList(uint64_t exceptid=0) const;
    SourceList::iterator begin ();
    SourceList::iterator end ();
    SourceList::iterator find (Source *s);
    SourceList::iterator find (std::string name);
    SourceList::iterator find (Node *node);
    SourceList::iterator find (float depth_from, float depth_to);
    SourceList::iterator find (uint64_t id);

    // manage sources by #id (ordered index)
    SourceList::iterator at (int index);
    int index (SourceList::iterator it) const;    
    void move (int current_index, int target_index);

    // get total number of sources (recursively)
    uint numSources() const;

    // update all sources and mark sources which failed
    inline bool ready () const  { return ready_; }
    void update (float dt);
    uint64_t runtime() const;

    void execute(void (*func)(Source *));

    // update mode (active or not)
    void setActive (bool on);
    inline bool active () { return active_; }

    // return the list of sources which failed
    SourceListUnique failedSources () const { return failed_; }
    void deleteFailedSources ();

    // get frame result of render
    inline FrameBuffer *frame () const { return render_.frame(); }

    // get an newly rendered thumbnail
    inline FrameBufferImage *renderThumbnail () { return render_.thumbnail(); }

    // get / set thumbnail image
    inline FrameBufferImage *thumbnail () const { return thumbnail_; }
    void setThumbnail(FrameBufferImage *t = nullptr);
    void resetThumbnail();

    // configure rendering resolution
    void setResolution (glm::vec3 resolution, bool useAlpha = false);

    // manipulate fading of output
    void setFadingTarget (float f, float duration = 0.f);
    inline float fadingTarget () const { return fading_.target; }
    inline float fading () const { return render_.fading(); }

    // activation threshold for source (mixing distance)
    inline void setActivationThreshold(float t) { activation_threshold_ = t; }
    inline float activationThreshold() const { return activation_threshold_;}

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
    bool hasLink (SourceList sources);
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

    // snapshots
    SessionSnapshots * snapshots () { return &snapshots_; }
    void applySnapshot(uint64_t key);

    // playlists
    void addBatch(const SourceIdList &ids);
    void deleteBatch(size_t i);
    size_t numBatch() const;
    SourceList getBatch(size_t i) const;
    void addSourceToBatch(size_t i, Source *s);
    void removeSourceFromBatch(size_t i, Source *s);
    std::vector<SourceIdList> getAllBatch() { return batch_; }

    // callbacks associated to inputs
    void assignInputCallback(uint input, Target target, SourceCallback *callback);
    std::list< std::pair<Target, SourceCallback*> > getSourceCallbacks(uint input);
    void deleteInputCallback (SourceCallback *callback);
    void deleteInputCallbacks(Target target);
    void deleteInputCallbacks(uint input);
    void clearInputCallbacks ();
    std::list<uint> assignedInputs();
    bool inputAssigned(uint input);
    void swapInputCallback(uint from, uint to);
    void copyInputCallback(uint from, uint to);
    std::list<uint> inputsForSource( uint64_t id );

    void setInputSynchrony(uint input, Metronome::Synchronicity sync);
    std::vector<Metronome::Synchronicity> getInputSynchrony();
    Metronome::Synchronicity inputSynchrony(uint input);

protected:
    uint64_t id_;
    bool active_;
    float activation_threshold_;
    RenderView render_;
    std::string filename_;
    SourceListUnique failed_;
    SourceList sources_;
    void validate(SourceList &sources);
    std::list<SessionNote> notes_;
    std::list<MixingGroup *> mixing_groups_;
    std::map<View::Mode, Group*> config_;
    SessionSnapshots snapshots_;
    std::vector<SourceIdList> batch_;
    std::mutex access_;
    FrameBufferImage *thumbnail_;
    uint64_t start_time_;
    bool ready_;

    struct Fading
    {
        bool  active;
        float start;
        float target;
        float duration;
        float progress;

        Fading() {
            active = false;
            start  = 0.f;
            target = 0.f;
            duration = 0.f;
            progress = 0.f;
        }
    };
    Fading fading_;

    struct InputSourceCallback {
        bool active_;
        SourceCallback *model_;
        std::map<uint64_t, std::pair< SourceCallback *, SourceCallback *> > instances_;
        Target target_;
        InputSourceCallback() {
            active_ = false;
            model_   = nullptr;
            target_  = nullptr;
        }
        void clear();
    };
    std::multimap<uint, InputSourceCallback> input_callbacks_;
    std::vector<Metronome::Synchronicity> input_sync_;

};


#endif // SESSION_H
