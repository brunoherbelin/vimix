uniform float Size;
uniform float Edge;

const mat3 I = mat3( 0, 0, 0, 0, 1, 0, 0, 0, 0);
const mat3 G = mat3( 0.0625, 0.125, 0.0625, 0.125, 0.25, 0.125, 0.0625, 0.125, 0.0625);
        
void mainImage( out vec4 fragColor, in vec2 fragCoord ) {

    vec2 size = vec2(3.0 * iResolution.x / iResolution.y, 3.0 );
    size =  mix( size, iResolution.xy, pow(1.0 - Size, 3) ) ;
    vec2 vUv = size / iResolution.xy;

    vec4 blur = vec4(0,0,0,0);
    for (int i=0; i<3; i++)
    for (int j=0; j<3; j++) {
        blur +=  G[i][j] * texture(iChannel0, floor(vUv * (fragCoord + vec2(i-1,j-1))) / size );
    }

    vec4 sample = texture(iChannel0, floor(vUv * fragCoord) / size );
    sample += Edge * (sample - blur);
    
    fragColor = sample;
}

