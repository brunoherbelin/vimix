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
    void render () override;
    bool playing () const override { return !paused_; }
    void play (bool) override;
    void replay () override;
    void reload () override;
    bool playable () const  override { return true; }
    Failure failed () const override;
    uint texture() const override;

    glm::vec3 resolution() const;

    Group *label;
    Character *label_0_, *label_1_;

protected:

    void init() override;

    // control
    bool paused_;
};


#endif // CANVASSOURCE_H
