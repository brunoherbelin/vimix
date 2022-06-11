/*
 * This file is part of vimix - video live mixer
 * https://github.com/brunoherbelin/vimix
 * (C) 2019-2022 Bruno Herbelin <bruno.herbelin@gmail.com>
 * Distributed under GNU GPL3+ License
**/
uniform float  Factor;

const mat3 G[2] = mat3[](
	mat3( 1.0, 2.0, 1.0, 0.0, 0.0, 0.0, -1.0, -2.0, -1.0 ),
	mat3( 1.0, 0.0, -1.0, 2.0, 0.0, -2.0, 1.0, 0.0, -1.0 )
);

void mainImage( out vec4 fragColor, in vec2 fragCoord )
{
    mat3 I;
    float cnv[2];
    vec4 s;
    for (int i=0; i<3; i++)
    for (int j=0; j<3; j++) {
        s = texture(iChannel0, (fragCoord + vec2(i-1,j-1)) / iResolution.xy  );
        I[i][j] = length(s) * mix(1.0, 3.0, Factor);
    }
    for (int i=0; i<2; i++) {
        float dp3 = dot(G[i][0], I[0]) + dot(G[i][1], I[1]) + dot(G[i][2], I[2]);
        cnv[i] = dp3 * dp3;
    }

    fragColor = vec4( vec3( sqrt(cnv[0]*cnv[0]+cnv[1]*cnv[1]) ), 1.0);
}

