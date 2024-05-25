#version 410 core

layout (location = 0) in vec2 position;
layout (location = 1) in vec2 aTexCoords;

uniform mat4 model;
uniform mat4 projection;

out vec2 TexCoords;

void main()
{
	TexCoords = aTexCoords;
	gl_Position = projection * model * vec4(position, 0.0, 1.0);

}