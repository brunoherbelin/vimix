#ifndef ACTIONMANAGER_H
#define ACTIONMANAGER_H

#include <list>
#include <atomic>

#include <tinyxml2.h>



class Action
{
    // Private Constructor
    Action();
    Action(Action const& copy);            // Not Implemented
    Action& operator=(Action const& copy); // Not Implemented

public:

    static Action& manager()
    {
        // The only instance
        static Action _instance;
        return _instance;
    }
    void init(const std::string &xml = "");

    // UNDO History
    void store(const std::string &label);
    void undo();
    void redo();
    void stepTo(uint target);

    inline uint current() const { return history_step_; }
    inline uint max() const { return history_max_step_; }
    std::string label(uint s) const;

    // Snapshots
    void snapshot(const std::string &label);

    inline std::list<uint64_t> snapshots() const { return snapshots_; }
    void restore(uint64_t snapshotid);
    void remove (uint64_t snapshotid);

    std::string label(uint64_t snapshotid) const;
    void setLabel (uint64_t snapshotid, const std::string &label);

    const char *snapshotsDescription();

//    inline const tinyxml2::XMLElement *snapshotsRoot() const { return snapshots_doc_.RootElement(); }

private:

    tinyxml2::XMLDocument history_doc_;
    uint history_step_;
    uint history_max_step_;
    std::atomic<bool> locked_;
    void restore(uint target);

    tinyxml2::XMLDocument snapshots_doc_;
    std::list<uint64_t> snapshots_;

};


#endif // ACTIONMANAGER_H
