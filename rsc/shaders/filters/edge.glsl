/*
 * This file is part of vimix - video live mixer
 * https://github.com/brunoherbelin/vimix
 * (C) 2019-2022 Bruno Herbelin <bruno.herbelin@gmail.com>
 * Distributed under GNU GPL3+ License
**/

uniform float Threshold;

void mainImage( out vec4 fragColor, in vec2 fragCoord )
{
    vec4 c[16];
    for (int i=0; i < 4; ++i)
        for (int j=0; j < 4; ++j)
        {
            c[4*i+j] = texture(iChannel0, (fragCoord.xy+vec2(i-2,j-2)) / iResolution.xy);
        }

    // pseudo Scharr masks 4 x 4
    vec4 Lx = 10.0*(c[14]-c[2] + c[13]-c[1]) + 3.0*(c[15]-c[3] + c[12]-c[0]);
    Lx += 2.0*(c[10]-c[6] + c[9]-c[5]) + (c[11]-c[7] + c[8]-c[4]);

    vec4 Ly = 10.0*(c[8]-c[11]+ c[4]-c[7]) + 3.0*(c[12]-c[15] + c[0]-c[3]);
    Ly += 2.0*(c[9]-c[10] + c[5]-c[6]) + (c[13]-c[14] + c[1]-c[2]);

    vec4 G = sqrt(Lx*Lx+Ly*Ly);
    //vec3 G = abs(Lx)+abs(Ly);

    fragColor = vec4( vec3( step( mix( 0.5, 10.0, Threshold), length(G)) ), 1.0);
}
