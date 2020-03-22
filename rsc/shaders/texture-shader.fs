#version 330 core

out vec4 FragColor;

uniform sampler2D sourceTexture;
in vec3 vertexUV;

uniform vec3 color;
uniform float contrast;
uniform float brightness;

void main()
{		
    vec4 texturecolor = texture(sourceTexture, vertexUV.xy);
    vec3 transformedRGB = mix(vec3(0.62), texturecolor.rgb, contrast + 1.0) + brightness;
    transformedRGB *= color;

    FragColor = vec4(transformedRGB, texturecolor.a);
}
