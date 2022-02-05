#ifndef SOURCECALLBACK_H
#define SOURCECALLBACK_H

#include <glm/glm.hpp>

class Visitor;
class Source;

class SourceCallback
{
public:

    typedef enum {
        CALLBACK_GENERIC = 0,
        CALLBACK_ALPHA,
        CALLBACK_LOOM,
        CALLBACK_GRAB,
        CALLBACK_RESIZE,
        CALLBACK_TURN,
        CALLBACK_DEPTH,
        CALLBACK_PLAY,
        CALLBACK_REPLAY,
        CALLBACK_RESETGEO,
        CALLBACK_LOCK
    } CallbackType;

    static SourceCallback *create(CallbackType type);

    SourceCallback();
    virtual ~SourceCallback() {}

    virtual void update (Source *, float) = 0;
    virtual SourceCallback *clone () const = 0;
    virtual SourceCallback *reverse (Source *) const { return nullptr; }
    virtual CallbackType type () { return CALLBACK_GENERIC; }
    virtual void accept (Visitor& v);

    inline bool finished () const { return finished_; }
    inline bool active () const { return active_; }
    inline void reset () { initialized_ = false; }

protected:
    bool active_;
    bool finished_;
    bool initialized_;
};

class ResetGeometry : public SourceCallback
{
public:
    ResetGeometry () : SourceCallback() {}
    void update (Source *s, float) override;
    SourceCallback *clone () const override;
    CallbackType type () override { return CALLBACK_RESETGEO; }
};

class SetAlpha : public SourceCallback
{
    float alpha_;
    glm::vec2 pos_;
    glm::vec2 step_;

public:
    SetAlpha ();
    SetAlpha (float alpha);

    float value () const { return alpha_;}
    void  setValue (float a) { alpha_ = a; }

    void update (Source *s, float) override;
    SourceCallback *clone () const override;
    SourceCallback *reverse(Source *s) const override;
    CallbackType type () override { return CALLBACK_ALPHA; }
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
    Loom ();
    Loom (float d, float duration = 0.f);

    float value () const { return speed_;}
    void  setValue (float d) { speed_ = d; }
    float duration () const { return duration_;}
    void  setDuration (float d) { duration_ = d; }

    void update (Source *s, float) override;
    SourceCallback *clone() const override;
    CallbackType type () override { return CALLBACK_LOOM; }
    void accept (Visitor& v) override;
};

class Lock : public SourceCallback
{
    bool lock_;

public:
    Lock ();
    Lock (bool on);

    bool value () const { return lock_;}
    void setValue (bool l) { lock_ = l;}

    void update (Source *s, float) override;
    SourceCallback *clone() const override;
    CallbackType type () override { return CALLBACK_LOCK; }
};

class SetDepth : public SourceCallback
{
    float duration_;
    float progress_;
    float start_;
    float target_;

public:
    SetDepth ();
    SetDepth (float depth, float duration = 0.f);

    float value () const { return target_;}
    void  setValue (float d) { target_ = d; }
    float duration () const { return duration_;}
    void  setDuration (float d) { duration_ = d; }

    void update (Source *s, float dt) override;
    SourceCallback *clone () const override;
    SourceCallback *reverse(Source *s) const override;
    CallbackType type () override { return CALLBACK_DEPTH; }
    void accept (Visitor& v) override;
};

class Play : public SourceCallback
{
    bool play_;

public:
    Play ();
    Play (bool on);

    bool value () const { return play_;}
    void setValue (bool on) { play_ = on; }

    void update (Source *s, float) override;
    SourceCallback *clone() const override;
    SourceCallback *reverse(Source *s) const override;
    CallbackType type () override { return CALLBACK_PLAY; }
};

class RePlay : public SourceCallback
{
public:
    RePlay();

    void update(Source *s, float) override;
    SourceCallback *clone() const override;
    CallbackType type () override { return CALLBACK_REPLAY; }
};

class Grab : public SourceCallback
{
    glm::vec2 speed_;
    glm::vec2 start_;
    float duration_;
    float progress_;

public:
    Grab();
    Grab(float dx, float dy, float duration = 0.f);

    glm::vec2 value () { return speed_;}
    void  setValue (glm::vec2 d) { speed_ = d; }
    float duration () const { return duration_;}
    void  setDuration (float d) { duration_ = d; }

    void update (Source *s, float) override;
    SourceCallback *clone () const override;
    CallbackType type () override { return CALLBACK_GRAB; }
    void accept (Visitor& v) override;
};

class Resize : public SourceCallback
{
    glm::vec2 speed_;
    glm::vec2 start_;
    float duration_;
    float progress_;

public:
    Resize();
    Resize(float dx, float dy, float duration = 0.f);

    glm::vec2 value () { return speed_;}
    void  setValue (glm::vec2 d) { speed_ = d; }
    float duration () const { return duration_;}
    void  setDuration (float d) { duration_ = d; }

    void update (Source *s, float) override;
    SourceCallback *clone () const override;
    CallbackType type () override { return CALLBACK_RESIZE; }
    void accept (Visitor& v) override;
};

class Turn : public SourceCallback
{
    float speed_;
    float start_;
    float duration_;
    float progress_;

public:
    Turn();
    Turn(float da, float duration = 0.f);

    float value () { return speed_;}
    void  setValue (float d) { speed_ = d; }
    float duration () const { return duration_;}
    void  setDuration (float d) { duration_ = d; }

    void update (Source *s, float) override;
    SourceCallback *clone () const override;
    CallbackType type () override { return CALLBACK_TURN; }
    void accept (Visitor& v) override;
};


#endif // SOURCECALLBACK_H
