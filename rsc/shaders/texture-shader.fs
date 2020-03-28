#version 330 core

out vec4 FragColor;

in vec3 vertexUV;
in vec3 vertexColor;

uniform sampler2D sourceTexture;
uniform vec4 color;
uniform float contrast;
uniform float brightness;

void main()
{		
    vec4 texturecolor = texture(sourceTexture, vertexUV.xy);
    vec3 transformedRGB = mix(vec3(0.62), texturecolor.rgb, contrast + 1.0) + brightness;
    transformedRGB *= vertexColor;

    FragColor = color * vec4(transformedRGB, texturecolor.a);
}
