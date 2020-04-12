#ifndef PRIMITIVES_H
#define PRIMITIVES_H

#include <string>

#include "Scene.h"

// Draw a Rectangle (triangle strip) with a texture
class ImageSurface : public Primitive {

    void deleteGLBuffers_() override {}

public:
    ImageSurface(const std::string& path = "" );

    void init () override;
    void draw (glm::mat4 modelview, glm::mat4 projection) override;
    void accept (Visitor& v) override;

    inline std::string getResource() const { return resource_; }

protected:
    std::string resource_;
    uint textureindex_;
};


// Draw a Rectangle (triangle strip) with a media as animated texture
class MediaPlayer;

class MediaSurface : public ImageSurface {

    MediaPlayer *mediaplayer_;

public:
    MediaSurface(const std::string& path);
    ~MediaSurface();

    void init () override;
    void draw (glm::mat4 modelview, glm::mat4 projection) override;
    void accept (Visitor& v) override;
    void update (float dt) override;

    MediaPlayer *getMediaPlayer() { return mediaplayer_; }
};

//// Draw a Frame Buffer
//class FrameBufferSurface : public Primitive {

//    void deleteGLBuffers_() {}

//public:
//    FrameBufferSurface();

//    void init () override;
//    void draw(glm::mat4 modelview, glm::mat4 projection) override;
//    void accept(Visitor& v) override;

//};


// Draw a Point
class Points : public Primitive {

    uint pointsize_;

public:
    Points(std::vector<glm::vec3> points, glm::vec4 color, uint pointsize = 10);

    virtual void init() override;
    virtual void draw(glm::mat4 modelview, glm::mat4 projection) override;
    virtual void accept(Visitor& v) override;

    std::vector<glm::vec3> getPoints() { return points_; }
    glm::vec4 getColor() { return colors_[0]; }

    inline void setPointSize(uint v) { pointsize_ = v; }
    inline uint getPointSize() const { return pointsize_; }
};

// Draw a line strip
class LineStrip : public Primitive {

    uint linewidth_;

public:
    LineStrip(std::vector<glm::vec3> points, glm::vec4 color, uint linewidth = 1);

    virtual void init() override;
    virtual void draw(glm::mat4 modelview, glm::mat4 projection) override;
    virtual void accept(Visitor& v) override;

    std::vector<glm::vec3> getPoints() { return points_; }
    glm::vec4 getColor() { return colors_[0]; }

    inline void setLineWidth(uint v) { linewidth_ = v; }
    inline uint getLineWidth() const { return linewidth_; }
};

class LineSquare : public LineStrip {

    void deleteGLBuffers_() override {}

public:
    LineSquare(glm::vec4 color, uint linewidth = 1);

    void init() override;
    void accept(Visitor& v) override;
};

class LineCircle : public LineStrip {

    void deleteGLBuffers_() override {}

public:
    LineCircle(glm::vec4 color, uint linewidth = 1);

    void init() override;
    void accept(Visitor& v) override;
};



#endif // PRIMITIVES_H
