#ifndef CANVASSOURCE_H
#define CANVASSOURCE_H

#include "Source.h"
#include <cstddef>

class RenderView;

// A canvas surface is a source representing a canvas framebuffer
// It is used internally in Geometry View to display the canvas content
// and in CanvasSource to render it in the display output
// CanvasSurface is managed by Canvas manager and cannot be used 
// as normal sources in a session
class CanvasSurface : public Source
{
public:
    CanvasSurface(uint64_t id = 0);
    ~CanvasSurface();

    // implementation of source API
    void update (float dt) override;
    void render () override;
    bool playing () const override { return false; }
    void play (bool) override;
    void reload () override;
    bool playable () const  override { return false; }
    Failure failed () const override;
    uint texture() const override;

    glm::vec3 resolution() const;
    float aspectRatio() const ;
    
    glm::vec3 scale() const { return group(View::GEOMETRY)->scale_; }

    Group *label;
    Character *label_0_, *label_1_;

protected:

    void init() override;

    // control
    bool paused_;
};

// A canvas source is a source representing the content of a canvas rendered in the output
// It is added to the canvas manager session to render the canvas content to the output framebuffer
// It is manipulated in DisplayView to select which canvas to render and to change its 
// position and geometry on Displays
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
    guint64 playtime () const override { return 0; }
    Failure failed () const override;
    uint texture() const override;
    void accept (Visitor& v) override;

    inline size_t canvas () const { return canvas_index_; }
    inline void setCanvas (size_t index) { canvas_index_ = index; }

    glm::vec3 resolution() const;

    glm::ivec2 icon() const override;
    std::string info() const override;

protected:

    void init() override;

    size_t canvas_index_;
    FrameBuffer *rendered_output_;

    // control
    bool paused_;
    bool reset_;
};

#endif // CANVASSOURCE_H
