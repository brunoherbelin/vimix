// Gaussian blur with mipmapping
// Bruno Herbelin
// Following tutorial https://www.shadertoy.com/view/WtKfD3

#define N 13

uniform float Radius;

vec4 blur1D(vec2 U, vec2 D, float rad)
{       
    float w = rad * iResolution.y;
    float z = ceil(max(0.,log2(w/float(N)))); // LOD  N/w = res/2^z
    vec4  O = vec4(0);                                                      
    float r = float(N-1)/2., g, t=0., x;                                    
    for( int k=0; k<N; k++ ) {                                              
        x = float(k)/r -1.;                                                  
        t += g = exp(-2.*x*x );                                             
        O += g * textureLod(iChannel0, (U + w*x*D) / iResolution.xy, z );
    }                                                                       
    return O/t;                                                             
}

void mainImage( out vec4 fragColor, in vec2 fragCoord )
{
    fragColor = blur1D( fragCoord, vec2(1,0), Radius * 0.5 );
}
