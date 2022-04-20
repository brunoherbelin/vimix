#ifndef IMAGEFILTER_H
#define IMAGEFILTER_H

#include <map>
#include <string>

#include <glm/glm.hpp>

class Surface;
class FrameBuffer;
class ImageFilteringShader;


class ImageFilter
{
    std::string code_;

public:

    ImageFilter();
    ImageFilter(const ImageFilter &other);

    ImageFilter& operator = (const ImageFilter& other);
    bool operator !=(const ImageFilter& other) const;

    // set the code of the filter
    inline void setCode(const std::string &code) { code_ = code; }

    // get the code of the filter
    inline std::string code() const { return code_; }
};


class ImageFilterRenderer
{
    Surface *surface_;
    FrameBuffer *buffer_;
    ImageFilteringShader *shader_;
    bool enabled_;

    ImageFilter filter_;

public:

    // instanciate an image filter at given resolution, with alpha channel
    ImageFilterRenderer(glm::vec3 resolution);
    ~ImageFilterRenderer();

    inline void setEnabled (bool on) { enabled_ = on; }
    inline bool enabled () const { return enabled_; }

    // set the texture to draw into the framebuffer
    void setInputTexture(uint t);

    // draw the input texture with filter on the framebuffer
    void draw();

    // get the texture id of the rendered framebuffer
    uint getOutputTexture() const;

    // set the code of the filter
    void setFilter(const ImageFilter &f);

    // get the code of the filter
    inline ImageFilter filter() const { return filter_; }

};


#endif // IMAGEFILTER_H
