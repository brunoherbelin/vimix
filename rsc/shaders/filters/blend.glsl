// Blending of source texture (channel 0) with output window loopback (channel 1)
// by Bruno Herbelin for vimix 
uniform float Blend;

void mainImage( out vec4 fragColor, in vec2 fragCoord )
{
    vec2 uv = fragCoord.xy / iResolution.xy;
    fragColor = mix( texture(iChannel0, uv),  texture(iChannel1, uv), Blend);
}



