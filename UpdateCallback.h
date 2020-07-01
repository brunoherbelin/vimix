#ifndef UPDATECALLBACK_H
#define UPDATECALLBACK_H

#include "GlmToolkit.h"

class Node;

class UpdateCallback
{
    bool enabled_;

public:
    UpdateCallback();

    virtual void update(Node *, float) = 0;

    inline bool enabled() const {return enabled_;}
    inline void enable() { enabled_ = true; }
    inline void disable() { enabled_ = false; }
};

class MoveToCenterCallback : public UpdateCallback
{
    float duration_;
    bool initialized_;
    glm::vec3 initial_position_;

public:
    MoveToCenterCallback(float duration = 1000.f) : duration_(duration), initialized_(false) {}

    void update(Node *n, float dt);

    inline void reset() { initialized_ = false; }
};

#endif // UPDATECALLBACK_H
