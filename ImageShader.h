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
    virtual void accept(Visitor& v);

    float brightness;
    float contrast;
    float stipple;
};

#endif // IMAGESHADER_H
