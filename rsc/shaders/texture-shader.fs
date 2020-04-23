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
    vec4 textureColor = texture(sourceTexture, vertexUV);
    vec3 transformedRGB = mix(vec3(0.62), textureColor.rgb, contrast + 1.0) + brightness;
    transformedRGB *= vertexColor.rgb * color.rgb;

    float a = int(gl_FragCoord.x + gl_FragCoord.y)%2 > 1 - int(stipple) ? 1.0 : color.a;

    FragColor = vec4(transformedRGB, textureColor.a * vertexColor.a * a);
}
