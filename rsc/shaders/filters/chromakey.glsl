/*
chroma key algorithm used to remove the greens screen
this technique is nice as you can choose a color removal range.
using this range, the algorithm will either remove the given color
or blend it with background

because of this, difficult parts (ie. the sword) can either have a
green-ish look or are a bit transparent (hard to catch with a lot of movement)

(c) tudordot from https://www.shadertoy.com/view/MlVXWD
*/

uniform float Red;
uniform float Green;
uniform float Blue;
uniform float Tolerance;

//conversion between rgb and YUV
mat4 RGBtoYUV = mat4(0.257,  0.439, -0.148, 0.0,
                     0.504, -0.368, -0.291, 0.0,
                     0.098, -0.071,  0.439, 0.0,
                     0.0625, 0.500,  0.500, 1.0 );

//color to be removed
vec4 chromaKey = vec4(Red, Green, Blue, 1);

//range is used to decide the amount of color to be used from either foreground or background
//if the current distance from pixel color to chromaKey is smaller then maskRange.x we use background,
//if the current distance from pixel color to chromaKey is bigger then maskRange.y we use foreground,
//else, we blend them
//playing with this variable will decide how much the foreground and background blend together
vec2 maskRange = vec2(0.01, Tolerance * 0.5);

//compute color distance in the UV (CbCr, PbPr) plane
float colorclose(vec3 yuv, vec3 keyYuv, vec2 tol)
{
    float tmp = sqrt(pow(keyYuv.g - yuv.g, 2.0) + pow(keyYuv.b - yuv.b, 2.0));
    if (tmp < tol.x)
        return 0.0;
    else if (tmp < tol.y)
        return (tmp - tol.x)/(tol.y - tol.x);
    else
        return 1.0;
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



void mainImage( out vec4 fragColor, in vec2 fragCoord )
{
    vec4 texColor0 = texture(iChannel0, fragCoord.xy / iResolution.xy);

    //convert from RGB to YCvCr/YUV
//    vec4 keyYUV = RGBtoYUV * chromaKey;
//    vec4 yuv = RGBtoYUV * texColor0;
//    float mask = 1.0 - colorclose(yuv.rgb, keyYUV.rgb, maskRange);
//    fragColor = max(texColor0 - mask * chromaKey, 0.0);

    float A = alphachromakey(texColor0.rgb, chromaKey.rgb, Tolerance);

    fragColor = vec4( texColor0.rgb, A );
}
