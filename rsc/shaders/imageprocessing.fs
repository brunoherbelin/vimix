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
uniform float threshold;
uniform int   nbColors;
uniform int   invert;

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

void main(void)
{
    // adjust UV
    texcoord = iTransform * vec4(vertexUV.x, vertexUV.y, 0.0, 1.0);

    vec4 texcolor = texture(iChannel0, texcoord.xy);

    // deal with alpha separately
    float alpha = clamp(texcolor.a * color.a, 0.0, 1.0);

    // read color
    vec3 transformedRGB = texcolor.rgb;

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

    // level threshold
    transformedHSL = mix( transformedHSL, vec3(0.0, 0.0, 0.95 - step( transformedHSL.z, threshold )), float(threshold > EPSILON));

    // after operations on HSL, convert back to RGB
    transformedRGB = HSV2RGB(transformedHSL);

    // apply gamma correction
    transformedRGB = LevelsControl(transformedRGB, levels.x, gamma.rgb * gamma.a, levels.y, levels.z, levels.w);

    // apply base color and alpha for final fragment color
    FragColor = vec4(transformedRGB * vertexColor.rgb * color.rgb * alpha, alpha);

}


