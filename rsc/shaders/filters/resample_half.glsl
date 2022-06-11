/*
 * This file is part of vimix - video live mixer
 * https://github.com/brunoherbelin/vimix
 * (C) 2019-2022 Bruno Herbelin <bruno.herbelin@gmail.com>
 * Distributed under GNU GPL3+ License
**/
// inspired by https://www.shadertoy.com/view/fsjBWm (License: MIT)

void mainImage( out vec4 fragColor, in vec2 fragCoord )
{
    vec4 col = vec4(0.0);
    vec2 uv = fragCoord.xy / iResolution.xy;

    // optimized lowpass 2X downsampling filter.
    col += 0.37487566 * texture(iChannel0, uv + vec2(-0.75777156,-0.75777156)/iChannelResolution[0].xy);
    col += 0.37487566 * texture(iChannel0, uv + vec2(0.75777156,-0.75777156)/iChannelResolution[0].xy);
    col += 0.37487566 * texture(iChannel0, uv + vec2(0.75777156,0.75777156)/iChannelResolution[0].xy);
    col += 0.37487566 * texture(iChannel0, uv + vec2(-0.75777156,0.75777156)/iChannelResolution[0].xy);
    col += -0.12487566 * texture(iChannel0, uv + vec2(-2.90709914,0.0)/iChannelResolution[0].xy);
    col += -0.12487566 * texture(iChannel0, uv + vec2(2.90709914,0.0)/iChannelResolution[0].xy);
    col += -0.12487566 * texture(iChannel0, uv + vec2(0.0,-2.90709914)/iChannelResolution[0].xy);
    col += -0.12487566 * texture(iChannel0, uv + vec2(0.0,2.90709914)/iChannelResolution[0].xy);

//    col = texture( iChannel0, uv );
    fragColor = col;
}
