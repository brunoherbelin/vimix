/** 
 * Earlybird Filter by Ruofei Du (DuRuofei.com)
 * Demo: https://www.shadertoy.com/view/XlSyWV
 * starea @ ShaderToy,License Creative Commons Attribution-NonCommercial-ShareAlike 3.0 Unported License.
 * https://creativecommons.org/licenses/by-nc-sa/3.0/
 * 
 * Reference: 
 * [1] colorBurn function forked from ben's https://www.shadertoy.com/view/XdS3RW
 * [2] starea's Dotted Drawing / Sketch Effect: https://www.shadertoy.com/view/ldSyzV
 * [3] starea's BrightnessContrastSaturationHue: https://www.shadertoy.com/view/MdjBRy
 *
 * Series:
 * [1] Brannan Filter: https://www.shadertoy.com/view/4lSyDK
 * [2] Earlybird Filter: https://www.shadertoy.com/view/XlSyWV
 * [3] Starea Filter: https://www.shadertoy.com/view/MtjyDK
 * 
 *
 * Write-ups:
 * [1] http://blog.ruofeidu.com/implementing-instagram-filters-brannan/
 **/
float greyScale(in vec3 col) 
{
    return dot(col, vec3(0.3, 0.59, 0.11));
}

mat3 saturationMatrix( float saturation ) {
    vec3 luminance = vec3( 0.3086, 0.6094, 0.0820 );
    float oneMinusSat = 1.0 - saturation;
    vec3 red = vec3( luminance.x * oneMinusSat );
    red.r += saturation;
    
    vec3 green = vec3( luminance.y * oneMinusSat );
    green.g += saturation;
    
    vec3 blue = vec3( luminance.z * oneMinusSat );
    blue.b += saturation;
    
    return mat3(red, green, blue);
}

void levels(inout vec3 col, in vec3 inleft, in vec3 inright, in vec3 outleft, in vec3 outright) {
    col = clamp(col, inleft, inright);
    col = (col - inleft) / (inright - inleft);
    col = outleft + col * (outright - outleft);
}

void brightnessAdjust( inout vec3 color, in float b) {
    color += b;
}

void contrastAdjust( inout vec3 color, in float c) {
    float t = 0.5 - c * 0.5; 
    color = color * c + t;
}


vec3 colorBurn(in vec3 s, in vec3 d )
{
	return 1.0 - (1.0 - d) / s;
}

uniform float Contrast;

void mainImage( out vec4 fragColor, in vec2 fragCoord )
{
	vec2 uv = fragCoord.xy / iResolution.xy;
    vec3 col = texture(iChannel0, uv).rgb; 
    vec2 coord = ( fragCoord.xy + fragCoord.xy -  iResolution.xy ) / iResolution.y;
    vec3 gradient = vec3(pow(1.0 - length(coord * 0.4), 0.6) * 1.2); 
    vec3 grey = vec3(184./255.);
    vec3 tint = vec3(252., 243., 213.) / 255.;
    col = saturationMatrix(0.68) * col; 
    levels(col, vec3(0.), vec3(1.0), 
                vec3(27.,0.,0.) / 255., vec3(255.) / 255.); 
    col = pow(col, vec3(1.19));
    brightnessAdjust(col, 0.13); 
    contrastAdjust(col, 1.05); 
    col = saturationMatrix(0.85) * col; 
    levels(col, vec3(0.), vec3(235./255.), 
                vec3(0.,0.,0.) / 255., vec3(255.) / 255.); 
    
    col = mix(tint * col, col, gradient); 
    col = colorBurn(grey, col); 
    col *= Contrast + 0.5;
    fragColor = vec4(col, 1.0);
}

