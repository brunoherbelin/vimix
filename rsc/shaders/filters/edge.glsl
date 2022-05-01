void mainImage( out vec4 fragColor, in vec2 fragCoord )
{
	vec3 c[16];
	for (int i=0; i < 4; ++i)
		for (int j=0; j < 4; ++j)
		{
			c[4*i+j] = texture(iChannel0, (fragCoord.xy+vec2(i-2,j-2)) / iResolution.xy).rgb;
		}
	
	vec3 Lx = 2.0*(c[14]-c[2]) + 2.0*(c[13]-c[1]) + c[15] - c[3] + c[12] - c[0];
	Lx += 0.5 * (2.0*(c[10]-c[6]) + 2.0*(c[9]-c[5]) + c[11] - c[7] + c[8] - c[4] );
	
	vec3 Ly = 2.0*(c[8]-c[11])  + 2.0*(c[4]-c[7]) + c[12] + c[0] - c[15] - c[3];
	Ly += 0.5 * (2.0*(c[9]-c[10])  + 2.0*(c[5]-c[6]) + c[13] + c[1] - c[14] - c[2] );
	
	//vec3 G = sqrt(Lx*Lx+Ly*Ly);
	vec3 G = abs(Lx)+abs(Ly);
	
	fragColor = vec4( mix(vec3(1.0), vec3(0.0), pow(length(G),2)  ), 1.0);
	
}

