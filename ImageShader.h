#ifndef IMAGESHADER_H
#define IMAGESHADER_H

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
};

#endif // IMAGESHADER_H
