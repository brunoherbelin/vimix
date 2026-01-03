#ifndef CANVASSOURCE_H
#define CANVASSOURCE_H

#include "Source.h"

class RenderView;

class CanvasSurface : public Source
{
public:
    CanvasSurface(uint64_t id = 0);
    ~CanvasSurface();

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


// class CanvasRenderSource : public Source
// {
// public:
//     CanvasRenderSource(uint64_t id = 0);
//     ~CanvasRenderSource();

//     // implementation of source API
//     void update (float dt) override;
//     bool playing () const override { return !paused_; }
//     void play (bool) override;
//     void replay () override;
//     void reload () override;
//     bool playable () const  override { return true; }
//     guint64 playtime () const override { return 0; }
//     Failure failed () const override;
//     uint texture() const override;
//     void accept (Visitor& v) override;

//     inline CanvasSource *canvas () const { return canvas_; }
//     inline void setCanvas (CanvasSource *se) { canvas_ = se; }

//     glm::vec3 resolution() const;

//     glm::ivec2 icon() const override;
//     std::string info() const override;

// protected:

//     void init() override;
//     CanvasSource *canvas_;

//     FrameBuffer *rendered_output_;

//     // control
//     bool paused_;
//     bool reset_;
// };

#endif // CANVASSOURCE_H
