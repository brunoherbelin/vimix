uniform float Strength;

vec3 texSample(const int x, const int y, in vec2 fragCoord)
{
	vec2 uv = fragCoord.xy / iResolution.xy * iChannelResolution[0].xy;
	uv = (uv + vec2(x, y)) / iChannelResolution[0].xy;
	return texture(iChannel0, uv).xyz;       
}    

void blur( out vec3 rgb, in vec2 fragCoord )
{
    vec3 tot = vec3(0.0);
    
    for( int j=0; j<9; j++ )
        for( int i=0; i<9; i++ )
            tot += pow( texSample(i-4, j-4, fragCoord), vec3(2.2));
    
    rgb = pow(tot/81.0,vec3(1.0/2.2)); 
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
    vec3 original = texSample( 0, 0, fragCoord);
    // sharpen mask
    vec3 sharped;
    sharpen(sharped, fragCoord);
    // sharpen by subtracting blurred image to original
    vec3 blurred;
    blur(blurred, fragCoord);
    blurred  = original + (original-blurred);
    // progressive mix from original to most sharpen
    sharped = mix(sharped, blurred, Strength);
    fragColor = vec4( mix(original, sharped, Strength * 2.0), 1.0);
}


