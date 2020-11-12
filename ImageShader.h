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

    uint mask;
    uint custom_textureindex;
    float stipple;
    glm::vec4 uv;

    static const char* mask_names[11];
    static std::vector< uint > mask_presets;
};

#endif // IMAGESHADER_H
