// From https://www.shadertoy.com/view/XdfcWN
// Created by inigo quilez - iq/2014
// License Creative Commons Attribution-NonCommercial-ShareAlike 3.0 Unported License.
// Adapted by Bruno Herbelin for vimix

float Factor = 0.5;

vec2 hash( vec2 p ) {
    p = 2.0 * vec2(dot(p,vec2(127.1,311.7)), dot(p,vec2(269.5,183.3)));
    return -1.0 + 2.0*fract(sin(p)*43758.5453123);
}

void mainImage( out vec4 fragColor, in vec2 fragCoord )
{
    vec2 uv = fragCoord.xy / iResolution.xy;

    // sample zbuffer (in linear eye space) at the current shading point
    float zr = 1.0-texture( iChannel0, fragCoord.xy / iResolution.xy ).x;

    // sample neighbor pixels
    float ao = 0.0;
    for( int i=0; i<8; i++ )
    {
        // get a random 2D offset vector
        vec2 off = -1.0 + 2.0 * hash( fragCoord.xy) *float(i);
        // sample the zbuffer at a neightbor pixel (in a 16 pixel radious)
        float z = 1.0-texture( iChannel0, (fragCoord.xy + floor(off*16.0))/iResolution.xy ).x;
        // accumulate occlusion if difference is less than 0.1 units
        ao += clamp( (zr-z)/0.1, 0.0, 1.0);
    }
    // average down the occlusion
    ao = clamp( 1.0 - ao/8.0, 0.0, 1.0 );

    vec3 col = vec3(ao);
    vec3 og = texture(iChannel0, uv).xyz;

    vec3 one = vec3(1.);
    vec3 two = vec3(2.);
    vec3 point5 = vec3(0.5);
    col = col.x > 0.5 ? one - (one - og) * (one - two * (col - point5)) : col * two * og;

    // hard light
    col.r = col.r > 0.5 ? 1. - (1. - og.r) * (1. - 2. * (col.r - 0.5)) : col.r * 2. * og.r;
    col.g = col.g > 0.5 ? 1. - (1. - og.g) * (1. - 2. * (col.g - 0.5)) : col.g * 2. * og.g;
    col.b = col.b > 0.5 ? 1. - (1. - og.b) * (1. - 2. * (col.b - 0.5)) : col.b * 2. * og.b;

    fragColor = vec4( mix(texture(iChannel0, uv).xyz, col, Factor), 1.0);
}
