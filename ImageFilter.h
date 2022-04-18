#ifndef IMAGEFILTER_H
#define IMAGEFILTER_H

#include <map>

class Surface;
class FrameBuffer;

#include "Shader.h"

class ImageFilteringShader : public Shader
{
    ShadingProgram custom_shading_;

    // fragment shader GLSL code
    std::string code_;

    // list of uniform vars in GLSL
    // with associated pair (current_value, default_values) in range [0.f 1.f]
    std::map<std::string, glm::vec2> uniforms_;

public:

    ImageFilteringShader();
    ~ImageFilteringShader();

    void use() override;
    void reset() override;
    void accept(Visitor& v) override;
    void copy(ImageFilteringShader const& S);

    // set fragment shader code and uniforms (with default values)
    void setFragmentCode(const std::string &code, std::map<std::string, float> parameters);
};


class ImageFilter
{
    Surface *surface_;
    FrameBuffer *buffer_;
    ImageFilteringShader *shader_;
    uint type_;

public:

    // instanciate an image filter at given resolution, with alpha channel
    ImageFilter(glm::vec3 resolution, bool useAlpha = false);
    ~ImageFilter();

    // set the texture to draw into the framebuffer
    void setInputTexture(uint t);

//    typedef enum {

//    } ;


    void setFilter(uint filter);

    // draw the input texture with filter on the framebuffer
    void draw();

    // get the texture id of the rendered framebuffer
    uint getOutputTexture() const;

};


#endif // IMAGEFILTER_H
