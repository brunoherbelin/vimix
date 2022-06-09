// from https://www.shadertoy.com/view/4d2Xzw
// Bokeh disc.
// by David Hoskins.
// Creative Commons Attribution-NonCommercial-ShareAlike 3.0 Unported License.
// The Golden Angle is (3.-sqrt(5.0))*PI radians
#define GOLDEN_ANGLE 2.39996323
#define ITERATIONS 60

float Radius = 0.3;

mat2 rot = mat2(cos(GOLDEN_ANGLE), sin(GOLDEN_ANGLE), -sin(GOLDEN_ANGLE), cos(GOLDEN_ANGLE));

vec3 Bokeh(sampler2D tex, vec2 uv, float radius, float amount)
{
    vec3 acc = vec3(0.0);
    vec3 div = vec3(0.0);
    vec2 pixel = 1.0 / iResolution.xy;
    float r = 1.0;
    vec2 vangle = vec2(0.0,radius); // Start angle
    amount += radius*100.0;

    for (int j = 0; j < ITERATIONS; j++)
    {
        r += 1. / r;
        vangle = rot * vangle;
        // (r-1.0) here is the equivalent to sqrt(0, 1, 2, 3...)
        vec3 col = texture(tex, uv + pixel * (r-1.) * vangle).xyz;
        col = col * col * 1.6; // ...contrast it for better highlights
        vec3 bokeh = pow(col, vec3(9.0)) * amount+.4;
        acc += col * bokeh;
        div += bokeh;
    }
    return acc / div;
}

void mainImage( out vec4 fragColor, in vec2 fragCoord )
{
    // Parameters
    float a = 40.0;
    vec2 uv = fragCoord.xy / iResolution.xy;
    fragColor = vec4(Bokeh(iChannel0, uv, Radius, a), 1.0);
}
