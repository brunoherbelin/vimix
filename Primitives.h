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

protected:
    uint textureindex_;
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

    void init () override;
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


/**
 * @brief The LineStrip class is a Primitive to draw lines
 */
class LineStrip : public Primitive {

    uint linewidth_;

public:
    LineStrip(std::vector<glm::vec3> points, std::vector<glm::vec4> colors, uint linewidth = 1);

    virtual void draw(glm::mat4 modelview, glm::mat4 projection) override;
    virtual void accept(Visitor& v) override;

    std::vector<glm::vec3> getPoints() { return points_; }
    std::vector<glm::vec4> getColors() { return colors_; }

    inline void setLineWidth(uint v) { linewidth_ = v; }
    inline uint getLineWidth() const { return linewidth_; }
};


/**
 * @brief The LineSquare class is a square LineStrip (width & height = 1.0)
 */
class LineSquare : public LineStrip {

public:
    LineSquare(uint linewidth = 1);

    void init() override;
    void accept(Visitor& v) override;

    virtual ~LineSquare();
};


/**
 * @brief The LineCircle class is a circular LineStrip (diameter = 1.0)
 */
class LineCircle : public LineStrip {

public:
    LineCircle(uint linewidth = 1);

    void init() override;
    void accept(Visitor& v) override;

    virtual ~LineCircle();
};



#endif // PRIMITIVES_H
