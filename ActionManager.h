#ifndef ACTIONMANAGER_H
#define ACTIONMANAGER_H


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

    void clear();
    void capture();

    void undo();
    void redo();

private:

};

#endif // ACTIONMANAGER_H
