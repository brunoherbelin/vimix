// Texture from external Source
// by Bruno Herbelin for vimix 
uniform sampler2D Source;

void mainImage( out vec4 fragColor, in vec2 fragCoord )
{
    vec2 uv = fragCoord.xy / iResolution.xy;
    fragColor = texture(Source, uv);
}



