vec3 texSample(const int x, const int y, in vec2 fragCoord)
{
    vec2 uv = fragCoord.xy / iResolution.xy * iChannelResolution[0].xy;
    uv = (uv + vec2(x, y)) / iChannelResolution[0].xy;
    return texture(iChannel0, uv).xyz;
}    

void sharpen( out vec3 rgb, in vec2 fragCoord )
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
    vec3 sharped;
    sharpen(sharped, fragCoord);

    fragColor = vec4( sharped, 1.0);
}


