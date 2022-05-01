uniform float Amount;

float SCurve (float x) {
    x = x * 2.0 - 1.0;
    return -x * abs(x) * 0.5 + x + 0.5;
}
vec3 BlurV (sampler2D source, vec2 uv, float step, float radius) {

    vec3 A = vec3(0.0); 
    vec3 C = vec3(0.0); 
    float divisor = 0.0; 
    float weight = 0.0;
    float radiusMultiplier = 1.0 / radius;

    for (float y = -radius; y <= radius; y++)
    {
        A = texture(source, uv + vec2(0.0, y * step)).rgb;
        weight = SCurve(1.0 - (abs(y) * radiusMultiplier)); 
        C += A * weight; 
        divisor += weight; 
    }

    return C / divisor;
}

void mainImage( out vec4 fragColor, in vec2 fragCoord )
{
    float subsampling = 1.0 - ( Amount * 0.9 );
    vec2 uv = fragCoord.xy / ( iResolution.xy * subsampling );
    
    // Apply vertical blur on downsampled image (allows to divide radius)
	fragColor = vec4( BlurV(iChannel0, uv, 1.0 / (iResolution.y * subsampling), Amount * 100.0 * subsampling), 1.0);
}
