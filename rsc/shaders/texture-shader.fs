#version 330 core

out vec4 FragColor;

in vec4 vertexColor;
in vec2 vertexUV;

uniform sampler2D sourceTexture;
uniform vec4 color;
uniform float contrast;
uniform float brightness;
uniform vec3  resolution;           // viewport resolution (in pixels)

void main()
{		
    vec4 texturecolor = texture(sourceTexture, vertexUV);
    vec3 transformedRGB = mix(vec3(0.62), texturecolor.rgb, contrast + 1.0) + brightness;
    transformedRGB *= vertexColor.rgb;

    FragColor = color * vec4(transformedRGB, texturecolor.a * vertexColor.a);
}
