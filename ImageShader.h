#ifndef IMAGESHADER_H
#define IMAGESHADER_H

#include <string>
#include <map>

#ifdef __APPLE__
#include <sys/types.h>
#endif

#include "Shader.h"

class ImageShader : public Shader
{

public:
    ImageShader();

    void use() override;
    void reset() override;
    void accept(Visitor& v) override;
    void operator = (const ImageShader &S);

    uint mask_texture;

    // uniforms
    float stipple;
};

class AlphaShader : public ImageShader
{

public:
    AlphaShader();

};


class MaskShader : public Shader
{

public:
    MaskShader();

    void use() override;
    void reset() override;
    void accept(Visitor& v) override;
    void operator = (const MaskShader &S);

    enum Modes {
        NONE = 0,
        PAINT = 1,
        SHAPE = 2
    };
    uint mode;

    enum Shapes {
        ELIPSE = 0,
        OBLONG = 1,
        RECTANGLE = 2,
        HORIZONTAL = 3,
        VERTICAL = 4
    };
    uint shape;

    // uniforms
    glm::vec2 size;
    float blur;

    int option;
    int effect;
    glm::vec4 cursor;
    glm::vec3 brush;

    static const char* mask_names[3];
    static const char* mask_shapes[5];
};

#endif // IMAGESHADER_H
