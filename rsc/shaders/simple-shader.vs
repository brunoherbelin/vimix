#version 330 core

layout (location = 0) in vec3 position;
layout (location = 1) in vec3 color;

out vec3 vertexColor;

uniform float rotation;
uniform vec2 translation;
uniform float aspectratio;
uniform float zoom;

void main()
{
	mat4 projection = mat4( zoom, 0.0, 0.0, 0.0,
							0.0, zoom * aspectratio, 0.0, 0.0,
							0.0, 0.0, -1.0, 0.0,
							0.0, 0.0, 0.0, 1.0);
	vec2 rotated_pos;
	rotated_pos.x = translation.x + position.x*cos(rotation) - position.y*sin(rotation);
	rotated_pos.y = translation.y + position.x*sin(rotation) + position.y*cos(rotation);
    gl_Position = projection * vec4(rotated_pos.x, rotated_pos.y, position.z, 1.0);
	vertexColor = color;
}
