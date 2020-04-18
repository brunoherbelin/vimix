#ifndef VIEW_H
#define VIEW_H

#include <glm/glm.hpp>

#include "Scene.h"
class FrameBuffer;

class View
{
public:
    View();

    virtual void update (float dt);
    virtual void draw () = 0;
    virtual void zoom (float) {}
    virtual void drag (glm::vec2, glm::vec2) {}

    Scene scene;

protected:
    Group backgound_;
    Group foreground_;
};


class MixingView : public View
{
public:
    MixingView();
    ~MixingView();

    void draw () override;
    void zoom (float factor);
    void drag (glm::vec2 from, glm::vec2 to);
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
