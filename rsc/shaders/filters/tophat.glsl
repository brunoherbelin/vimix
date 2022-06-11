/*
 * This file is part of vimix - video live mixer
 * https://github.com/brunoherbelin/vimix
 * (C) 2019-2022 Bruno Herbelin <bruno.herbelin@gmail.com>
 * Distributed under GNU GPL3+ License
**/
uniform float Radius;
#define MAX_SIZE 5

vec3 dilation (sampler2D source, vec2 uv, vec2 uv_step, float R) {
    
    vec3 maxValue = vec3(0.0);
    float D = length(vec2( R / 2.));

    for (float i=-R; i <= R; ++i)
    {
        for (float j=-R; j <= R; ++j)
        {
            vec2 delta = vec2(i, j);
            maxValue = max(texture(source, uv + delta * smoothstep(R, D, length(delta)) * uv_step ).rgb, maxValue); 
        }
    }

    return maxValue;
}

void mainImage( out vec4 fragColor, in vec2 fragCoord )
{
    vec2 uv = fragCoord.xy / iResolution.xy;

    // tophat is the difference between input image and Opening of the input image
    // Opening is Erosion (first pass) followed by Dilation (second pass)

    // get original image
    vec3 c = texture(iChannel1, uv).rgb;
    
    // get result of Opening
    vec3 d = dilation(iChannel0, uv, 1.0 / iResolution.xy, mix(1., MAX_SIZE, Radius));

    // composition
    fragColor = vec4( c + (c-d), 1.0);
}
