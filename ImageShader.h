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

    void setBrightness(float v);
    void setContrast(float v);

private:
    float brightness;
    float contrast;
    bool imageshader_changed;
};

#endif // IMAGESHADER_H
