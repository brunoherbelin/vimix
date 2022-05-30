// 16x acceleration of https://www.shadertoy.com/view/4tSyzy
// by applying gaussian at intermediate MIPmap level.

const int samples = 25,
          LOD = 2,         // gaussian done on MIPmap at scale LOD
          sLOD = 1 << LOD; // tile size = 2^LOD
const float sigma = float(samples) * .25;

float gaussian(vec2 i) {
    return exp( -.5* dot(i/=sigma,i) ) / ( 6.28 * sigma*sigma );
}

vec4 blur(sampler2D sp, vec2 U, vec2 scale) {
    vec4 O = vec4(0);
    int s = samples/sLOD;
    float t = 0., g;

    for ( int i = 0; i < s*s; i++ ) {
        vec2 d = vec2(i%s, i/s)*float(sLOD) - float(samples)/2.;
        t += g = gaussian(d);
        O += g * textureLod( sp, U + scale * d , float(LOD) );
    }

    return O / t;
}

void mainImage(out vec4 O, vec2 U) {
    O = blur( iChannel0, U/iResolution.xy, 1./iChannelResolution[0].xy );
}
