#version 330 core

out vec4 FragColor;

in vec4 vertexColor;
in vec2 vertexUV;

uniform sampler2D sourceTexture;
uniform vec4 color;
uniform float contrast;
uniform float brightness;
uniform float stipple;
uniform vec3  resolution;           // viewport resolution (in pixels)

void main()
{
    // color is a mix of texture (manipulated with brightness & contrast), vertex and uniform colors
    vec4 textureColor = texture(sourceTexture, vertexUV);
    vec3 RGB = mix(vec3(0.62), textureColor.rgb, contrast + 1.0) + brightness;
    RGB *= vertexColor.rgb * color.rgb;

    // alpha is a mix of texture alpha, vertex alpha, and uniform alpha affected by stippling
    float A = textureColor.a * vertexColor.a * color.a;
    A *= int(gl_FragCoord.x + gl_FragCoord.y) % 2 > (1 - int(stipple)) ? 0.0 : 1.0;

    // output RGBA
    FragColor = vec4(RGB, A);
}
