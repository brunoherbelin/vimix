/*
 * This file is part of vimix - video live mixer
 * https://github.com/brunoherbelin/vimix
 * (C) 2019-2022 Bruno Herbelin <bruno.herbelin@gmail.com>
 * Distributed under GNU GPL3+ License
**/
uniform float Amount;

vec4 texSample(const int x, const int y, in vec2 fragCoord)
{
    vec2 uv = fragCoord.xy / iResolution.xy * iChannelResolution[0].xy;
    uv = (uv + vec2(x, y)) / iChannelResolution[0].xy;
    return texture(iChannel0, uv);
}    

void sharpen( out vec4 rgb, in vec2 fragCoord )
{
    rgb =
    texSample(-1,-1, fragCoord) *  -1. +                     
    texSample( 0,-1, fragCoord) *  -1. +                    
    texSample( 1,-1, fragCoord) *  -1. +                      
    texSample(-1, 0, fragCoord) *  -1. +                    
    texSample( 0, 0, fragCoord) *   9. +                     
    texSample( 1, 0, fragCoord) *  -1. +                      
    texSample(-1, 1, fragCoord) *  -1. +                     
    texSample( 0, 1, fragCoord) *  -1. +                     
    texSample( 1, 1, fragCoord) *  -1. ; 
}

void mainImage( out vec4 fragColor, in vec2 fragCoord )
{
    // sharpen mask
    vec4 sharped;
    sharpen(sharped, fragCoord);

    fragColor = mix(texSample( 0, 0, fragCoord), sharped , 1.5 * Amount);;
}


