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
    virtual void drag (glm::vec2, glm::vec2) {}
    virtual void grab (glm::vec2, glm::vec2, Source*) {}

    virtual void restoreSettings();
    virtual void saveSettings();

    Scene scene;

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
    void drag (glm::vec2 from, glm::vec2 to) override;
    void grab (glm::vec2 from, glm::vec2 to, Source *s) override;

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
    void drag (glm::vec2 from, glm::vec2 to) override;
    void grab (glm::vec2 from, glm::vec2 to, Source *s) override;

private:

};


#endif // VIEW_H
