#ifndef ACTIONMANAGER_H
#define ACTIONMANAGER_H


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

    void store(const std::string &label);

    void clear();
    void undo();
    void redo();
    void stepTo(uint target);

    inline uint current() const { return step_; }
    inline uint max() const { return max_step_; }

    std::string label(uint s) const;

private:

    void restore(uint target);

    tinyxml2::XMLDocument xmlDoc_;
    uint step_;
    uint max_step_;
    std::atomic<bool> locked_;
};


#endif // ACTIONMANAGER_H
