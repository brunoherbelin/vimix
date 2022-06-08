uniform float Radius;
#define MAX_SIZE 5

vec4 dilation (sampler2D source, vec2 uv, vec2 uv_step, float rad) {
    
    vec4 maxValue = vec4(0.0);
    float R = length(vec2(rad)) ;
    float D = rad;

    for (float i=-rad; i <= rad; ++i)
    {
        for (float j=-rad; j <= rad; ++j)
        {
            vec2 delta = vec2(i, j);
            maxValue = max(texture(source, uv + delta * smoothstep(R, D, length(delta)) * uv_step ), maxValue);
        }
    }

    return maxValue;
}

void mainImage( out vec4 fragColor, in vec2 fragCoord )
{
    vec2 uv = fragCoord.xy / iResolution.xy;
    fragColor = dilation(iChannel0, uv, 1.0 / iResolution.xy, mix(1., MAX_SIZE, Radius));
}
