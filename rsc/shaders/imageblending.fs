#version 330 core

out vec4 FragColor;

in vec4 vertexColor;
in vec2 vertexUV;

// from General Shader
uniform vec3 iResolution;           // viewport image resolution (in pixels)
uniform mat4 iTransform;            // image transformation
uniform vec4 color;

// Image Shader
uniform sampler2D iChannel0;             // input channel (texture id).
uniform sampler2D iChannel1;             // input mask
uniform float stipple;

void main()
{
    // adjust UV
    vec4 texcoord = iTransform * vec4(vertexUV.x, vertexUV.y, 0.0, 1.0);

    // color texture
    vec4 textureColor = texture(iChannel0, texcoord.xy);
    vec3 RGB = textureColor.rgb * vertexColor.rgb * color.rgb;

    // alpha is a mix of texture, vertex, uniform and mask alpha
    float A = textureColor.a * vertexColor.a * color.a;

    // post-reversing premultiplication of alpha
    float invA = 1.0 / clamp( A , 0.000001, 10.0);
//    float invA = 1.0 / (A*A);

    // output
    FragColor = vec4( RGB * invA, A);
}
