#ifndef SOURCECALLBACK_H
#define SOURCECALLBACK_H

#include <glm/glm.hpp>

#include "Scene.h"

class Visitor;
class Source;

class SourceCallback
{
public:

    typedef enum {
        CALLBACK_GENERIC = 0,
        CALLBACK_ALPHA = 1,
        CALLBACK_LOOM = 2,
        CALLBACK_GEOMETRY = 3,
        CALLBACK_GRAB = 4,
        CALLBACK_RESIZE = 5,
        CALLBACK_TURN = 6,
        CALLBACK_DEPTH = 7,
        CALLBACK_PLAY = 8,
        CALLBACK_REPLAY = 9,
        CALLBACK_RESETGEO = 10,
        CALLBACK_LOCK = 11
    } CallbackType;

    static SourceCallback *create(CallbackType type);
    static bool overlap(SourceCallback *a, SourceCallback *b);

    SourceCallback();
    virtual ~SourceCallback() {}

    virtual void update (Source *, float) = 0;
    virtual void multiply (float) {};
    virtual SourceCallback *clone () const = 0;
    virtual SourceCallback *reverse (Source *) const { return nullptr; }
    virtual CallbackType type () const { return CALLBACK_GENERIC; }
    virtual void accept (Visitor& v);

    inline bool finished () const { return finished_; }
    inline void reset () { initialized_ = false; }

protected:
    bool finished_;
    bool initialized_;
};

class SetAlpha : public SourceCallback
{
    float duration_;
    float progress_;
    float alpha_;
    glm::vec2 start_;
    glm::vec2 target_;
    bool bidirectional_;

public:
    SetAlpha (float alpha = 0.f, float ms = 0.f, bool revert = false);

    float value () const { return alpha_; }
    void  setValue (float a) { alpha_ = a; }
    float duration () const { return duration_; }
    void  setDuration (float ms) { duration_ = ms; }
    bool  bidirectional () const { return bidirectional_; }
    void  setBidirectional (bool on) { bidirectional_ = on; }

    void update (Source *s, float) override;
    void multiply (float factor) override;
    SourceCallback *clone () const override;
    SourceCallback *reverse(Source *s) const override;
    CallbackType type () const override { return CALLBACK_ALPHA; }
    void accept (Visitor& v) override;
};

class Loom : public SourceCallback
{
    float speed_;
    glm::vec2 pos_;
    glm::vec2 step_;
    float duration_;
    float progress_;

public:
    Loom (float speed = 0.f, float ms = 0.f);

    float value () const { return speed_; }
    void  setValue (float d) { speed_ = d; }
    float duration () const { return duration_; }
    void  setDuration (float ms) { duration_ = ms; }

    void update (Source *s, float) override;
    void multiply (float factor) override;
    SourceCallback *clone() const override;
    CallbackType type () const override { return CALLBACK_LOOM; }
    void accept (Visitor& v) override;
};

class Lock : public SourceCallback
{
    bool lock_;

public:
    Lock (bool on = true);

    bool value () const { return lock_;}
    void setValue (bool on) { lock_ = on;}

    void update (Source *s, float) override;
    SourceCallback *clone() const override;
    CallbackType type () const override { return CALLBACK_LOCK; }
};

class SetDepth : public SourceCallback
{
    float duration_;
    float progress_;
    float start_;
    float target_;
    bool bidirectional_;

public:
    SetDepth (float depth = 0.f, float ms = 0.f, bool revert = false);

    float value () const { return target_;}
    void  setValue (float d) { target_ = d; }
    float duration () const { return duration_;}
    void  setDuration (float ms) { duration_ = ms; }
    bool  bidirectional () const { return bidirectional_;}
    void  setBidirectional (bool on) { bidirectional_ = on; }

    void update (Source *s, float dt) override;
    void multiply (float factor) override;
    SourceCallback *clone () const override;
    SourceCallback *reverse(Source *s) const override;
    CallbackType type () const override { return CALLBACK_DEPTH; }
    void accept (Visitor& v) override;
};

class Play : public SourceCallback
{
    bool play_;
    bool bidirectional_;

public:
    Play (bool on = true, bool revert = false);

    bool value () const { return play_;}
    void setValue (bool on) { play_ = on; }
    bool  bidirectional () const { return bidirectional_;}
    void  setBidirectional (bool on) { bidirectional_ = on; }

    void update (Source *s, float) override;
    SourceCallback *clone() const override;
    SourceCallback *reverse(Source *s) const override;
    CallbackType type () const override { return CALLBACK_PLAY; }
    void accept (Visitor& v) override;
};

class RePlay : public SourceCallback
{
public:
    RePlay();

    void update(Source *s, float) override;
    SourceCallback *clone() const override;
    CallbackType type () const override { return CALLBACK_REPLAY; }
};

class ResetGeometry : public SourceCallback
{
public:
    ResetGeometry () : SourceCallback() {}
    void update (Source *s, float) override;
    SourceCallback *clone () const override;
    CallbackType type () const override { return CALLBACK_RESETGEO; }
};

class SetGeometry : public SourceCallback
{
    float duration_;
    float progress_;
    Group start_;
    Group target_;
    bool bidirectional_;

public:
    SetGeometry (const Group *g = nullptr, float ms = 0.f, bool revert = false);

    void  getTarget (Group *g) const;
    void  setTarget (const Group *g);
    float duration () const { return duration_;}
    void  setDuration (float ms) { duration_ = ms; }
    bool  bidirectional () const { return bidirectional_;}
    void  setBidirectional (bool on) { bidirectional_ = on; }

    void update (Source *s, float dt) override;
    void multiply (float factor) override;
    SourceCallback *clone () const override;
    SourceCallback *reverse(Source *s) const override;
    CallbackType type () const override { return CALLBACK_GEOMETRY; }
    void accept (Visitor& v) override;
};

class Grab : public SourceCallback
{
    glm::vec2 speed_;
    glm::vec2 start_;
    float duration_;
    float progress_;

public:
    Grab(float dx = 0.f, float dy = 0.f, float ms = 0.f);

    glm::vec2 value () const { return speed_; }
    void  setValue (glm::vec2 d) { speed_ = d; }
    float duration () const { return duration_; }
    void  setDuration (float ns) { duration_ = ns; }

    void update (Source *s, float) override;
    void multiply (float factor) override;
    SourceCallback *clone () const override;
    CallbackType type () const override { return CALLBACK_GRAB; }
    void accept (Visitor& v) override;
};

class Resize : public SourceCallback
{
    glm::vec2 speed_;
    glm::vec2 start_;
    float duration_;
    float progress_;

public:
    Resize(float dx = 0.f, float dy = 0.f, float ms = 0.f);

    glm::vec2 value () const { return speed_; }
    void  setValue (glm::vec2 d) { speed_ = d; }
    float duration () const { return duration_; }
    void  setDuration (float ms) { duration_ = ms; }

    void update (Source *s, float) override;
    void multiply (float factor) override;
    SourceCallback *clone () const override;
    CallbackType type () const override { return CALLBACK_RESIZE; }
    void accept (Visitor& v) override;
};

class Turn : public SourceCallback
{
    float speed_;
    float start_;
    float duration_;
    float progress_;

public:
    Turn(float speed = 0.f, float ms = 0.f);

    float value () const { return speed_; }
    void  setValue (float d) { speed_ = d; }
    float duration () const { return duration_; }
    void  setDuration (float ms) { duration_ = ms; }

    void update (Source *s, float) override;
    void multiply (float factor) override;
    SourceCallback *clone () const override;
    CallbackType type () const override { return CALLBACK_TURN; }
    void accept (Visitor& v) override;
};


#endif // SOURCECALLBACK_H
