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
    inline Mode mode() const { return mode_; }

    virtual void update (float dt);
    virtual void draw () = 0;
    virtual void zoom (float) {}
    virtual void drag (glm::vec2, glm::vec2);
    virtual void grab (glm::vec2, glm::vec2, Source*, std::pair<Node *, glm::vec2>) {}

    virtual void restoreSettings();
    virtual void saveSettings();

    Scene scene;

    // hack to avoid reordering scene of view if not necessary
    static bool need_reordering_;

protected:
    Mode mode_;
};


class MixingView : public View
{
public:
    MixingView();
    ~MixingView();

    void draw () override;
    void zoom (float factor) override;
    void grab (glm::vec2 from, glm::vec2 to, Source *s, std::pair<Node *, glm::vec2>) override;

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

    inline FrameBuffer *frameBuffer () const { return frame_buffer_; }
};

class GeometryView : public View
{
public:
    GeometryView();
    ~GeometryView();

    void draw () override;
    void zoom (float factor) override;
    void grab (glm::vec2 from, glm::vec2 to, Source *s, std::pair<Node *, glm::vec2> pick) override;

private:

};

class LayerView : public View
{
public:
    LayerView();
    ~LayerView();

    void draw () override;
    void zoom (float factor) override;
    void grab (glm::vec2 from, glm::vec2 to, Source *s, std::pair<Node *, glm::vec2> pick) override;

private:
    float aspect_ratio;
};


#endif // VIEW_H
