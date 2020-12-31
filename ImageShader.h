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


class MaskShader : public Shader
{

public:

    MaskShader();

    void use() override;
    void reset() override;
    void accept(Visitor& v) override;
    void operator = (const MaskShader &S);

    uint mode;

    // uniforms
    float blur;
    glm::vec2 size;

    static const char* mask_names[6];
    static std::vector< ShadingProgram* > mask_programs;
};

#endif // IMAGESHADER_H
