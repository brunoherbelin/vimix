uniform float  Factor;

const mat3 G[2] = mat3[](
	mat3( 1.0, 2.0, 1.0, 0.0, 0.0, 0.0, -1.0, -2.0, -1.0 ),
	mat3( 1.0, 0.0, -1.0, 2.0, 0.0, -2.0, 1.0, 0.0, -1.0 )
);

void sobel( out vec3 rgb, in vec2 fragCoord )
{
    mat3 I;
    float cnv[2];
    vec3 sample;
    for (int i=0; i<3; i++)
    for (int j=0; j<3; j++) {
        sample = texture(iChannel0, (fragCoord + vec2(i-1,j-1)) / iResolution.xy  ).rgb;
        I[i][j] = length(sample) * mix(1.0, 3.0, Factor); 
    }
    for (int i=0; i<2; i++) {
	    float dp3 = dot(G[i][0], I[0]) + dot(G[i][1], I[1]) + dot(G[i][2], I[2]);
	    cnv[i] = dp3 * dp3; 
    }
    rgb = vec3( sqrt(cnv[0]*cnv[0]+cnv[1]*cnv[1]) );
    //rgb = vec3(  abs(cnv[0]) + abs(cnv[1]) );
}

void mainImage( out vec4 fragColor, in vec2 fragCoord )
{
    vec3 v;
    sobel(v, fragCoord);
    fragColor = vec4(v, 1.0);
}

