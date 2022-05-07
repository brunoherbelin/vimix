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

    // loop on pixels in X to apply horizontal blur
    for (float x = -radius; x <= radius; x++) {
        weight = SCurve(1.0 - (abs(x) * radiusMultiplier)); 
        C += texture(source, uv + vec2(x * step, 0.0)).rgb * weight;
        divisor += weight; 
    }

    return C / divisor;
}

void mainImage( out vec4 fragColor, in vec2 fragCoord )
{
    vec2 uv = fragCoord.xy / iResolution.xy;
    
    // get original image
    vec3 c = texture(iChannel1, uv).rgb;

    // Remove blurred image to original image
    vec3 lumcoeff = vec3(0.299,0.587,0.114);
    float luminance = dot( c - BlurH(iChannel0, uv, 1.0 / iResolution.x, mix(1.0, 0.1*iResolution.y, Amount) ), lumcoeff);

    // composition
    fragColor = vec4( c + vec3(luminance), 1.0);
}
