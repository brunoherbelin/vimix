#ifndef RENDERVIEW_H
#define RENDERVIEW_H

#include <vector>
#include <future>

#include "View.h"
#include "FrameBuffer.h"

class RenderView : public View
{
    friend class Session;

    // rendering FBO
    FrameBuffer *frame_buffer_;
    Surface *fading_overlay_;

    // promises of returning thumbnails after an update
    std::vector< std::promise<FrameBufferImage *> > thumbnailer_;

public:
    RenderView ();
    ~RenderView ();

    // render frame (in opengl context)
    void draw () override;
    bool canSelect(Source *) override { return false; }

    void setResolution (glm::vec3 resolution, bool useAlpha = false);
    glm::vec3 resolution() const { return frame_buffer_->resolution(); }

    // current frame
    inline FrameBuffer *frame () const { return frame_buffer_; }

    // size descriptions
    enum AspectRatio {
        AspectRatio_4_3 = 0,
        AspectRatio_3_2,
        AspectRatio_16_10,
        AspectRatio_16_9,
        AspectRatio_21_9,
        AspectRatio_Custom
    };
    static const char* ratio_preset_name[6];
    static glm::vec2 ratio_preset_value[6];
    static const char* height_preset_name[5];
    static float height_preset_value[5];

    static glm::vec3 resolutionFromPreset(int ar, int h);
    static glm::ivec2 presetFromResolution(glm::vec3 res);

protected:

    void setFading(float f = 0.f);
    float fading() const;

    // get a thumbnail outside of opengl context; wait for a promise to be fullfiled after draw
    void drawThumbnail();
    FrameBufferImage *thumbnail ();
    FrameBuffer *frame_thumbnail_;
};

#endif // RENDERVIEW_H
