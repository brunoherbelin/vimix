// Blending of source texture (channel 0) with texture of an external Source
// by Bruno Herbelin for vimix 
uniform float Blend;
uniform sampler2D Source;

void mainImage( out vec4 fragColor, in vec2 fragCoord )
{
    vec2 uv = fragCoord.xy / iResolution.xy;
    fragColor = mix( texture(iChannel0, uv),  texture(Source, uv), Blend);
}



