#version 330 core

out vec4 FragColor;

in vec4 vertexColor;
in vec2 vertexUV;

// from General Shader
uniform vec3 iResolution;      // viewport resolution (in pixels)
uniform mat4 iTransform;       // image transformation
uniform vec4 color;            // drawing color
uniform vec4 uv;

// Mask Shader
uniform vec2 size;
uniform float blur;


float sdRoundSide( in vec2 p, in vec2 b, in float r )
{
    vec2 q = vec2(1.0, -1.0) * p - b + r;
    return min(max(q.x,q.y),0.0) + length(max(q,0.0)) - r;
}

void main()
{
    vec2 uv = -1.0 + 2.0 * gl_FragCoord.xy / iResolution.xy;
    uv.x *= iResolution.x / iResolution.y;

    vec2 s = 2.0 - size;
    float d = sdRoundSide( uv, vec2(s.x * iResolution.x/iResolution.y, s.y), blur );

    vec3 col = vec3(1.0 - sign(d));
    float delta = 0.5 * length(min(size,1.0));
    col *= 1.0 - exp( -20.0/ (blur * 100.0 * delta + 1.0)  * abs(d) );

    FragColor = vec4( col, 1.0 );
}
