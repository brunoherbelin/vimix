#ifndef PRIMITIVES_H
#define PRIMITIVES_H

#include <string>

#include "Scene.h"
#include "ImageShader.h"

class MediaPlayer;
class FrameBuffer;


/**
 * @brief The Surface class is a Primitive to draw an flat square
 * surface with texture coordinates.
 *
 * It uses a unique vertex array object to draw all surfaces.
 *
 */
class Surface : public Primitive {

public:
    Surface(Shader *s = new ImageShader);
    virtual ~Surface();

    void init () override;
    void draw (glm::mat4 modelview, glm::mat4 projection) override;
    void accept (Visitor& v) override;

    inline void setTextureIndex(uint t) { textureindex_ = t; }
    inline uint textureIndex() const { return textureindex_; }

    inline void setMirrorTexture(bool m) { mirror_ = m; }
    inline bool mirrorTexture() { return mirror_; }

protected:
    uint textureindex_;
    bool mirror_;
};


/**
 * @brief The ImageSurface class is a Surface to draw an image
 *
 * It creates an ImageShader for rendering.
 *
 * Image is read from the Ressouces (not an external file)
 * Height = 1.0, Width is set by the aspect ratio of the image
 */
class ImageSurface : public Surface {

public:
    ImageSurface(const std::string& path, Shader *s = new ImageShader);

    void init () override;
    void accept (Visitor& v) override;

    inline std::string resource() const { return resource_; }

protected:
    std::string resource_;
};


/**
 * @brief The MediaSurface class is a Surface to draw a video
 *
 * URI is passed to a Media Player to handle the video playback
 * Height = 1.0, Width is set by the aspect ratio of the image
 */
class MediaSurface : public Surface {

public:
    MediaSurface(const std::string& p, Shader *s = new ImageShader);
    ~MediaSurface();

    void init () override;
    void draw (glm::mat4 modelview, glm::mat4 projection) override;
    void accept (Visitor& v) override;
    void update (float dt) override;

    inline std::string path() const { return path_; }
    inline MediaPlayer *mediaPlayer() const { return mediaplayer_; }

protected:
    std::string path_;
    MediaPlayer *mediaplayer_;
};

/**
 * @brief The FrameBufferSurface class is a Surface to draw a framebuffer
 *
 * URI is passed to a Media Player to handle the video playback
 * Height = 1.0, Width is set by the aspect ratio of the image
 */
class FrameBufferSurface : public Surface {

public:
    FrameBufferSurface(FrameBuffer *fb, Shader *s = new ImageShader);

    void draw (glm::mat4 modelview, glm::mat4 projection) override;
    void accept (Visitor& v) override;

    inline FrameBuffer *getFrameBuffer() const { return frame_buffer_; }

protected:
    FrameBuffer *frame_buffer_;
};

/**
 * @brief The Points class is a Primitive to draw Points
 */
class Points : public Primitive {

    uint pointsize_;

public:
    Points(std::vector<glm::vec3> points, glm::vec4 color, uint pointsize = 10);

    virtual void draw(glm::mat4 modelview, glm::mat4 projection) override;
    virtual void accept(Visitor& v) override;

    std::vector<glm::vec3> getPoints() { return points_; }
    glm::vec4 getColor() { return colors_[0]; }

    inline void setPointSize(uint v) { pointsize_ = v; }
    inline uint getPointSize() const { return pointsize_; }
};


class HLine : public Primitive {

public:

    HLine(float width = 1.f);
    virtual ~HLine();

    void init () override;
    void draw(glm::mat4 modelview, glm::mat4 projection) override;

    glm::vec4 color;
    float width;
};

class VLine : public Primitive {

public:

    VLine(float width = 1.f);
    virtual ~VLine();

    void init () override;
    void draw(glm::mat4 modelview, glm::mat4 projection) override;

    glm::vec4 color;
    float width;
};

/**
 * @brief The LineSquare class is a group of 4 lines (width & height = 1.0)
 */
class LineSquare : public Group {

    HLine *top_, *bottom_;
    VLine *left_, *right_;

public:
    LineSquare(float linewidth = 1.f);

    void setLineWidth(float v);
    inline float lineWidth() const { return top_->width; }

    void setColor(glm::vec4 c);
    inline glm::vec4 color() const { return top_->color; }
};

/**
 * @brief The LineStrip class is a Primitive to draw lines
 */
class LineStrip : public Primitive {

public:
    LineStrip(const std::vector<glm::vec2> &path, float linewidth = 1.f);
    virtual ~LineStrip();

    virtual void init () override;
    virtual void accept(Visitor& v) override;

    inline std::vector<glm::vec2> path() { return path_; }
    inline float lineWidth() const { return linewidth_ * 500.f; }

    void changePath(std::vector<glm::vec2> path);
    void editPath(uint index, glm::vec2 position);
    void setLineWidth(float linewidth);

protected:
    float linewidth_;
    uint arrayBuffer_;
    std::vector<glm::vec2> path_;
    virtual void updatePath();

};

/**
 * @brief The LineLoop class is a LineStrip with closed path
 */
class LineLoop : public LineStrip {

public:
    LineLoop(const std::vector<glm::vec2> &path, float linewidth = 1.f);

protected:
    void updatePath() override;
};


/**
 * @brief The LineCircle class is a circular LineLoop (diameter = 1.0)
 */
class LineCircle : public LineLoop {

public:
    LineCircle(float linewidth = 1.f);

};



#endif // PRIMITIVES_H
