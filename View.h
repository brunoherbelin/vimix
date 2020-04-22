#ifndef VIEW_H
#define VIEW_H

#include <glm/glm.hpp>

#include "Scene.h"
class FrameBuffer;
class Source;

class View
{
public:
    View();

    typedef enum {RENDERING = 0, MIXING=1, GEOMETRY=2, LAYER=3, INVALID=4 } Mode;

    virtual void update (float dt);
    virtual void draw () = 0;
    virtual void zoom (float) {}
    virtual void drag (glm::vec2, glm::vec2) {}
    virtual void grab (glm::vec2, glm::vec2, Source*) {}

    Scene scene;

protected:
    Group backgound_;
};


class MixingView : public View
{
public:
    MixingView();
    ~MixingView();

    void draw () override;
    void zoom (float factor);
    void drag (glm::vec2 from, glm::vec2 to);
    void grab (glm::vec2 from, glm::vec2 to, Source *s);

};

class RenderView : public View
{
    FrameBuffer *frame_buffer_;

public:
    RenderView ();
    ~RenderView ();

    void draw () override;

    void setResolution (uint width, uint height);
    inline FrameBuffer *frameBuffer () const { return frame_buffer_; }

};
#endif // VIEW_H
