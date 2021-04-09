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

float sdRoundSide( in vec2 p, in vec2 b, in float r )
{
    vec2 q = vec2(-1.0, 1.0) * p - b + r;
    return min(max(q.x,q.y),0.0) + length(max(q,0.0)) - r;
}

float sdHorizontal( in vec2 p, in float y )
{
    float d = 0.0;

//    if (y < 0.0) {

//        d =  smoothstep(-2.0 * blur, 0.0, (-2.0 * blur) - (y + p.y));
////        d =  smoothstep(-2.0 * blur, 0.0, -y -blur -p.y); // centered blur


//    }
//    else {

//        d = smoothstep(0.0, 2.0 * blur, y + p.y);

////        d = smoothstep(0.0, 2.0 * blur, y +blur +p.y); // centered blur

//    }
    d = step(0.0, y);
    d = d * smoothstep(0.0, 2.0 * blur, y + p.y) + (1.0-d)*smoothstep(-2.0 * blur, 0.0, (-2.0 * blur) - (y + p.y));

    return d;
}

void main()
{
    vec2 uv = -1.0 + 2.0 * gl_FragCoord.xy / iResolution.xy;
    uv.x *= iResolution.x / iResolution.y;

    float d = sdHorizontal( uv, size.y);

    vec3 col = vec3(d);


//    col = vec3(sign(d));
//    col *= 1.0 - exp( -600.0/ (blur * 1000.0 + 1.0) * abs(d));

//    float d = sdRoundSide( uv, vec2(size.x * iResolution.x/iResolution.y, size.y), blur );
//    vec3 col = vec3(1.0 - sign(d));
//    float delta = 0.5 * length(min(size,1.0));
//    col *= 1.0 - exp( -300.0/ (blur * 1000.0 * delta + 1.0) * abs(d));

    FragColor = vec4( col, 1.0 );
}
