#ifndef SOURCECALLBACK_H
#define SOURCECALLBACK_H

#include <cmath>
#include <string>
#include <glm/glm.hpp>

#include "Scene.h"

class Visitor;
class Source;
class ImageProcessingShader;

/**
 * @brief The SourceCallback class defines operations on Sources that
 * are applied at each update of Source. A SourceCallback is added to
 * a source with; source->call( new SourceCallback );
 *
 * A source contains a list of SourceCallbacks. At each update(dt)
 * a source calls the update on all its SourceCallbacks.
 * The SourceCallback is created as PENDING, and becomes ACTIVE after
 * the first update call.
 * The SourceCallback is removed from the list when FINISHED.
 *
 */
class SourceCallback
{
public:

    typedef enum {
        CALLBACK_GENERIC = 0,
        CALLBACK_ALPHA,
        CALLBACK_LOOM,
        CALLBACK_GEOMETRY,
        CALLBACK_GRAB,
        CALLBACK_RESIZE,
        CALLBACK_TURN,
        CALLBACK_DEPTH,
        CALLBACK_PLAY,
        CALLBACK_PLAYSPEED,
        CALLBACK_PLAYFFWD,
        CALLBACK_SEEK,
        CALLBACK_REPLAY,
        CALLBACK_RESETGEO,
        CALLBACK_LOCK,
        CALLBACK_GAMMA,
        CALLBACK_BRIGHTNESS,
        CALLBACK_CONTRAST,
        CALLBACK_SATURATION,
        CALLBACK_HUE,
        CALLBACK_THRESHOLD,
        CALLBACK_INVERT,
        CALLBACK_POSTERIZE,
        CALLBACK_FILTER
    } CallbackType;

    static SourceCallback *create(CallbackType type);

    SourceCallback();
    virtual ~SourceCallback() {}

    virtual void update (Source *, float);
    virtual void multiply (float) {}
    virtual SourceCallback *clone () const = 0;
    virtual SourceCallback *reverse (Source *) const { return nullptr; }
    virtual CallbackType type () const { return CALLBACK_GENERIC; }
    virtual void accept (Visitor& v);

    inline void reset () { status_ = PENDING; }
    inline void finish () { status_ = FINISHED; }
    inline bool finished () const { return status_ > ACTIVE; }
    inline void delay (float milisec) { delay_ = milisec;}

protected:

    typedef enum {
        PENDING = 0,
        READY,
        ACTIVE,
        FINISHED
    } state;

    state status_;
    float delay_;
    float elapsed_;
};

/**
 * @brief The ValueSourceCallback class is a generic type of
 * callback which operates on a single float value of a source.
 *
 * ValueSourceCallback are practical for creating SourceCallbacks
 * that change a single attribute of a source, that can be read
 * and write on the source object.
 */
class ValueSourceCallback : public SourceCallback
{
protected:
    float duration_;
    float start_;
    float target_;
    bool  bidirectional_;

    virtual float readValue(Source *s) const = 0;
    virtual void  writeValue(Source *s, float val) = 0;

public:
    ValueSourceCallback (float v = 0.f, float ms = 0.f, bool revert = false);

    float value () const { return target_;}
    void  setValue (float v) { target_ = v; }
    float duration () const { return duration_;}
    void  setDuration (float ms) { duration_ = ms; }
    bool  bidirectional () const { return bidirectional_;}
    void  setBidirectional (bool on) { bidirectional_ = on; }

    void update (Source *s, float) override;
    void multiply (float factor) override;
    SourceCallback *clone () const override;
    SourceCallback *reverse(Source *s) const override;
    void accept (Visitor& v) override;
};


class SetAlpha : public SourceCallback
{
    float duration_;
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
    glm::vec2 step_;
    float duration_;

public:
    Loom (float speed = 0.f, float ms = FLT_MAX);

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

    void update (Source *s, float dt) override;
    SourceCallback *clone() const override;
    SourceCallback *reverse(Source *s) const override;
    CallbackType type () const override { return CALLBACK_PLAY; }
    void accept (Visitor& v) override;
};

class RePlay : public SourceCallback
{
public:
    RePlay();

    void update(Source *s, float dt) override;
    SourceCallback *clone() const override;
    CallbackType type () const override { return CALLBACK_REPLAY; }
};

class PlaySpeed : public ValueSourceCallback
{
    float readValue(Source *s) const override;
    void  writeValue(Source *s, float val) override;
public:
    PlaySpeed (float v = 1.f, float ms = 0.f, bool revert = false);
    CallbackType type () const override { return CALLBACK_PLAYSPEED; }
};

class PlayFastForward : public SourceCallback
{
    class MediaPlayer *media_;
    uint step_;
    float duration_;
    double playspeed_;
public:
    PlayFastForward (uint seekstep = 30, float ms = FLT_MAX);

    uint  value () const { return step_; }
    void  setValue (uint s) { step_ = s; }
    float duration () const { return duration_; }
    void  setDuration (float ms) { duration_ = ms; }

    void update (Source *s, float) override;
    void multiply (float factor) override;
    SourceCallback *clone() const override;
    CallbackType type () const override { return CALLBACK_PLAYFFWD; }
    void accept (Visitor& v) override;
};

class Seek : public SourceCallback
{
    float target_percent_;
    glm::uint64 target_time_;
    bool bidirectional_;

public:
    Seek (glm::uint64 target = 0, bool revert = false);
    Seek (float percent);
    Seek (int hh, int mm, int ss, int ms);

    glm::uint64 value () const { return target_time_; }
    void  setValue (glm::uint64 t) { target_time_ = t; }
    bool  bidirectional () const { return bidirectional_; }
    void  setBidirectional (bool on) { bidirectional_ = on; }

    void update (Source *s, float) override;
    SourceCallback *clone() const override;
    SourceCallback *reverse(Source *s) const override;
    CallbackType type () const override { return CALLBACK_SEEK; }
    void accept (Visitor& v) override;
};

class ResetGeometry : public SourceCallback
{
public:
    ResetGeometry () : SourceCallback() {}
    void update (Source *s, float dt) override;
    SourceCallback *clone () const override;
    CallbackType type () const override { return CALLBACK_RESETGEO; }
};

class SetGeometry : public SourceCallback
{
    float duration_;
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
    float duration_;

public:
    Grab(float dx = 0.f, float dy = 0.f, float ms = FLT_MAX);

    glm::vec2 value () const { return speed_; }
    void  setValue (glm::vec2 d) { speed_ = d; }
    float duration () const { return duration_; }
    void  setDuration (float ms) { duration_ = ms; }

    void update (Source *s, float) override;
    void multiply (float factor) override;
    SourceCallback *clone () const override;
    CallbackType type () const override { return CALLBACK_GRAB; }
    void accept (Visitor& v) override;
};

class Resize : public SourceCallback
{
    glm::vec2 speed_;
    float duration_;

public:
    Resize(float dx = 0.f, float dy = 0.f, float ms = FLT_MAX);

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
    float duration_;

public:
    Turn(float speed = 0.f, float ms = FLT_MAX);

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

class SetBrightness : public ValueSourceCallback
{
    float readValue(Source *s) const override;
    void  writeValue(Source *s, float val) override;
public:
    SetBrightness (float v = 0.f, float ms = 0.f, bool revert = false);
    CallbackType type () const override { return CALLBACK_BRIGHTNESS; }
};

class SetContrast : public ValueSourceCallback
{
    float readValue(Source *s) const override;
    void  writeValue(Source *s, float val) override;
public:
    SetContrast (float v = 0.f, float ms = 0.f, bool revert = false);
    CallbackType type () const override { return CALLBACK_CONTRAST; }
};

class SetSaturation : public ValueSourceCallback
{
    float readValue(Source *s) const override;
    void  writeValue(Source *s, float val) override;
public:
    SetSaturation (float v = 0.f, float ms = 0.f, bool revert = false);
    CallbackType type () const override { return CALLBACK_SATURATION; }
};

class SetHue : public ValueSourceCallback
{
    float readValue(Source *s) const override;
    void  writeValue(Source *s, float val) override;
public:
    SetHue (float v = 0.f, float ms = 0.f, bool revert = false);
    CallbackType type () const override { return CALLBACK_HUE; }
};

class SetThreshold : public ValueSourceCallback
{
    float readValue(Source *s) const override;
    void  writeValue(Source *s, float val) override;
public:
    SetThreshold (float v = 0.f, float ms = 0.f, bool revert = false);
    CallbackType type () const override { return CALLBACK_THRESHOLD; }
};

class SetInvert : public ValueSourceCallback
{
    float readValue(Source *s) const override;
    void  writeValue(Source *s, float val) override;
public:
    SetInvert (float v = 0.f, float ms = 0.f, bool revert = false);
    CallbackType type () const override { return CALLBACK_INVERT; }
};

class SetPosterize : public ValueSourceCallback
{
    float readValue(Source *s) const override;
    void  writeValue(Source *s, float val) override;
public:
    SetPosterize (float v = 0.f, float ms = 0.f, bool revert = false);
    CallbackType type () const override { return CALLBACK_POSTERIZE; }
};

class SetGamma : public SourceCallback
{
    float duration_;
    glm::vec4 start_;
    glm::vec4 target_;
    bool bidirectional_;

public:
    SetGamma (glm::vec4 g = glm::vec4(), float ms = 0.f, bool revert = false);

    glm::vec4 value () const { return target_; }
    void  setValue (glm::vec4 g) { target_ = g; }
    float duration () const { return duration_; }
    void  setDuration (float ms) { duration_ = ms; }
    bool  bidirectional () const { return bidirectional_; }
    void  setBidirectional (bool on) { bidirectional_ = on; }

    void update (Source *s, float) override;
    void multiply (float factor) override;
    SourceCallback *clone () const override;
    SourceCallback *reverse(Source *s) const override;
    CallbackType type () const override { return CALLBACK_GAMMA; }
    void accept (Visitor& v) override;
};

class SetFilter : public SourceCallback
{
    std::string target_filter_;
    std::string target_method_;
    std::string target_parameter_;
    float start_value_;
    float target_value_;
    float duration_;
    class ImageFilter *imagefilter;
    class DelayFilter *delayfilter;

public:
    SetFilter(const std::string &filter = std::string(),
              const std::string &method = std::string(),
              float value = NAN,
              float ms = 0.f);

    std::string filter() const { return target_filter_; }
    void setFilter(const std::string &f) { target_filter_ = f; }
    std::string method() const { return target_method_; }
    void setMethod(const std::string &m) { target_method_ = m; }
    float value () const { return target_value_; }
    void  setValue (float g) { target_value_ = g; }
    float duration () const { return duration_; }
    void  setDuration (float ms) { duration_ = ms; }

    void update (Source *s, float) override;
    void multiply (float factor) override;
    SourceCallback *clone () const override;
    CallbackType type () const override { return CALLBACK_FILTER; }
    void accept (Visitor& v) override;
};

#endif // SOURCECALLBACK_H
