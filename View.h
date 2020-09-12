#ifndef VIEW_H
#define VIEW_H

#include <glm/glm.hpp>

#include "Scene.h"
#include "FrameBuffer.h"
class Source;
class SessionSource;
class Surface;

class View
{
public:

    typedef enum {RENDERING = 0, MIXING=1, GEOMETRY=2, LAYER=3,  TRANSITION=4, INVALID=5 } Mode;

    View(Mode m);
    virtual ~View() {}

    inline Mode mode() const { return mode_; }

    virtual void update (float dt);
    virtual void draw ();

    virtual void zoom (float) {}
    virtual void recenter();
    virtual void centerSource(Source *) {}

    typedef enum {
        Cursor_Arrow = 0,
        Cursor_TextInput,
        Cursor_ResizeAll,
        Cursor_ResizeNS,
        Cursor_ResizeEW,
        Cursor_ResizeNESW,
        Cursor_ResizeNWSE,
        Cursor_Hand,
        Cursor_NotAllowed
    } CursorType;

    typedef struct Cursor {
        CursorType type;
        std::string info;
        Cursor() { type = Cursor_Arrow; info = "";}
        Cursor(CursorType t, std::string i = "") { type = t; info = i;}
    } Cursor;

    // picking of nodes in a view provided a point coordinates in screen coordinates
    virtual std::pair<Node *, glm::vec2> pick(glm::vec2);

    // select sources provided a start and end selection points in screen coordinates
    virtual void select(glm::vec2, glm::vec2);
    virtual void selectAll();

    // drag the view provided a start and an end point in screen coordinates
    virtual Cursor drag (glm::vec2, glm::vec2);

    // grab a source provided a start and an end point in screen coordinates and the picking point
    virtual void initiate();
    virtual void terminate() {}
    virtual Cursor grab (Source*, glm::vec2, glm::vec2, std::pair<Node *, glm::vec2>) {
        return Cursor();
    }

//    // test mouse over provided a point in screen coordinates and the picking point
//    virtual Cursor over (Source*, glm::vec2, std::pair<Node *, glm::vec2>) {
//        return Cursor();
//    }

    // accessible scene
    Scene scene;

    // reordering scene when necessary
    static bool need_deep_update_;

protected:

    virtual void restoreSettings();
    virtual void saveSettings();

    Mode mode_;
};


class MixingView : public View
{
public:
    MixingView();

    void draw () override;
    void update (float dt) override;
    void zoom (float factor) override;
    void centerSource(Source *) override;
    void selectAll() override;

    std::pair<Node *, glm::vec2> pick(glm::vec2) override;
    Cursor grab (Source *s, glm::vec2 from, glm::vec2 to, std::pair<Node *, glm::vec2>) override;
    Cursor drag (glm::vec2, glm::vec2) override;

    void setAlpha (Source *s);
    inline float limboScale() { return limbo_scale_; }

private:
    uint textureMixingQuadratic();
    float limbo_scale_;

    Group *slider_root_;
    class Disk *slider_;
    class Disk *button_white_;
    class Disk *button_black_;
    class Mesh *mixingCircle_;
};

class RenderView : public View
{
    FrameBuffer *frame_buffer_;
    Surface *fading_overlay_;

public:
    RenderView ();
    ~RenderView ();

    void draw () override;

    void setResolution (glm::vec3 resolution = glm::vec3(0.f));
    glm::vec3 resolution() const { return frame_buffer_->resolution(); }

    void setFading(float f = 0.f);
    float fading() const;

    inline FrameBuffer *frame () const { return frame_buffer_; }
};

class GeometryView : public View
{
public:
    GeometryView();

    void draw () override;
    void update (float dt) override;
    void zoom (float factor) override;

    std::pair<Node *, glm::vec2> pick(glm::vec2 P) override;
    Cursor grab (Source *s, glm::vec2 from, glm::vec2 to, std::pair<Node *, glm::vec2> pick) override;
//    Cursor over (Source *s, glm::vec2 pos, std::pair<Node *, glm::vec2> pick) override;
    Cursor drag (glm::vec2, glm::vec2) override;
    void terminate();

private:
    Node *overlay_position_;
    Node *overlay_position_cross_;
    Node *overlay_rotation_;
    Node *overlay_rotation_fix_;
    Node *overlay_rotation_clock_;
    Node *overlay_scaling_;
    Node *overlay_scaling_cross_;
    Node *overlay_scaling_grid_;
};

class LayerView : public View
{
public:
    LayerView();

    void update (float dt) override;
    void zoom (float factor) override;

    Cursor grab (Source *s, glm::vec2 from, glm::vec2 to, std::pair<Node *, glm::vec2> pick) override;
    Cursor drag (glm::vec2, glm::vec2) override;

    float setDepth (Source *, float d = -1.f);

private:
    float aspect_ratio;
};

class TransitionView : public View
{
public:
    TransitionView();

    void draw () override;
    void update (float dt) override;
    void zoom (float factor) override;
    void selectAll() override;

    std::pair<Node *, glm::vec2> pick(glm::vec2 P) override;
    Cursor grab (Source *s, glm::vec2 from, glm::vec2 to, std::pair<Node *, glm::vec2> pick) override;
    Cursor drag (glm::vec2, glm::vec2) override;

    void attach(SessionSource *ts);
    class Session *detach();
    void play(bool open);

private:
    class Surface *output_surface_;
    class Mesh *mark_100ms_, *mark_1s_;
    Switch *gradient_;
    SessionSource *transition_source_;
};



#endif // VIEW_H
