#ifndef UPDATECALLBACK_H
#define UPDATECALLBACK_H

#include "GlmToolkit.h"

class Node;

class UpdateCallback
{

public:
    UpdateCallback();

    virtual void update(Node *, float) = 0;

    inline bool finished() const { return finished_; }
    inline bool enabled() const { return enabled_; }

protected:
    bool enabled_;
    bool finished_;
};

class MoveToCenterCallback : public UpdateCallback
{
    float duration_;
    float progress_;
    bool initialized_;
    glm::vec3 initial_position_;

public:
    MoveToCenterCallback(float duration = 1000.f) : duration_(duration), progress_(0.f), initialized_(false) {}

    void update(Node *n, float dt);

    inline void reset() { initialized_ = false; }
};

class BounceScaleCallback : public UpdateCallback
{
    float duration_;
    float progress_;
    bool initialized_;
    glm::vec3 initial_scale_;

public:
    BounceScaleCallback(float duration = 100.f) : duration_(duration), progress_(0.f), initialized_(false) {}

    void update(Node *n, float dt);
};

#endif // UPDATECALLBACK_H
