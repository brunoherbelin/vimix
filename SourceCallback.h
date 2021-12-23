#ifndef SOURCECALLBACK_H
#define SOURCECALLBACK_H

#include <glm/glm.hpp>

class Source;

class SourceCallback
{
public:

    typedef  enum {
        CALLBACK_GENERIC = 0,
        CALLBACK_ALPHA,
        CALLBACK_DEPTH,
        CALLBACK_PLAY,
        CALLBACK_REPLAY,
        CALLBACK_GRAB,
        CALLBACK_RESIZE,
        CALLBACK_TURN
    } CallbackType;

    SourceCallback();
    virtual ~SourceCallback() {}

    virtual void update(Source *, float) = 0;
    virtual CallbackType type () { return CALLBACK_GENERIC; }

    inline bool finished() const { return finished_; }
    inline bool active() const { return active_; }
    inline void reset() { initialized_ = false; }

protected:
    bool active_;
    bool finished_;
    bool initialized_;
};

class ResetGeometry : public SourceCallback
{

public:
    ResetGeometry() : SourceCallback() {}
    void update(Source *s, float) override;
};

class SetAlpha : public SourceCallback
{
    float alpha_;
    glm::vec2 pos_;
    glm::vec2 step_;

public:
    SetAlpha(float alpha);
    void update(Source *s, float) override;
    CallbackType type () override { return CALLBACK_ALPHA; }
};

class SetDepth : public SourceCallback
{
    float duration_;
    float progress_;
    float start_;
    float target_;

public:
    SetDepth(float depth, float duration = 0.f);
    void update(Source *s, float dt) override;
    CallbackType type () override { return CALLBACK_DEPTH; }
};

class SetPlay : public SourceCallback
{
    bool play_;

public:
    SetPlay(bool on);
    void update(Source *s, float) override;
    CallbackType type () override { return CALLBACK_PLAY; }
};

class RePlay : public SourceCallback
{

public:
    RePlay();
    void update(Source *s, float) override;
    CallbackType type () override { return CALLBACK_REPLAY; }
};

class Grab : public SourceCallback
{
    glm::vec2 speed_;
    glm::vec2 start_;
    glm::vec2 target_;
    float duration_;
    float progress_;

public:
    Grab(float dx, float dy, float duration = 0.f);
    void update(Source *s, float) override;
    CallbackType type () override { return CALLBACK_GRAB; }
};

class Resize : public SourceCallback
{
    glm::vec2 speed_;
    glm::vec2 start_;
    glm::vec2 target_;
    float duration_;
    float progress_;

public:
    Resize(float dx, float dy, float duration = 0.f);
    void update(Source *s, float) override;
    CallbackType type () override { return CALLBACK_RESIZE; }
};

class Turn : public SourceCallback
{
    float speed_;
    float start_;
    float target_;
    float duration_;
    float progress_;

public:
    Turn(float da, float duration = 0.f);
    void update(Source *s, float) override;
    CallbackType type () override { return CALLBACK_TURN; }
};


#endif // SOURCECALLBACK_H
