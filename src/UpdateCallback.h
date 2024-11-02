#ifndef UPDATECALLBACK_H
#define UPDATECALLBACK_H

#include <glm/glm.hpp>

class Node;

class UpdateCallback
{

public:
    UpdateCallback();
    virtual ~UpdateCallback() {}

    virtual void update(Node *, float) = 0;

    inline bool finished() const { return finished_; }
    inline void reset() { initialized_ = false; }

protected:
    bool finished_;
    bool initialized_;
};

class CopyCallback : public UpdateCallback
{
    Node *target_;

public:
    CopyCallback(Node *target);
    void update(Node *n, float);
};

class MoveToCallback : public UpdateCallback
{
    float duration_;
    float progress_;
    glm::vec3 startingpoint_;
    glm::vec3 target_;

public:
    MoveToCallback(glm::vec3 target, float duration = 100.f);
    void update(Node *n, float dt);

};

class RotateToCallback : public UpdateCallback
{
    float duration_;
    float progress_;
    float startingangle_;
    float target_;

public:
    RotateToCallback(float target, float duration = 100.f);
    void update(Node *n, float dt);

};

class BounceScaleCallback : public UpdateCallback
{
    float duration_;
    float scale_;
    float progress_;
    glm::vec3 initial_scale_;

public:
    BounceScaleCallback(float scale = 0.05f);
    void update(Node *n, float dt);
};

class InfiniteGlowCallback : public UpdateCallback
{
    float amplitude_;
    float time_;
    glm::vec3 initial_scale_;

public:
    InfiniteGlowCallback(float amplitude = 0.1f);
    void update(Node *n, float dt);
};

#endif // UPDATECALLBACK_H
