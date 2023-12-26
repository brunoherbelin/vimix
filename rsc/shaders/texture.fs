#version 330 core

out vec4 FragColor;

in vec4 vertexColor;
in vec2 vertexUV;

// from General Shader
uniform vec3 iResolution;           // viewport image resolution (in pixels)
uniform mat4 iTransform;            // image transformation
uniform vec4 color;

// Image Shader
uniform sampler2D iChannel0;        // input channel (texture id).

void main()
{
    // adjust UV
    vec4 texcoord = iTransform * vec4(vertexUV.x, vertexUV.y, 0.0, 1.0);

    // color is a mix of texture, vertex and uniform colors
    vec4 textureColor = texture(iChannel0, texcoord.xy);
    vec3 RGB = textureColor.rgb * vertexColor.rgb * color.rgb;

    // alpha is a mix of texture, vertex, uniform
    float A = textureColor.a * vertexColor.a * color.a;

    // output RGB with Alpha pre-multiplied
    FragColor = vec4(RGB * A, A);
}
