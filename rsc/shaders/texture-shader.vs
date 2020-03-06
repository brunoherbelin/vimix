#version 330 core

layout (location = 0) in vec3 position;
layout (location = 1) in vec3 texCoord;

out vec3 vertexUV;

uniform float rotation;
uniform float scale;
uniform vec2 translation;
uniform float aspectratio;

uniform mat4 render_projection;


void main()
{
	// geometry
	vec2 pos = vec2(position.x * aspectratio, position.y);
	pos *= scale;
	vec2 rotated_pos;
	rotated_pos.x = translation.x + pos.x*cos(rotation) - pos.y*sin(rotation);
	rotated_pos.y = translation.y + pos.x*sin(rotation) + pos.y*cos(rotation);

    // output
    gl_Position = render_projection * vec4(rotated_pos.x, rotated_pos.y, position.z, 1.0);
	vertexUV = texCoord;
}
