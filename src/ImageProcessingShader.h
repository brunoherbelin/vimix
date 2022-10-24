#ifndef IMAGEPROCESSINGSHADER_H
#define IMAGEPROCESSINGSHADER_H

#include "Shader.h"

class ImageProcessingShader : public Shader
{
public:

    ImageProcessingShader();

    void use() override;
    void reset() override;
    void accept(Visitor& v) override;

    void copy(ImageProcessingShader const& S);

    // color effects
    float brightness; // [-1 1]
    float contrast;   // [-1 1]
    float saturation; // [-1 1]
    float hueshift;   // [0 1]
    float threshold;  // [0 1]
    // gamma
    glm::vec4 gamma;
    glm::vec4 levels;
    // discrete operations
    int nbColors;
    int invert;

};



#endif // IMAGEPROCESSINGSHADER_H
