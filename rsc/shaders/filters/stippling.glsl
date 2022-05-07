uniform float Factor;

#define SEED1 -0.5775604999999985
#define SEED2 6.440483302499992

vec2 stepnoise(vec2 p, float size) {
    p += 10.0;
    float x = floor(p.x/size)*size;
    float y = floor(p.y/size)*size;

    x = fract(x*0.1) + 1.0 + x*0.0002;
    y = fract(y*0.1) + 1.0 + y*0.0003;

    float a = fract(1.0 / (0.000001*x*y + 0.00001));
    a = fract(1.0 / (0.000001234*a + 0.00001));

    float b = fract(1.0 / (0.000002*(x*y+x) + 0.00001));
    b = fract(1.0 / (0.0000235*b + 0.00001));

    return vec2(a, b);
}

float tent(float f) {
    return 1.0 - abs(fract(f)-0.5)*2.0;
}

float mask(vec2 p) {
    vec2 r = stepnoise(p, 8.423424);
    p[0] += r[0];
    p[1] += r[1];

    float f1 = tent(p[0]*SEED1 + p[1]/(SEED1+0.5));
    float f2 = tent(p[1]*SEED2 + p[0]/(SEED2+0.5));
    float f = sqrt( f1*f2 );

    return f;
}

float saturation(vec3 color) {
    return (min(color.r, min(color.g, color.b)) + max(color.r, max(color.g, color.b))) * 0.5;
}

void mainImage( out vec4 fragColor, in vec2 fragCoord )
{
    vec2 uv = fragCoord.xy;
    vec2 uv2 = fragCoord.xy / iResolution.x;

    float f = saturation( texture(iChannel0, fragCoord.xy / iResolution.xy).rgb );
    float c = mask(uv);
    c = float( f > mix( 0.6 * c, 1.4 * c, Factor) );

    fragColor = vec4(c, c, c, 1.0);
}
