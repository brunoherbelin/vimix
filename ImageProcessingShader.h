#ifndef IMAGEPROCESSINGSHADER_H
#define IMAGEPROCESSINGSHADER_H

#include "Shader.h"

class ImageProcessingShader : public Shader
{
public:

    ImageProcessingShader();
    virtual ~ImageShader() {}

    virtual void use();
    virtual void reset();
    virtual void accept(Visitor& v);

    float brightness;
    float contrast;
    float stipple;
};



#endif // IMAGEPROCESSINGSHADER_H
