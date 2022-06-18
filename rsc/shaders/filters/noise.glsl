// From https://www.shadertoy.com/view/4t2fRz
// By luluco250. Feel free to steal this :^)
// Consider it MIT licensed, you can link to this page if you want to.
// Adapted by Bruno Herbelin for vimix

#define SRGB 1
// 0: Addition, 1: Screen, 2: Overlay, 3: Soft Light, 4: Lighten-Only
#define BLEND_MODE 2
#define SPEED 1.5
// What gray level noise should tend to.
#define MEAN 0.0
// Controls the contrast/variance of noise.
#define VARIANCE 0.5

uniform float Amount;

vec4 channel_mix(vec4 a, vec4 b, vec4 w) {
    return vec4(mix(a.r, b.r, w.r), mix(a.g, b.g, w.g), mix(a.b, b.b, w.b), mix(a.a, b.a, w.a));
}

float hash11(float p)
{
    p = fract(p * .1031);
    p *= p + 33.33;
    p *= p + p;
    return fract(p);
}
float hash12(vec2 p)
{
    vec3 p3  = fract(vec3(p.xyx) * .1031);
    p3 += dot(p3, p3.yzx + 33.33);
    return fract((p3.x + p3.y) * p3.z);
}

float gaussian(float z, float u, float o) {
    return (1.0 / (o * sqrt(2.0 * 3.1415))) * exp(-(((z - u) * (z - u)) / (2.0 * (o * o))));
}

vec4 madd(vec4 a, vec4 b, float w) {
    return a + a * b * w;
}

vec4 screen(vec4 a, vec4 b, float w) {
    return mix(a, vec4(1.0) - (vec4(1.0) - a) * (vec4(1.0) - b), w);
}

vec4 overlay(vec4 a, vec4 b, float w) {
    return mix(a, channel_mix(
        2.0 * a * b,
        vec4(1.0) - 2.0 * (vec4(1.0) - a) * (vec4(1.0) - b),
        step(vec4(0.5), a)
    ), w);
}

vec4 soft_light(vec4 a, vec4 b, float w) {
    return mix(a, pow(a, pow(vec4(2.0), 2.0 * (vec4(0.5) - b))), w);
}

void mainImage(out vec4 color, in vec2 coord)
{
    vec2 uv = coord / iResolution.xy;
    color = texture(iChannel0, uv);
    #if SRGB
    color = pow(color, vec4(2.2));
    #endif

    float t = iTime * float(SPEED);
    float seed = dot(uv, vec2(12.9898, 78.233));
    float noise = fract(sin(seed) * 43758.5453 + t + hash12(coord));

    noise = gaussian(noise, float(MEAN), float(VARIANCE) * float(VARIANCE));

    float w = mix(0.05, 0.8, Amount );
    vec4 grain = vec4(noise) * (1.0 - color);

    #if BLEND_MODE == 0
    color += grain * w;
    #elif BLEND_MODE == 1
    color = screen(color, grain, w);
    #elif BLEND_MODE == 2
    color = overlay(color, grain, w);
    #elif BLEND_MODE == 3
    color = soft_light(color, grain, w);
    #elif BLEND_MODE == 4
    color = max(color, grain * w);
    #endif

    #if SRGB
    color = pow(color, vec4(1.0 / 2.2));
    #endif
}
