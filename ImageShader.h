#ifndef IMAGESHADER_H
#define IMAGESHADER_H

#include "Shader.h"

class ImageShader : public Shader
{
public:

    ImageShader();
    virtual ~ImageShader() {}

    virtual void use();
    virtual void reset();

    float brightness;
    float contrast;
};

#endif // IMAGESHADER_H
