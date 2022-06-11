/*
 * This file is part of vimix - video live mixer
 * https://github.com/brunoherbelin/vimix
 * (C) 2019-2022 Bruno Herbelin <bruno.herbelin@gmail.com>
 * Distributed under GNU GPL3+ License
**/
uniform float Red;
uniform float Green;
uniform float Blue;

void mainImage( out vec4 fragColor, in vec2 fragCoord )
{
    vec2 fragPos =  fragCoord.xy / iResolution.xy;
    vec4 color = texture(iChannel0, fragPos);
    fragColor = vec4( mix(vec3(Red, Green, Blue), color.rgb, color.a), 1.f);
}
