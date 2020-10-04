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

    void store(const std::string &label, int id = -1);

    void clear();
    void undo();
    void redo();
    void stepTo(uint target);

private:

    void restore(uint target, int id);

    tinyxml2::XMLDocument xmlDoc_;
    uint step_;
    uint max_step_;
    std::atomic<bool> locked_;
};

#endif // ACTIONMANAGER_H
