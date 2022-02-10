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
        CALLBACK_ALPHA = 1,
        CALLBACK_LOOM = 2,
        CALLBACK_GRAB = 3,
        CALLBACK_RESIZE = 4,
        CALLBACK_TURN = 5,
        CALLBACK_DEPTH = 6,
        CALLBACK_PLAY = 7,
        CALLBACK_REPLAY = 8,
        CALLBACK_RESETGEO = 9,
        CALLBACK_LOCK = 10
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

class ResetGeometry : public SourceCallback
{
public:
    ResetGeometry () : SourceCallback() {}
    void update (Source *s, float) override;
    SourceCallback *clone () const override;
    CallbackType type () const override { return CALLBACK_RESETGEO; }
};

class SetAlpha : public SourceCallback
{
    float duration_;
    float progress_;
    float alpha_;
    glm::vec2 start_;
    glm::vec2 target_;

public:
    SetAlpha ();
    SetAlpha (float alpha, float duration = 0.f);

    float value () const { return alpha_;}
    void  setValue (float a) { alpha_ = a; }
    float duration () const { return duration_;}
    void  setDuration (float d) { duration_ = d; }

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
    Loom ();
    Loom (float d, float duration = 0.f);

    float value () const { return speed_;}
    void  setValue (float d) { speed_ = d; }
    float duration () const { return duration_;}
    void  setDuration (float d) { duration_ = d; }

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
    Lock ();
    Lock (bool on);

    bool value () const { return lock_;}
    void setValue (bool l) { lock_ = l;}

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

public:
    SetDepth ();
    SetDepth (float depth, float duration = 0.f);

    float value () const { return target_;}
    void  setValue (float d) { target_ = d; }
    float duration () const { return duration_;}
    void  setDuration (float d) { duration_ = d; }

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

public:
    Play ();
    Play (bool on);

    bool value () const { return play_;}
    void setValue (bool on) { play_ = on; }

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

class Grab : public SourceCallback
{
    glm::vec2 speed_;
    glm::vec2 start_;
    float duration_;
    float progress_;

public:
    Grab();
    Grab(float dx, float dy, float duration = 0.f);

    glm::vec2 value () const { return speed_;}
    void  setValue (glm::vec2 d) { speed_ = d; }
    float duration () const { return duration_;}
    void  setDuration (float d) { duration_ = d; }

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
    Resize();
    Resize(float dx, float dy, float duration = 0.f);

    glm::vec2 value () const { return speed_;}
    void  setValue (glm::vec2 d) { speed_ = d; }
    float duration () const { return duration_;}
    void  setDuration (float d) { duration_ = d; }

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
    Turn();
    Turn(float da, float duration = 0.f);

    float value () const { return speed_;}
    void  setValue (float d) { speed_ = d; }
    float duration () const { return duration_;}
    void  setDuration (float d) { duration_ = d; }

    void update (Source *s, float) override;
    void multiply (float factor) override;
    SourceCallback *clone () const override;
    CallbackType type () const override { return CALLBACK_TURN; }
    void accept (Visitor& v) override;
};


#endif // SOURCECALLBACK_H
