#ifndef PRIMITIVES_H
#define PRIMITIVES_H

#include <string>

#include "Scene.h"
class MediaPlayer;

/**
 * @brief The ImageSurface class is a Primitive to draw an image
 *
 * Image is read from the Ressouces (not an external file)
 * Height = 1.0, Width is set by the aspect ratio of the image
 */
class ImageSurface : public Primitive {

    void deleteGLBuffers_() override {}

public:
    ImageSurface(const std::string& path = "" );
    ~ImageSurface();

    void init () override;
    void draw (glm::mat4 modelview, glm::mat4 projection) override;
    void accept (Visitor& v) override;

    inline std::string getResource() const { return resource_; }

protected:
    std::string resource_;
    uint textureindex_;
};


/**
 * @brief The MediaSurface class is an ImageSurface to draw a video
 *
 * URI is passed to a Media Player to handle the video playback
 * Height = 1.0, Width is set by the aspect ratio of the image
 */
class MediaSurface : public ImageSurface {

    MediaPlayer *mediaplayer_;

public:
    MediaSurface(const std::string& uri);
    ~MediaSurface();

    void init () override;
    void draw (glm::mat4 modelview, glm::mat4 projection) override;
    void accept (Visitor& v) override;
    void update (float dt) override;

    MediaPlayer *getMediaPlayer() { return mediaplayer_; }
};


/**
 * @brief The Points class is a Primitive to draw Points
 */
class Points : public Primitive {

    uint pointsize_;

public:
    Points(std::vector<glm::vec3> points, glm::vec4 color, uint pointsize = 10);
    ~Points();

    virtual void init() override;
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
    LineStrip(std::vector<glm::vec3> points, glm::vec4 color, uint linewidth = 1);
    ~LineStrip();

    virtual void init() override;
    virtual void draw(glm::mat4 modelview, glm::mat4 projection) override;
    virtual void accept(Visitor& v) override;

    std::vector<glm::vec3> getPoints() { return points_; }
    glm::vec4 getColor() { return colors_[0]; }

    inline void setLineWidth(uint v) { linewidth_ = v; }
    inline uint getLineWidth() const { return linewidth_; }
};


/**
 * @brief The LineSquare class is a square LineStrip (width & height = 1.0)
 */
class LineSquare : public LineStrip {

    void deleteGLBuffers_() override {}

public:
    LineSquare(glm::vec4 color, uint linewidth = 1);

    void init() override;
    void accept(Visitor& v) override;
};


/**
 * @brief The LineCircle class is a circular LineStrip (diameter = 1.0)
 */
class LineCircle : public LineStrip {

    void deleteGLBuffers_() override {}

public:
    LineCircle(glm::vec4 color, uint linewidth = 1);

    void init() override;
    void accept(Visitor& v) override;
};



#endif // PRIMITIVES_H
