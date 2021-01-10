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

float udSegment( in vec2 p, in vec2 a, in vec2 b )
{
    vec2 ba = b-a;
    vec2 pa = p-a;
    float h =clamp( dot(pa,ba)/dot(ba,ba), 0.0, 1.0 );
    return length(pa-h*ba);
}

void main()
{
    float ar = iResolution.x / iResolution.y;
    vec2 uv = -1.0 + 2.0 * gl_FragCoord.xy / iResolution.xy;
    uv.x *= ar;

    float th = 0.0;
    vec2 v1;
    vec2 v2;
    vec2 rect = size;
    rect.x *= ar;

    if(rect.x < rect.y) {
        th = rect.x;
        v1 = vec2(0.0, -rect.y + th) ;
        v2 = vec2(0.0,  rect.y - th) ;
    }
    else {
        th = rect.y;
        v1 = vec2(-rect.x + th, 0.0) ;
        v2 = vec2( rect.x - th, 0.0) ;
    }
    float d = udSegment( uv, v1, v2 )- th;

    vec3 col = vec3(1.0- sign(d));
    col *= 1.0 - exp( -600.0/ (blur * 1000.0 + 1.0) * abs(d));

    FragColor = vec4( col, 1.0 );
}
