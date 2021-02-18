#version 330 core

/*
** Gamma correction
** Details: http://blog.mouaif.org/2009/01/22/photoshop-gamma-correction-shader/
*/
#define GammaCorrection(color, gamma) pow( color, 1.0 / vec3(gamma))

/*
** Levels control (input (+gamma), output)
** Details: http://blog.mouaif.org/2009/01/28/levels-control-shader/
*/
#define LevelsControlInputRange(color, minInput, maxInput)  min(max(color - vec3(minInput), 0.0) / (vec3(maxInput) - vec3(minInput)), 1.0)
#define LevelsControlInput(color, minInput, gamma, maxInput) GammaCorrection(LevelsControlInputRange(color, minInput, maxInput), gamma)
#define LevelsControlOutputRange(color, minOutput, maxOutput)  mix(vec3(minOutput), vec3(maxOutput), color)
#define LevelsControl(color, minInput, gamma, maxInput, minOutput, maxOutput)   LevelsControlOutputRange(LevelsControlInput(color, minInput, gamma, maxInput), minOutput, maxOutput)

#define ONETHIRD 0.333333
#define TWOTHIRD 0.666666
#define EPSILON  0.000001

out vec4 FragColor;

// from vertex shader (interpolated)
in vec4 vertexColor;
in vec2 vertexUV;

vec4 texcoord;

// image processing specific
uniform sampler2D iChannel0;             // input channel (texture id).
uniform vec3      iResolution;           // viewport resolution (in pixels)
uniform mat4      iTransform;            // UV image transformation

// Image shader uniforms
uniform vec4 color;
uniform vec4 uv;

// Image processing uniforms
uniform float contrast;
uniform float brightness;
uniform float saturation;
uniform vec4  gamma;
uniform vec4  levels;
uniform float hueshift;
uniform vec4  chromakey;
uniform float chromadelta;
uniform float threshold;
uniform float lumakey;
uniform int   nbColors;
uniform int   invert;
uniform int   filterid;

// conversion between rgb and YUV
const mat4 RGBtoYUV = mat4(0.257,  0.439, -0.148, 0.0,
                           0.504, -0.368, -0.291, 0.0,
                           0.098, -0.071,  0.439, 0.0,
                           0.0625, 0.500,  0.500, 1.0 );

const mat3 KERNEL[5] = mat3[5]( mat3( 0.0, 0.0, 0.0,
                                      0.0, 1.0, 0.0,
                                      0.0, 0.0, 0.0),
                                mat3( 0.0625, 0.125, 0.0625,
                                      0.125, 0.25, 0.125,
                                      0.0625, 0.125, 0.0625),
                                mat3( -1.0, -1.0, -1.0,
                                      -1.0, 9.0, -1.0,
                                      -1.0, -1.0, -1.0),
                                mat3( -2.0, 1.0, -2.0,
                                      1.0, 4.0, 1.0,
                                      -2.0, 1.0, -2.0),
                                mat3( -2.0, -1.0, 0.0,
                                      -1.0, 1.0, 1.0,
                                      0.0, 1.0, 2.0)
                                );

vec3 erosion(int N, vec2 filter_step)
{
    vec3 minValue = vec3(1.0);

    minValue = min(texture(iChannel0, texcoord.xy + vec2 (0.0,0.0) * filter_step ).rgb, minValue);
    minValue = min(texture(iChannel0, texcoord.xy + vec2 (0.0,-1.0) * filter_step ).rgb, minValue);
    minValue = min(texture(iChannel0, texcoord.xy + vec2 (-1.0, 0.0) * filter_step ).rgb, minValue);
    minValue = min(texture(iChannel0, texcoord.xy + vec2 (1.0, 0.0) * filter_step ).rgb, minValue);
    minValue = min(texture(iChannel0, texcoord.xy + vec2 (0.0, 1.0) * filter_step ).rgb, minValue);
    if (N < 1)
        return minValue;
    minValue = min(texture(iChannel0, texcoord.xy + vec2 (-1.0, -2.0) * filter_step ).rgb, minValue);
    minValue = min(texture(iChannel0, texcoord.xy + vec2 (0.0,-2.0) * filter_step ).rgb, minValue);
    minValue = min(texture(iChannel0, texcoord.xy + vec2 (1.0,-2.0) * filter_step ).rgb, minValue);
    minValue = min(texture(iChannel0, texcoord.xy + vec2 (-1.0,2.0) * filter_step ).rgb, minValue);
    minValue = min(texture(iChannel0, texcoord.xy + vec2 (0.0, 2.0) * filter_step ).rgb, minValue);
    minValue = min(texture(iChannel0, texcoord.xy + vec2 (1.0, 2.0) * filter_step ).rgb, minValue);
    minValue = min(texture(iChannel0, texcoord.xy + vec2 (-2.0, 1.0) * filter_step ).rgb, minValue);
    minValue = min(texture(iChannel0, texcoord.xy + vec2 (-1.0, 1.0) * filter_step ).rgb, minValue);
    minValue = min(texture(iChannel0, texcoord.xy + vec2 ( 1.0, 1.0) * filter_step ).rgb, minValue);
    minValue = min(texture(iChannel0, texcoord.xy + vec2 ( 2.0, 1.0) * filter_step ).rgb, minValue);
    minValue = min(texture(iChannel0, texcoord.xy + vec2 (-2.0, -1.0) * filter_step ).rgb, minValue);
    minValue = min(texture(iChannel0, texcoord.xy + vec2 (-1.0, -1.0) * filter_step ).rgb, minValue);
    minValue = min(texture(iChannel0, texcoord.xy + vec2 ( 1.0, -1.0) * filter_step ).rgb, minValue);
    minValue = min(texture(iChannel0, texcoord.xy + vec2 ( 2.0, -1.0) * filter_step ).rgb, minValue);
    minValue = min(texture(iChannel0, texcoord.xy + vec2 (-2.0, 0.0) * filter_step ).rgb, minValue);
    minValue = min(texture(iChannel0, texcoord.xy + vec2 ( 2.0, 0.0) * filter_step ).rgb, minValue);
    if (N < 2)
        return minValue;
    minValue = min(texture(iChannel0, texcoord.xy + vec2 (-1.0, -3.0) * filter_step ).rgb, minValue);
    minValue = min(texture(iChannel0, texcoord.xy + vec2 (0.0,-3.0) * filter_step ).rgb, minValue);
    minValue = min(texture(iChannel0, texcoord.xy + vec2 (1.0,-3.0) * filter_step ).rgb, minValue);
    minValue = min(texture(iChannel0, texcoord.xy + vec2 (-1.0,3.0) * filter_step ).rgb, minValue);
    minValue = min(texture(iChannel0, texcoord.xy + vec2 (0.0, 3.0) * filter_step ).rgb, minValue);
    minValue = min(texture(iChannel0, texcoord.xy + vec2 (1.0, 3.0) * filter_step ).rgb, minValue);
    minValue = min(texture(iChannel0, texcoord.xy + vec2 (-2.0, 2.0) * filter_step ).rgb, minValue);
    minValue = min(texture(iChannel0, texcoord.xy + vec2 ( 2.0, 2.0) * filter_step ).rgb, minValue);
    minValue = min(texture(iChannel0, texcoord.xy + vec2 (-2.0, -2.0) * filter_step ).rgb, minValue);
    minValue = min(texture(iChannel0, texcoord.xy + vec2 ( 2.0, -2.0) * filter_step ).rgb, minValue);
    minValue = min(texture(iChannel0, texcoord.xy + vec2 (-3.0, -1.0) * filter_step ).rgb, minValue);
    minValue = min(texture(iChannel0, texcoord.xy + vec2 (3.0, -1.0) * filter_step ).rgb, minValue);
    minValue = min(texture(iChannel0, texcoord.xy + vec2 (-3.0, 1.0) * filter_step ).rgb, minValue);
    minValue = min(texture(iChannel0, texcoord.xy + vec2 ( 3.0, 1.0) * filter_step ).rgb, minValue);
    minValue = min(texture(iChannel0, texcoord.xy + vec2 (-3.0, 0.0) * filter_step ).rgb, minValue);
    minValue = min(texture(iChannel0, texcoord.xy + vec2 ( 3.0, 0.0) * filter_step ).rgb, minValue);

    return minValue;
}

vec3 dilation(int N, vec2 filter_step)
{
    vec3 maxValue = vec3(0.0);

    maxValue = max(texture(iChannel0, texcoord.xy + vec2 (0.0, 0.0) * filter_step ).rgb, maxValue);
    maxValue = max(texture(iChannel0, texcoord.xy + vec2 (0.0,-1.0) * filter_step ).rgb, maxValue);
    maxValue = max(texture(iChannel0, texcoord.xy + vec2 (-1.0, 0.0) * filter_step ).rgb, maxValue);
    maxValue = max(texture(iChannel0, texcoord.xy + vec2 (1.0, 0.0) * filter_step ).rgb, maxValue);
    maxValue = max(texture(iChannel0, texcoord.xy + vec2 (0.0, 1.0) * filter_step ).rgb, maxValue);
    if (N < 1)
        return maxValue;
    maxValue = max(texture(iChannel0, texcoord.xy + vec2 (-1.0, -2.0) * filter_step ).rgb, maxValue);
    maxValue = max(texture(iChannel0, texcoord.xy + vec2 (0.0,-2.0) * filter_step ).rgb, maxValue);
    maxValue = max(texture(iChannel0, texcoord.xy + vec2 (1.0,-2.0) * filter_step ).rgb, maxValue);
    maxValue = max(texture(iChannel0, texcoord.xy + vec2 (-1.0,2.0) * filter_step ).rgb, maxValue);
    maxValue = max(texture(iChannel0, texcoord.xy + vec2 (0.0, 2.0) * filter_step ).rgb, maxValue);
    maxValue = max(texture(iChannel0, texcoord.xy + vec2 (1.0, 2.0) * filter_step ).rgb, maxValue);
    maxValue = max(texture(iChannel0, texcoord.xy + vec2 (-2.0, 1.0) * filter_step ).rgb, maxValue);
    maxValue = max(texture(iChannel0, texcoord.xy + vec2 (-1.0, 1.0) * filter_step ).rgb, maxValue);
    maxValue = max(texture(iChannel0, texcoord.xy + vec2 ( 1.0, 1.0) * filter_step ).rgb, maxValue);
    maxValue = max(texture(iChannel0, texcoord.xy + vec2 ( 2.0, 1.0) * filter_step ).rgb, maxValue);
    maxValue = max(texture(iChannel0, texcoord.xy + vec2 (-2.0, -1.0) * filter_step ).rgb, maxValue);
    maxValue = max(texture(iChannel0, texcoord.xy + vec2 (-1.0, -1.0) * filter_step ).rgb, maxValue);
    maxValue = max(texture(iChannel0, texcoord.xy + vec2 ( 1.0, -1.0) * filter_step ).rgb, maxValue);
    maxValue = max(texture(iChannel0, texcoord.xy + vec2 ( 2.0, -1.0) * filter_step ).rgb, maxValue);
    maxValue = max(texture(iChannel0, texcoord.xy + vec2 (-2.0, 0.0) * filter_step ).rgb, maxValue);
    maxValue = max(texture(iChannel0, texcoord.xy + vec2 ( 2.0, 0.0) * filter_step ).rgb, maxValue);
    if (N < 2)
        return maxValue;
    maxValue = max(texture(iChannel0, texcoord.xy + vec2 (-1.0, -3.0) * filter_step ).rgb, maxValue);
    maxValue = max(texture(iChannel0, texcoord.xy + vec2 (0.0,-3.0) * filter_step ).rgb, maxValue);
    maxValue = max(texture(iChannel0, texcoord.xy + vec2 (1.0,-3.0) * filter_step ).rgb, maxValue);
    maxValue = max(texture(iChannel0, texcoord.xy + vec2 (-1.0,3.0) * filter_step ).rgb, maxValue);
    maxValue = max(texture(iChannel0, texcoord.xy + vec2 (0.0, 3.0) * filter_step ).rgb, maxValue);
    maxValue = max(texture(iChannel0, texcoord.xy + vec2 (1.0, 3.0) * filter_step ).rgb, maxValue);
    maxValue = max(texture(iChannel0, texcoord.xy + vec2 (-2.0, 2.0) * filter_step ).rgb, maxValue);
    maxValue = max(texture(iChannel0, texcoord.xy + vec2 ( 2.0, 2.0) * filter_step ).rgb, maxValue);
    maxValue = max(texture(iChannel0, texcoord.xy + vec2 (-2.0, -2.0) * filter_step ).rgb, maxValue);
    maxValue = max(texture(iChannel0, texcoord.xy + vec2 ( 2.0, -2.0) * filter_step ).rgb, maxValue);
    maxValue = max(texture(iChannel0, texcoord.xy + vec2 (-3.0, -1.0) * filter_step ).rgb, maxValue);
    maxValue = max(texture(iChannel0, texcoord.xy + vec2 (3.0, -1.0) * filter_step ).rgb, maxValue);
    maxValue = max(texture(iChannel0, texcoord.xy + vec2 (-3.0, 1.0) * filter_step ).rgb, maxValue);
    maxValue = max(texture(iChannel0, texcoord.xy + vec2 ( 3.0, 1.0) * filter_step ).rgb, maxValue);
    maxValue = max(texture(iChannel0, texcoord.xy + vec2 (-3.0, 0.0) * filter_step ).rgb, maxValue);
    maxValue = max(texture(iChannel0, texcoord.xy + vec2 ( 3.0, 0.0) * filter_step ).rgb, maxValue);

    return maxValue;
}


vec3 opening(vec2 filter_step)
{
    // 1) erosion
    vec3 minValue1 = vec3(1.0);
    minValue1 = min(texture(iChannel0, texcoord.xy + vec2 (0.0, 0.0) * filter_step ).rgb, minValue1);
    minValue1 = min(texture(iChannel0, texcoord.xy + vec2 (0.0, 1.0) * filter_step ).rgb, minValue1);
    minValue1 = min(texture(iChannel0, texcoord.xy + vec2 (0.0, 2.0) * filter_step ).rgb, minValue1);
    minValue1 = min(texture(iChannel0, texcoord.xy + vec2 (1.0, 1.0) * filter_step ).rgb, minValue1);
    minValue1 = min(texture(iChannel0, texcoord.xy + vec2 (1.0, -1.0) * filter_step ).rgb, minValue1);
    vec3 minValue2 = vec3(1.0);
    minValue2 = min(texture(iChannel0, texcoord.xy + vec2 (0.0, 0.0) * filter_step ).rgb, minValue2);
    minValue2 = min(texture(iChannel0, texcoord.xy + vec2 (-1.0, 0.0) * filter_step ).rgb, minValue2);
    minValue2 = min(texture(iChannel0, texcoord.xy + vec2 (-2.0, 0.0) * filter_step ).rgb, minValue2);
    minValue2 = min(texture(iChannel0, texcoord.xy + vec2 (1.0, -1.0) * filter_step ).rgb, minValue2);
    minValue2 = min(texture(iChannel0, texcoord.xy + vec2 (-1.0, -1.0) * filter_step ).rgb, minValue2);
    vec3 minValue3 = vec3(1.0);
    minValue3 = min(texture(iChannel0, texcoord.xy + vec2 (0.0, 0.0) * filter_step ).rgb, minValue3);
    minValue3 = min(texture(iChannel0, texcoord.xy + vec2 (0.0, -1.0) * filter_step ).rgb, minValue3);
    minValue3 = min(texture(iChannel0, texcoord.xy + vec2 (0.0, -2.0) * filter_step ).rgb, minValue3);
    minValue3 = min(texture(iChannel0, texcoord.xy + vec2 (-1.0, -1.0) * filter_step ).rgb, minValue3);
    minValue3 = min(texture(iChannel0, texcoord.xy + vec2 (-1.0, 1.0) * filter_step ).rgb, minValue3);
    vec3 minValue4 = vec3(1.0);
    minValue4 = min(texture(iChannel0, texcoord.xy + vec2 (0.0, 0.0) * filter_step ).rgb, minValue4);
    minValue4 = min(texture(iChannel0, texcoord.xy + vec2 (1.0, 0.0) * filter_step ).rgb, minValue4);
    minValue4 = min(texture(iChannel0, texcoord.xy + vec2 (2.0, 0.0) * filter_step ).rgb, minValue4);
    minValue4 = min(texture(iChannel0, texcoord.xy + vec2 (1.0, 1.0) * filter_step ).rgb, minValue4);
    minValue4 = min(texture(iChannel0, texcoord.xy + vec2 (-1.0, 1.0) * filter_step ).rgb, minValue4);

    // 2) dilation
    vec3 maxValue = vec3(0.0);
    maxValue = max(minValue1, maxValue);
    maxValue = max(minValue2, maxValue);
    maxValue = max(minValue3, maxValue);
    maxValue = max(minValue4, maxValue);

    return maxValue;
}

vec3 convolution(mat3 kernel, vec2 filter_step)
{
    int i = 0, j = 0;
    vec3 sum = vec3(0.0);

    for (i = 0; i<3; ++i)
        for (j = 0; j<3; ++j)
            sum += texture(iChannel0, texcoord.xy + filter_step * vec2 (i-1, j-1) ).rgb * kernel[i][j];

    return sum;
}

vec3 apply_filter() {

    vec2 filter_step = 1.f / textureSize(iChannel0, 0);

    if (filterid < 1 || filterid > 11)
        return texture(iChannel0, texcoord.xy).rgb;
    else if (filterid < 5)
        return convolution( KERNEL[filterid], filter_step);
    else if (filterid < 6)
        return opening(filter_step);
    else if (filterid < 9)
        return erosion( filterid - 6 , filter_step);
    else
        return dilation( filterid - 9, filter_step);
}

/*
** Hue, saturation, luminance <=> Red Green Blue
*/

float HueToRGB(float f1, float f2, float hue)
{
    float res;

    hue += mix( -float( hue > 1.0 ), 1.0, float(hue < 0.0) );

    res = mix( f1, mix( clamp( f1 + (f2 - f1) * ((2.0 / 3.0) - hue) * 6.0, 0.0, 1.0) , mix( f2,  clamp(f1 + (f2 - f1) * 6.0 * hue, 0.0, 1.0), float((6.0 * hue) < 1.0)),  float((2.0 * hue) < 1.0)), float((3.0 * hue) < 2.0) );

    return res;
}

vec3 HSV2RGB(vec3 hsl)
{
    vec3 rgb;
    float f1, f2;

    f2 = mix( (hsl.z + hsl.y) - (hsl.y * hsl.z), hsl.z * (1.0 + hsl.y), float(hsl.z < 0.5) );

    f1 = 2.0 * hsl.z - f2;

    rgb.r = HueToRGB(f1, f2, hsl.x + ONETHIRD);
    rgb.g = HueToRGB(f1, f2, hsl.x);
    rgb.b = HueToRGB(f1, f2, hsl.x - ONETHIRD);

    rgb =  mix( rgb, vec3(hsl.z), float(hsl.y < EPSILON));

    return rgb;
}

vec3 RGB2HSV( vec3 color )
{
    vec3 hsl = vec3(0.0); // init to 0

    float fmin = min(min(color.r, color.g), color.b);    //Min. value of RGB
    float fmax = max(max(color.r, color.g), color.b);    //Max. value of RGB
    float delta = fmax - fmin + EPSILON;             //Delta RGB value

    vec3 deltaRGB = ( ( vec3(fmax) - color ) / 6.0  + vec3(delta) / 2.0 ) / delta ;

    hsl.z = (fmax + fmin) / 2.0; // Luminance

    hsl.y = delta / ( EPSILON + mix( 2.0 - fmax - fmin, fmax + fmin, float(hsl.z < 0.5)) );

    hsl.x = mix( hsl.x, TWOTHIRD + deltaRGB.g - deltaRGB.r, float(color.b == fmax));
    hsl.x = mix( hsl.x, ONETHIRD + deltaRGB.r - deltaRGB.b, float(color.g == fmax));
    hsl.x = mix( hsl.x, deltaRGB.b - deltaRGB.g,  float(color.r == fmax));

    hsl.x += mix( - float( hsl.x > 1.0 ), 1.0, float(hsl.x < 0.0) );

    hsl = mix ( hsl, vec3(-1.0, 0.0, hsl.z), float(delta<EPSILON) );

    return hsl;
}

float alphachromakey(vec3 color, vec3 colorKey, float delta)
{
   // magic values
   vec2 tol = vec2(0.5, 0.4*delta);

   //convert from RGB to YCvCr/YUV
   vec4 keyYuv = RGBtoYUV * vec4(colorKey, 1.0);
   vec4 yuv = RGBtoYUV * vec4(color, 1.0);

   // color distance in the UV (CbCr, PbPr) plane
   vec2 dif = yuv.yz - keyYuv.yz;
   float d = sqrt(dot(dif, dif));

   // tolerance clamping
   return max( (1.0 - step(d, tol.y)), step(tol.x, d) * (d - tol.x) / (tol.y - tol.x));
}


void main(void)
{
    // adjust UV
    texcoord = iTransform * vec4(vertexUV.x, vertexUV.y, 0.0, 1.0);

    // deal with alpha separately
    float ma = texture(iChannel0, texcoord.xy).a;
//    float ma = texture(maskTexture, texcoord.xy).a * texture(iChannel0, texcoord.xy).a;
    float alpha = clamp(ma * color.a, 0.0, 1.0);

    // read color & apply basic filterid
    vec3 transformedRGB;
    transformedRGB = apply_filter();

    // chromakey
    alpha -= mix( 0.0, 1.0 - alphachromakey( transformedRGB, chromakey.rgb, chromadelta), float(chromadelta > 0.0001) );

    // brightness and contrast transformation
    transformedRGB = mix(vec3(0.62), transformedRGB, contrast + 1.0) + brightness;

    // RGB invert
    transformedRGB = vec3(float(invert==1)) + ( transformedRGB * vec3(1.0 - 2.0 * float(invert==1)) );

    // Convert to HSL
    vec3 transformedHSL = RGB2HSV( transformedRGB );

    // Luminance invert
    transformedHSL.z = float(invert==2) +  transformedHSL.z * (1.0 - 2.0 * float(invert==2) );

    // perform hue shift
    transformedHSL.x = transformedHSL.x + hueshift;

    // Saturation
    transformedHSL.y *= saturation + 1.0;

    // perform reduction of colors
    transformedHSL = mix( transformedHSL, floor(transformedHSL * vec3(nbColors)) / vec3(nbColors-1),  float( nbColors > 0 ) );

    // luma key
    alpha -= mix( 0.0, step( transformedHSL.z, lumakey ), float(lumakey > EPSILON));

    // level threshold
    transformedHSL = mix( transformedHSL, vec3(0.0, 0.0, 0.95 - step( transformedHSL.z, threshold )), float(threshold > EPSILON));

    // after operations on HSL, convert back to RGB
    transformedRGB = HSV2RGB(transformedHSL);

    // apply gamma correction
    transformedRGB = LevelsControl(transformedRGB, levels.x, gamma.rgb * gamma.a, levels.y, levels.z, levels.w);

    // apply base color and alpha for final fragment color
    FragColor = vec4(transformedRGB * vertexColor.rgb * color.rgb * alpha, clamp(alpha, 0.0, 1.0) );

}


