#ifndef VIEW_H
#define VIEW_H

#include <glm/glm.hpp>

#include "Scene.h"
#include "FrameBuffer.h"
class Source;

class View
{
public:

    typedef enum {RENDERING = 0, MIXING=1, GEOMETRY=2, LAYER=3, INVALID=4 } Mode;

    View(Mode m);
    virtual ~View() {}

    inline Mode mode() const { return mode_; }

    virtual void update (float dt);
    virtual void draw ();

    virtual void zoom (float) {}
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

    virtual std::pair<Node *, glm::vec2> pick(glm::vec3 point);
    virtual Cursor drag (glm::vec2, glm::vec2);
    virtual Cursor grab (glm::vec2, glm::vec2, Source*, std::pair<Node *, glm::vec2>) {
        return Cursor();
    }
    virtual Cursor over (glm::vec2, Source*, std::pair<Node *, glm::vec2>) {
        return Cursor();
    }


    virtual void restoreSettings();
    virtual void saveSettings();

    Scene scene;

    // hack to avoid reordering scene of view if not necessary
    static bool need_deep_update_;

protected:
    Mode mode_;
};


class MixingView : public View
{
public:
    MixingView();

    void zoom (float factor) override;
    void centerSource(Source *) override;

    Cursor grab (glm::vec2 from, glm::vec2 to, Source *s, std::pair<Node *, glm::vec2>) override;

private:
    uint textureMixingQuadratic();
};

class RenderView : public View
{
    FrameBuffer *frame_buffer_;

public:
    RenderView ();
    ~RenderView ();

    void draw () override;

    void setResolution (glm::vec3 resolution = glm::vec3(0.f));
    glm::vec3 resolution() const { return frame_buffer_->resolution(); }

    inline FrameBuffer *frame () const { return frame_buffer_; }
};

class GeometryView : public View
{
public:
    GeometryView();

    void draw () override;
    void update (float dt) override;
    void zoom (float factor) override;
    std::pair<Node *, glm::vec2> pick(glm::vec3 point) override;
    Cursor grab (glm::vec2 from, glm::vec2 to, Source *s, std::pair<Node *, glm::vec2> pick) override;
    Cursor over (glm::vec2, Source*, std::pair<Node *, glm::vec2>) override;

};

class LayerView : public View
{
public:
    LayerView();

    void update (float dt) override;
    void zoom (float factor) override;
    Cursor grab (glm::vec2 from, glm::vec2 to, Source *s, std::pair<Node *, glm::vec2> pick) override;

    float setDepth (Source *, float d = -1.f);

private:
    float aspect_ratio;
};


#endif // VIEW_H
