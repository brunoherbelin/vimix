#ifndef UPDATECALLBACK_H
#define UPDATECALLBACK_H

#include "GlmToolkit.h"

class Node;

class UpdateCallback
{

public:
    UpdateCallback();
    virtual ~UpdateCallback() {}

    virtual void update(Node *, float) = 0;

    inline bool finished() const { return finished_; }
    inline bool enabled() const { return enabled_; }

protected:
    bool enabled_;
    bool finished_;
};

class MoveToCallback : public UpdateCallback
{
    float duration_;
    float progress_;
    bool initialized_;
    glm::vec3 startingpoint_;
    glm::vec3 target_;

public:
    MoveToCallback(glm::vec3 target, float duration = 1000.f);
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
    BounceScaleCallback(float duration = 100.f);
    void update(Node *n, float dt);
};

#endif // UPDATECALLBACK_H
