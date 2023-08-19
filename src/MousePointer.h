#ifndef POINTER_H
#define POINTER_H

#include <string>
#include <vector>
#include <glm/glm.hpp>

#define ICON_POINTER_DEFAULT 7, 3
#define ICON_POINTER_OPTION 12, 9
#define ICON_POINTER_SPRING 13, 9
#define ICON_POINTER_LINEAR 14, 9
#define ICON_POINTER_WIGGLY 10, 3
#define ICON_POINTER_METRONOME 6, 13

class Pointer
{
public:
    typedef enum {
        POINTER_DEFAULT = 0,
        POINTER_LINEAR,
        POINTER_SPRING,
        POINTER_WIGGLY,
        POINTER_METRONOME,
        POINTER_INVALID
    } Mode;
    static std::vector< std::tuple<int, int, std::string> > Modes;

    Pointer() : strength_(0.5) {}
    virtual ~Pointer() {}
    inline glm::vec2 pos() { return pos_; }

    virtual void initiate(glm::vec2 pos) { pos_ = pos; }
    virtual void update(glm::vec2 pos, float) { pos_ = pos; }
    virtual void draw() {}

    inline void setStrength(float percent) { strength_ = glm::clamp(percent, 0.f, 1.f); }
    inline float strength() const { return strength_; }

protected:
    glm::vec2 pos_;
    float strength_;
};

class PointerLinear : public Pointer
{
public:
    PointerLinear() {}
    void update(glm::vec2 pos, float dt) override;
    void draw() override;
};

class PointerSpring : public Pointer
{
    glm::vec2 velocity_;
public:
    PointerSpring() {}
    void initiate(glm::vec2 pos) override;
    void update(glm::vec2 pos, float dt) override;
    void draw() override;
};

class PointerWiggly : public Pointer
{
public:
    PointerWiggly() {}
    void update(glm::vec2 pos, float) override;
    void draw() override;
};

class PointerMetronome : public Pointer
{
public:
    PointerMetronome() {}
    void update(glm::vec2 pos, float dt) override;
    void draw() override;
};


class MousePointer
{
    // Private Constructor
    MousePointer();
    MousePointer(MousePointer const& copy) = delete;
    MousePointer& operator=(MousePointer const& copy) = delete;

public:

    static MousePointer& manager ()
    {
        // The only instance
        static MousePointer _instance;
        return _instance;
    }

    inline Pointer *active() { return active_; }
    inline Pointer::Mode activeMode() { return mode_; }

    void setActiveMode(Pointer::Mode m);

private:

    Pointer::Mode mode_;
    Pointer *active_;
};

#endif // POINTER_H
