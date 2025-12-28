#ifndef CANVASSOURCE_H
#define CANVASSOURCE_H

#include "Source.h"

class RenderView;

class CanvasSource : public Source
{
public:
    CanvasSource(uint64_t id = 0);
    ~CanvasSource();

    // implementation of source API
    void update (float dt) override;
    bool playing () const override { return !paused_; }
    void play (bool) override;
    void replay () override;
    void reload () override;
    bool playable () const  override { return true; }
    Failure failed () const override;
    uint texture() const override;
    void accept (Visitor& v) override;

    glm::vec3 resolution() const;

protected:

    void init() override;

    FrameBuffer *rendered_output_;
    Surface *rendered_surface_;
    Scene canvas_rendering_scene_;

    // control
    bool paused_;
    bool reset_;
};


#endif // CANVASSOURCE_H
