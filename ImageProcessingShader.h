#ifndef IMAGEPROCESSINGSHADER_H
#define IMAGEPROCESSINGSHADER_H

#include <glm/glm.hpp>

#include "Shader.h"


class ImageProcessingShader : public Shader
{
public:

    ImageProcessingShader();
//    virtual ~ImageProcessingShader() {}

    void use() override;
    void reset() override;
    void accept(Visitor& v) override;

//    // textures resolution
//    glm::vec3 iChannelResolution[1];

    // color effects
    float brightness; // [-1 1]
    float contrast;   // [-1 1]
    float saturation; // [-1 1]
    float hueshift;   // [0 1]
    float threshold;  // [0 1]
    float lumakey;    // [0 1]
    // gamma
    glm::vec4 gamma;
    glm::vec4 levels;
    // discrete operations
    int nbColors;
    int invert;
    // chroma key
    glm::vec4 chromakey;
    float chromadelta;
    // filter identifyer
    // [0] No filter
    // [1 4] 4 x kernel operations;  Blur, Sharpen, Edge, Emboss
    // [5] 1 x convolution opening (denoising)
    // [6 11] 6 x convolutions: erosion 3x3, 5x5, 7x7, dilation 3x3, 5x5, 7x7
    int filterid;
    static const char* filter_names[12];

};



#endif // IMAGEPROCESSINGSHADER_H
