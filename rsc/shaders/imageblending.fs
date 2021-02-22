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

    // output
    FragColor = vec4( textureColor.a);
}
