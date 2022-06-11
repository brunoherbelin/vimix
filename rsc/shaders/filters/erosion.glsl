/*
 * This file is part of vimix - video live mixer
 * https://github.com/brunoherbelin/vimix
 * (C) 2019-2022 Bruno Herbelin <bruno.herbelin@gmail.com>
 * Distributed under GNU GPL3+ License
**/
uniform float Radius;
#define MAX_SIZE 5

vec4 erosion (sampler2D source, vec2 uv, vec2 uv_step, float rad)
{
    vec4 minValue = vec4(1.0);
    float R = length(vec2(rad)) ;
    float D = rad;

    for (float i=-rad; i <= rad; ++i)
    {
        for (float j=-rad; j <= rad; ++j)
        {
            vec2 delta = vec2(i, j);
            minValue = min(texture(source, uv + delta * smoothstep(R, D, length(delta)) * uv_step ), minValue);
        }
    }

    return minValue;
}

void mainImage( out vec4 fragColor, in vec2 fragCoord )
{
    vec2 uv = fragCoord.xy / iResolution.xy;
    fragColor = erosion(iChannel0, uv, 1.0 / iResolution.xy, mix(1., MAX_SIZE, Radius));
}
