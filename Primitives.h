#ifndef PRIMITIVES_H
#define PRIMITIVES_H

#include <string>

#include "Scene.h"

// Draw a Rectangle (triangle strip) with a texture
class TexturedRectangle : public Primitive {

    std::string texturepath_;
    unsigned int textureindex_;

public:
    TexturedRectangle(const std::string& resourcepath);

    void draw(glm::mat4 modelview, glm::mat4 projection) override;
    void accept(Visitor& v) override { v.visit(*this);  }

    std::string getResourcePath() { return texturepath_; }
};


// Draw a Rectangle (triangle strip) with a media as animated texture
class MediaPlayer;

class MediaRectangle : public Primitive {

    MediaPlayer *mediaplayer_;
    std::string mediapath_;
    unsigned int textureindex_;

public:
    MediaRectangle(const std::string& mediapath);
    ~MediaRectangle();

    void update ( float dt ) override;
    void draw(glm::mat4 modelview, glm::mat4 projection) override;
    void accept(Visitor& v) override { v.visit(*this); }

    std::string getMediaPath() { return mediapath_; }
    MediaPlayer *getMediaPlayer() { return mediaplayer_;  }
};


// Draw a line strip
class LineStrip : public Primitive {

public:
    LineStrip(std::vector<glm::vec3> points, glm::vec3 color);

    void accept(Visitor& v) override { v.visit(*this);  }

    std::vector<glm::vec3> getPoints() { return points_; }
    glm::vec3 getColor() { return colors_[0]; }
};



#endif // PRIMITIVES_H
