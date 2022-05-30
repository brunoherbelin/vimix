#define TWOPI 6.28318530718

uniform float Radius;
uniform float Iterations;

float hash(vec2 co){
    return fract(sin(dot(co.xy, vec2(12.9898,78.233))) * 43758.5453);
}

void mainImage( out vec4 fragColor, in vec2 fragCoord )
{
   vec2 uv = fragCoord.xy / iResolution.xy;
   float R = 0.25 * Radius * iResolution.y / iResolution.x ;

   vec4 O = vec4(0.);
   float N = ( Iterations * 39.0 + 10.0 ); // between 10 and 50
   for (float i = 0.; i < N; i++) {
       vec2 q = vec2(cos(TWOPI*i/N), sin(TWOPI*i/N)) * hash(vec2(i, uv.x + uv.y));
       O += pow( texture(iChannel0, uv + q*R), vec4(2.2) );
       q = vec2(cos(TWOPI*((i+.5)/N)), sin(TWOPI*(i+.5)/N)) * hash(vec2(i + 2., uv.x * uv.y + 24.));
       O += pow( texture(iChannel0, uv + q*R), vec4(2.2) );
   }
   fragColor = pow( O/(2.*N), vec4(1.0/2.2) );
}
