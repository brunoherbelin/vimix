/*
 * This file is part of vimix - video live mixer
 * https://github.com/brunoherbelin/vimix
 * (C) 2019-2022 Bruno Herbelin <bruno.herbelin@gmail.com>
 * Distributed under GNU GPL3+ License
**/
// Inspired by https://www.shadertoy.com/view/MlVXWD by tudordot


uniform float Red;
uniform float Green;
uniform float Blue;
uniform float Threshold;
uniform float Tolerance;

//conversion between rgb and YUV
mat4 RGBtoYUV = mat4(0.257,  0.439, -0.148, 0.0,
                     0.504, -0.368, -0.291, 0.0,
                     0.098, -0.071,  0.439, 0.0,
                     0.0625, 0.500,  0.500, 1.0 );

// color to be removed
vec4 chromaKey = vec4(Red, Green, Blue, 1.);

//compute color distance in the UV (CbCr, PbPr) plane
float colordistance(vec3 yuv, vec3 keyYuv, vec2 tol)
{
    // color distance in the UV (CbCr, PbPr) plane
//    float tmp = sqrt(pow(keyYuv.y - yuv.y, 2.0) + pow(keyYuv.z - yuv.z, 2.0));
    vec2 dif = yuv.yz - keyYuv.yz;
    float tmp = sqrt(dot(dif, dif));

    if (tmp < tol.x)
        return 0.0;
    else if (tmp < tol.y)
        return (tmp - tol.x)/(tol.y - tol.x);
    else
        return 1.0;
}

void mainImage( out vec4 fragColor, in vec2 fragCoord )
{
    vec4 texColor0 = texture(iChannel0, fragCoord.xy / iResolution.xy);

    //convert from RGB to YCvCr/YUV
    vec4 keyYUV = RGBtoYUV * chromaKey;
    vec4 yuv = RGBtoYUV * texColor0;

    float t = (1. - Tolerance) * 0.2;
    float mask = 1.0 - colordistance(yuv.rgb, keyYUV.rgb, vec2(t, t + 0.2));
    vec4 col = max(texColor0 - mask * chromaKey, 0.0);

    t = Threshold * Threshold * 0.2;
    col.a -= 1.0 - colordistance(yuv.rgb, keyYUV.rgb, vec2(t, t + 0.25));

    fragColor = col;
}
