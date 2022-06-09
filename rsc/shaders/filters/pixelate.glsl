float Size = 0.5;
float Sharpen = 0.5;

const mat3 G = mat3( 0.0625, 0.125, 0.0625, 0.125, 0.25, 0.125, 0.0625, 0.125, 0.0625);
        
void mainImage( out vec4 fragColor, in vec2 fragCoord ) {

    vec2 size = vec2(3.0 * iResolution.x / iResolution.y, 3.0 );
    size =  mix( size, iResolution.xy, pow(1.0 - Size, 3.0) ) ;
    vec2 vUv = size / iResolution.xy;

    vec3 blur = vec3(0,0,0);
    for (int i=0; i<3; i++)
        for (int j=0; j<3; j++) {
            blur += G[i][j] * texture(iChannel0, round( (vUv * fragCoord) + vec2(i-1,j-1) ) / size ).rgb;
        }

    fragColor = vec4( mix( blur, texture(iChannel0, round(vUv * fragCoord) / size ).rgb, Sharpen), 1.0);
}

