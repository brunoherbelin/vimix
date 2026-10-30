#ifndef ACTIONMANAGER_H
#define ACTIONMANAGER_H

#include <list>
#include <string>
#include <mutex>

#include <tinyxml2.h>

#include "Source/SourceList.h"

#define MAX_COUNT_HISTORY 1000

class Session;
class Interpolator;
class FrameBufferImage;

class Action
{
    // Private Constructor
    Action();
    Action(Action const& copy) = delete;
    Action& operator=(Action const& copy) = delete;

public:

    static Action& manager ()
    {
        // The only instance
        static Action _instance;
        return _instance;
    }
    void init (const std::string &label);
    // capture again the initial step once the session is ready
    void refreshInit ();

    // Undo History
    void store (const std::string &label, bool threaded = true);
    void undo ();
    void redo ();
    void stepTo (uint target);

    inline uint current () const { return history_step_; }
    inline uint max () const { return history_max_step_; }
    inline uint min () const { return history_min_step_; }
    std::string label (uint s) const;
    std::string shortlabel (uint s) const;
    FrameBufferImage *thumbnail (uint s) const;

    // Snapshots
    static void takeSnapshot (Session *se, const std::string &label, bool create_thread);
    void snapshot (const std::string &label = "");
    void clearSnapshots ();
    std::list<uint64_t> snapshots () const;
    uint64_t currentSnapshot () const { return snapshot_id_; }

    void open    (uint64_t snapshotid);
    void replace (uint64_t snapshotid = 0);
    void restore (uint64_t snapshotid = 0);
    void remove  (uint64_t snapshotid = 0);
    void saveas  (const std::string& filename, uint64_t snapshotid = 0);

    std::string label (uint64_t snapshotid) const;
    std::string date  (uint64_t snapshotid) const;
    std::list<std::string> labels () const;
    void setLabel (uint64_t snapshotid, const std::string &label);
    FrameBufferImage *thumbnail (uint64_t snapshotid) const;

    // last snapshot restored (instantly or with animation)
    uint64_t restoredSnapshot () const { return restored_id_; }

    // restore with an animation of given duration (ms) from current state to the snapshot
    // (sources created by the snapshot fade in, sources removed fade out)
    void restore (uint64_t snapshotid, float duration);
    // animation of restore is ongoing
    inline bool animating () const { return animating_; }
    // progress of animation [0 1]
    float progress () const;
    // finish animation immediately (and store the result in history)
    void finish (bool store_history = true);
    // to be called at each frame (dt in ms)
    void update (float dt);
    // a source is deleted
    void forget (Source *s);

private:
    tinyxml2::XMLDocument history_doc_;
    uint history_step_;
    uint history_max_step_;
    uint history_min_step_;
    bool history_init_pending_;
    std::mutex history_access_;
    static void storeSession(Session *se, std::string label, bool thumbnail);
    void restore(uint target);

    uint64_t snapshot_id_;
    tinyxml2::XMLElement *snapshot_node_;

    uint64_t restored_id_;

    // animation of restore
    Interpolator *interpolator_;
    SourceList removed_;
    bool animating_;
    float duration_;
    float progress_;

};


#endif // ACTIONMANAGER_H
