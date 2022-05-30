#define N 13

uniform float Radius;

vec4 blur2D(vec2 U, float rad)
{
    float w = rad * iResolution.y;
    float z = ceil(max(0.,log2(w/float(N))));
    vec4  O = vec4(0);
    float r = float(N-1)/2., g, t=0.;
    for( int k=0; k<N*N; k++ ) {
        vec2 P = vec2(k%N, k/N) / r - 1.;
        if ( dot(P,P) < 1.0 ) {
            t += g = exp(-2.*dot(P,P) );
            O += g * textureLod(iChannel0, (U + w*P) / iResolution.xy, z );
        }
    }
    return O/t;
}

void mainImage( out vec4 fragColor, in vec2 fragCoord )
{
    fragColor = blur2D( fragCoord, Radius * 0.25 );
}
