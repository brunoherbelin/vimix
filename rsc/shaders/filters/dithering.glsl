// Ordered Dithering Shader https://www.shadertoy.com/view/4lSSRR
// by mooglemoogle
// Adapted by Bruno Herbelin for vimix
uniform float Factor = 0.5;

const float mult = 1.0 / 17.0;
const mat4 adjustments = (mat4(
    1, 13, 4, 16,
    9, 5, 12, 8,
    3, 15, 2, 14,
    11, 7, 10, 6
) - 8.) * mult;

float getLuminance( vec4 color ) {
    return (0.2126*color.r + 0.7152*color.g + 0.0722*color.b);
}

float adjustFrag( float val, vec2 coord ) {
    vec2 coordMod = mod(coord, 4.0);
    int xMod = int(coordMod.x);
    int yMod = int(coordMod.y);

    vec4 col;
    if (xMod == 0) col = adjustments[0];
    else if (xMod == 1) col = adjustments[1];
        else if (xMod == 2) col = adjustments[2];
        else if (xMod == 3) col = adjustments[3];

    float adjustment;
    if (yMod == 0) adjustment = col.x;
    else if (yMod == 1) adjustment = col.y;
    else if (yMod == 2) adjustment = col.z;
    else if (yMod == 3) adjustment = col.w;

    return val + (val * adjustment);
}

void mainImage( out vec4 fragColor, in vec2 fragCoord )
{
    vec2 uv = fragCoord.xy / iResolution.xy;
    vec4 videoColor = texture(iChannel0, uv);

    float vidLum = getLuminance(videoColor);
    vidLum = adjustFrag(vidLum, fragCoord.xy);

    fragColor = step( Factor - vidLum ,  vec4(0, 0, 0, 1) );
}
