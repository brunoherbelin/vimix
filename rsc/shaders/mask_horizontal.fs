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
uniform vec2  size;            // size of the mask area
uniform float blur;            // percent of blur

float sdHorizontal( in vec2 p, in float y )
{
    float d = step(0.0, y);
    d = d * smoothstep(0.0, 2.0 * blur, y + p.y) + (1.0-d)*smoothstep(-2.0 * blur, 0.0, (-2.0 * blur) - (y + p.y));
    return d;
}

void main()
{
    vec2 uv = -1.0 + 2.0 * gl_FragCoord.xy / iResolution.xy;

    float d = sdHorizontal( uv, size.y);

    FragColor = vec4( vec3(d), 1.0 );
}
