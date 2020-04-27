#ifndef IMAGESHADER_H
#define IMAGESHADER_H

#include <string>
#include <map>

#include "Shader.h"

class ImageShader : public Shader
{
public:

    ImageShader();
    virtual ~ImageShader() {}

    void use() override;
    void reset() override;
    void accept(Visitor& v) override;


    uint mask;
    float stipple;

    static const char* mask_names[];
    static std::vector< uint > mask_presets;
};

#endif // IMAGESHADER_H
