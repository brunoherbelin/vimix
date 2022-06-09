// from https://www.shadertoy.com/view/Ms2Xz3
float Intensity = 0.6;

vec4 BlurColor (in vec2 Coord, in sampler2D Tex, in float MipBias)
{
    vec2 TexelSize = MipBias/iChannelResolution[0].xy;
    vec4  Color = texture(Tex, Coord);
    Color += texture(Tex, Coord + vec2(TexelSize.x,0.0));
    Color += texture(Tex, Coord + vec2(-TexelSize.x,0.0));
    Color += texture(Tex, Coord + vec2(0.0,TexelSize.y));
    Color += texture(Tex, Coord + vec2(0.0,-TexelSize.y));
    Color += texture(Tex, Coord + vec2(TexelSize.x,TexelSize.y));
    Color += texture(Tex, Coord + vec2(-TexelSize.x,TexelSize.y));
    Color += texture(Tex, Coord + vec2(TexelSize.x,-TexelSize.y));
    Color += texture(Tex, Coord + vec2(-TexelSize.x,-TexelSize.y));

    return Color/9.0;
}

void mainImage( out vec4 fragColor, in vec2 fragCoord )
{
    float factor = 0.1 + Intensity * 3.0;
    float threshold = 0.8 - (0.6 * Intensity);

    vec2 uv = fragCoord.xy / iResolution.xy;
    vec4 Color = texture(iChannel0, uv);
    vec4 blurcolor = 0.6 * BlurColor(uv, iChannel0, 1.0) + 0.4 * BlurColor(uv, iChannel0, 3.0);
    vec4 Highlight = clamp(blurcolor - threshold, 0.0,1.0) * 1.0/(1.0-threshold);

    fragColor = 1.0 - (1.0-Color) * (1.0 - Highlight * factor);
}
