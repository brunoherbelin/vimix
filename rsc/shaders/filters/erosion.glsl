uniform float Radius;
#define MAX_SIZE 5

vec3 erosion (sampler2D source, vec2 uv, vec2 uv_step, float R) 
{
    vec3 minValue = vec3(1.0);  
    float D = length(vec2( R / 2.));

    for (float i=-R; i <= R; ++i)
    {
        for (float j=-R; j <= R; ++j)
        {
            vec2 delta = vec2(i, j);
            minValue = min(texture(source, uv + delta * smoothstep(R, D, length(delta)) * uv_step ).rgb, minValue); 
        }
    }

    return minValue;
}

void mainImage( out vec4 fragColor, in vec2 fragCoord )
{
    vec2 uv = fragCoord.xy / iResolution.xy;
    fragColor = vec4( erosion(iChannel0, uv, 1.0 / iResolution.xy, mix(1., MAX_SIZE, Radius)), 1.0);
}
