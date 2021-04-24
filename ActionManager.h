#ifndef ACTIONMANAGER_H
#define ACTIONMANAGER_H

#include <list>
#include <atomic>

#include <tinyxml2.h>

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
    void init ();

    // Undo History
    void store (const std::string &label);
    void undo ();
    void redo ();
    void stepTo (uint target);

    inline uint current () const { return history_step_; }
    inline uint max () const { return history_max_step_; }
    std::string label (uint s) const;
    FrameBufferImage *thumbnail (uint s) const;

    // Snapshots
    void snapshot (const std::string &label);

    std::list<uint64_t> snapshots () const;
    uint64_t currentSnapshot () const { return snapshot_id_; }

    void open (uint64_t snapshotid);
    void replace (uint64_t snapshotid = 0);
    void restore (uint64_t snapshotid = 0);
    void remove  (uint64_t snapshotid = 0);

    std::string label (uint64_t snapshotid) const;
    void setLabel (uint64_t snapshotid, const std::string &label);
    FrameBufferImage *thumbnail (uint64_t snapshotid) const;

    float interpolation ();
    void interpolate (float val, uint64_t snapshotid = 0);

private:

    tinyxml2::XMLDocument history_doc_;
    uint history_step_;
    uint history_max_step_;
    std::atomic<bool> locked_;
    void restore(uint target);

    uint64_t snapshot_id_;
    tinyxml2::XMLElement *snapshot_node_;

    Interpolator *interpolator_;
    tinyxml2::XMLElement *interpolator_node_;

};


#endif // ACTIONMANAGER_H
