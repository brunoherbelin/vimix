/*
 * This file is part of vimix - video live mixer
 * https://github.com/brunoherbelin/vimix
 * (C) 2019-2022 Bruno Herbelin <bruno.herbelin@gmail.com>
 * Distributed under GNU GPL3+ License
**/
uniform float Amount;

#define N 7
vec4 blur1D(vec2 U, vec2 D, float rad)
{
    float w = rad * iResolution.y;
    float z = ceil(max(0.,log2(w/float(N)))); // LOD  N/w = res/2^z
    vec4  O = vec4(0);
    float r = float(N-1)/2., g, t=0., x;
    for( int k=0; k<N; k++ ) {
        x = float(k)/r -1.;
        t += g = exp(-2.*x*x );
        O += g * texture(iChannel0, (U + w*x*D) / iResolution.xy );
    }
    return O/t;
}

void mainImage( out vec4 fragColor, in vec2 fragCoord )
{
    // get original image
    vec4 c = texture(iChannel1, fragCoord.xy / iResolution.xy);

    // Remove blurred image to original image
    c = blur1D(fragCoord, vec2(0,1), 0.1 * Amount ) - c;

    // convert to greyscale
    float luminance = dot(c, vec4(0.299, 0.587, 0.114, 1.));

    // composition
    fragColor =  vec4( vec3( smoothstep( 0.05, 0.2, luminance) ), 1.0);
}
