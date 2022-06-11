/*
 * This file is part of vimix - video live mixer
 * https://github.com/brunoherbelin/vimix
 * (C) 2019-2022 Bruno Herbelin <bruno.herbelin@gmail.com>
 * Distributed under GNU GPL3+ License
**/
uniform float Amount;

vec3 blurSample(in vec2 uv, in vec2 xoff, in vec2 yoff)
{
    vec3 v11 = texture(iChannel0, uv + xoff).rgb;
    vec3 v12 = texture(iChannel0, uv + yoff).rgb;
    vec3 v21 = texture(iChannel0, uv - xoff).rgb;
    vec3 v22 = texture(iChannel0, uv - yoff).rgb;
    return (v11 + v12 + v21 + v22 + 2.0 * texture(iChannel0, uv).rgb) * 0.166667;
}

vec3 edgeStrength(in vec2 uv)
{
    const float spread = 0.5;
    vec2 offset = vec2(1.0) / iChannelResolution[0].xy;
    vec2 up    = vec2(0.0, offset.y) * spread;
    vec2 right = vec2(offset.x, 0.0) * spread;
    const float frad =  3.0;
    vec3 v11 = blurSample(uv + up - right, 	right, up);
    vec3 v12 = blurSample(uv + up, 			right, up);
    vec3 v13 = blurSample(uv + up + right, 	right, up);
    
    vec3 v21 = blurSample(uv - right, 		right, up);
    vec3 v22 = blurSample(uv, 				right, up);
    vec3 v23 = blurSample(uv + right, 		right, up);
    
    vec3 v31 = blurSample(uv - up - right, 	right, up);
    vec3 v32 = blurSample(uv - up, 			right, up);
    vec3 v33 = blurSample(uv - up + right, 	right, up);
    
    vec3 laplacian_of_g = v11 * 0.0 + v12 *  1.0 + v13 * 0.0
            + v21 * 1.0 + v22 * -4.0 + v23 * 1.0
            + v31 * 0.0 + v32 *  1.0 + v33 * 0.0;
    laplacian_of_g = laplacian_of_g * 1.0;
    return laplacian_of_g.xyz;
}

void mainImage( out vec4 fragColor, in vec2 fragCoord )
{
    vec2 uv = fragCoord.xy / iResolution.xy;
    fragColor = vec4(texture(iChannel0, uv).xyz - edgeStrength(uv) * Amount * (iResolution.y*0.05), 1.0);
}
