#ifndef POINTER_H
#define POINTER_H

#include <string>
#include <map>
#include <vector>
#include <glm/glm.hpp>

#define ICON_POINTER_DEFAULT 7, 3
#define ICON_POINTER_OPTION 12, 9
#define ICON_POINTER_SPRING 13, 9
#define ICON_POINTER_LINEAR 14, 9
#define ICON_POINTER_GRID   15, 9
#define ICON_POINTER_WIGGLY 10, 3
#define ICON_POINTER_METRONOME 6, 13

///
/// \brief The Pointer class takes a position at each update and
/// computes a new position that is filtered.
///
/// A position can be given at initiation and a termination can be used to
/// finish up. Draw should perform a visual representation of the cursor
///
/// By default, a Pointer does not alter the position
/// The Pointer class has to be inerited to define other behaviors
///
class Pointer
{
public:
    typedef enum {
        POINTER_DEFAULT = 0,
        POINTER_GRID,
        POINTER_LINEAR,
        POINTER_SPRING,
        POINTER_WIGGLY,
        POINTER_METRONOME,
        POINTER_INVALID
    } Mode;
    static std::vector< std::tuple<int, int, std::string, std::string> > Modes;

    Pointer() : strength_(0.5) {}
    virtual ~Pointer() {}
    inline glm::vec2 target() const { return target_; }

    virtual void initiate(const glm::vec2 &pos) { current_ = target_ = pos; }
    virtual void update(const glm::vec2 &pos, float) { current_ = target_ = pos; }
    virtual void terminate() {}
    virtual void draw() {}

    inline void  setStrength(float percent)   { strength_ = glm::clamp(percent, 0.f, 1.f); }
    inline void  incrementStrength(float inc) { setStrength( strength_ + inc); }
    inline float strength() const            { return strength_; }

protected:
    glm::vec2 current_;
    glm::vec2 target_;
    float strength_;
};

///
/// \brief The PointerGrid activates the view grid
///
class PointerGrid : public Pointer
{
public:
    PointerGrid() {}
    void initiate(const glm::vec2&) override;
    void update(const glm::vec2 &pos, float) override;
    void terminate() override;
};

///
/// \brief The PointerLinear moves the pointer on a line
/// Strength modulates the value of the constant speed
///
class PointerLinear : public Pointer
{
public:
    PointerLinear() {}
    void update(const glm::vec2 &pos, float dt) override;
    void draw() override;
};

///
/// \brief The PointerSpring moves the pointer as a spring-mass system
/// Strength modulates the mass of the pointer
///
class PointerSpring : public Pointer
{
    glm::vec2 velocity_;
public:
    PointerSpring() {}
    void initiate(const glm::vec2 &pos) override;
    void update(const glm::vec2 &pos, float dt) override;
    void draw() override;
};

///
/// \brief The PointerWiggly moves randomly at high frequency
/// Strength modulates the radius of the movement
///
class PointerWiggly : public Pointer
{
public:
    PointerWiggly() {}
    void update(const glm::vec2 &pos, float) override;
    void draw() override;
};

///
/// \brief The PointerMetronome waits for the metronome to follow
/// TODO : modulate number of beat sync ?
///
class PointerMetronome : public Pointer
{
    glm::vec2 beat_pos_;
public:
    PointerMetronome() {}
    void initiate(const glm::vec2 &pos) override;
    void update(const glm::vec2 &pos, float dt) override;
    void draw() override;
};


class MousePointer
{
    // Private Constructor
    MousePointer();
    MousePointer(MousePointer const& copy) = delete;
    MousePointer& operator=(MousePointer const& copy) = delete;
    ~MousePointer();

public:
    static MousePointer& manager ()
    {
        // The only instance
        static MousePointer _instance;
        return _instance;
    }

    inline Pointer *active() { return pointer_[mode_]; }

    inline Pointer::Mode activeMode() { return mode_; }
    inline void setActiveMode(Pointer::Mode m) { mode_ = m; }

private:
    Pointer::Mode mode_;
    std::map< Pointer::Mode, Pointer* > pointer_;
};

#endif // POINTER_H
