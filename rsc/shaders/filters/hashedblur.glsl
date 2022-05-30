// Hashed blur
// David Hoskins.
// License Creative Commons Attribution-NonCommercial-ShareAlike 3.0 Unported License.
// https://www.shadertoy.com/view/XdjSRw

// Set this to iTime to add pixel noise.
#define TIME 0.0

uniform float Radius;
uniform float Iterations;

// hash function generate random radius and angle...
#define TAU  6.28318530718
vec2 Sample(inout vec2 r)
{
    r = fract(r * vec2(33.3983, 43.4427));
    return sqrt(r.x+.001) * vec2(sin(r.y * TAU), cos(r.y * TAU))*.5; // circular sampling.
}

#define HASHSCALE 443.8975
vec2 Hash22(vec2 p)
{
    vec3 p3 = fract(vec3(p.xyx) * HASHSCALE);
    p3 += dot(p3, p3.yzx+19.19);
    return fract(vec2((p3.x + p3.y)*p3.z, (p3.x+p3.z)*p3.y));
}

vec4 Blur(vec2 uv, float radius)
{
    radius = radius * .04;
    vec2 circle = vec2(radius) * vec2((iResolution.y / iResolution.x), 1.0);
    vec2 random = Hash22( uv + TIME );

    // Do the blur here...
    vec4 acc = vec4(0.0);
    int max = int( Iterations * 29.0 + 1.0 ); // between 1 and 30
    for (int i = 0; i < max; i++)
    {
        acc += textureLod(iChannel0, uv + circle * Sample(random), radius*10.0);
    }
    return acc / float(max);
}

void mainImage( out vec4 fragColor, in vec2 fragCoord )
{
    vec2 uv = fragCoord.xy / iResolution.xy;
    
    float radius = Radius * 0.01 * iResolution.y;
    radius = pow(radius, 2.0);
 
    fragColor = Blur(uv * vec2(1.0, -1.0), radius);
}
