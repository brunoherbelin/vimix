#define RADIUS 0.2

uniform float Amount;

float SCurve (float x) {
    x = x * 2.0 - 1.0;
    return -x * abs(x) * 0.5 + x + 0.5;
}

vec3 BlurH (sampler2D source, vec2 uv, float step, float radius) {
    vec3 C = vec3(0.0); 
    float divisor = 0.0; 
    float weight = 0.0;
    float radiusMultiplier = 1.0 / max(1.0, radius);

    // loop on pixels in X to apply horizontal blur: note optimization of +2 increment
    for (float x = -radius; x <= radius; x+=2.0) {
        weight = SCurve(1.0 - (abs(x) * radiusMultiplier)); 
        C += texture(source, uv + vec2(x * step, 0.0)).rgb * weight;
        divisor += weight; 
    }

    return C / divisor;
}

void mainImage( out vec4 fragColor, in vec2 fragCoord )
{
    // upsample the image from blur_1
    float upsampling = 1.0 - ( Amount * 0.9 );
    vec2 uv = (fragCoord.xy * upsampling) / iResolution.xy;
    
    // Apply horizontal blur on restored resolution image
    fragColor = vec4( BlurH(iChannel0, uv, upsampling / iResolution.x, RADIUS * iResolution.y * Amount), 1.0);
}
