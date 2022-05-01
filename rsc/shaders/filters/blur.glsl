
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

void mainImage( out vec4 fragColor, in vec2 fragCoord )
{
    vec3 b;
    blur(b, fragCoord);
    fragColor = vec4( b, 1.0);
}


