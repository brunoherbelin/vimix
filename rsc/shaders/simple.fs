#version 330 core

out vec4 FragColor;

in vec4 vertexColor;

uniform vec4 color;            // drawing color
uniform vec3 iResolution;      // viewport resolution (in pixels)

void main()
{
    FragColor = color * vertexColor;
}
