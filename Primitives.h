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

    inline std::string getFilename() const { return filename_; }

protected:
    std::string filename_;
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
    void update ( float dt ) override;

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

// Draw a line strip
class LineStrip : public Primitive {

    uint linewidth_;

public:
    LineStrip(std::vector<glm::vec3> points, glm::vec3 color, uint linewidth = 1);

    virtual void init() override;
    virtual void draw(glm::mat4 modelview, glm::mat4 projection) override;
    virtual void accept(Visitor& v) override;

    std::vector<glm::vec3> getPoints() { return points_; }
    glm::vec3 getColor() { return colors_[0]; }

    inline void setLineWidth(uint v) { linewidth_ = v; }
    inline uint getLineWidth() const { return linewidth_; }
};

class LineSquare : public LineStrip {

public:
    LineSquare(glm::vec3 color, uint linewidth = 1);
};

class LineCircle : public LineStrip {

    void deleteGLBuffers_() override {}

public:
    LineCircle(glm::vec3 color, uint linewidth = 1);

    void init() override;
    void accept(Visitor& v) override;
};


// Draw a Rectangle (triangle strip) with a texture
class ObjModel : public Primitive {

public:
    ObjModel(const std::string& path = "" );

    void init () override;
    void draw (glm::mat4 modelview, glm::mat4 projection) override;
    void accept (Visitor& v) override;

    inline std::string getFilename() const { return filename_; }

protected:
    std::string filename_;
    std::string texture_filename_;
    uint textureindex_;
};


#endif // PRIMITIVES_H
