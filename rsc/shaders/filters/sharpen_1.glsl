uniform float Amount;

float SCurve (float x) {
    x = x * 2.0 - 1.0;
    return -x * abs(x) * 0.5 + x + 0.5;
}

vec3 BlurV (sampler2D source, vec2 uv, float step, float radius) {
    vec3 C = vec3(0.0); 
    float divisor = 0.0; 
    float weight = 0.0;
    float radiusMultiplier = 1.0 / max(1.0, radius);

    // loop on pixels in Y to apply vertical blur
    for (float y = -radius; y <= radius; y++) {
        weight = SCurve(1.0 - (abs(y) * radiusMultiplier)); 
        C += texture(source, uv + vec2(0.0, y * step)).rgb * weight;
        divisor += weight; 
    }

    return C / divisor;
}

void mainImage( out vec4 fragColor, in vec2 fragCoord )
{
    vec2 uv = fragCoord.xy / iResolution.xy;
    
    // Apply vertical blur
    fragColor = vec4( BlurV(iChannel0, uv, 1.0 / iResolution.y, mix(1.0, 0.1*iResolution.y, Amount)), 1.0);
}
