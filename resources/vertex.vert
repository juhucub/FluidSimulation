#version 410 core

layout (location = 0) in vec3 vertexPos;
layout (location = 1) in vec3 vertexNorm;
layout (location = 2) in vec3 vertexColor;
layout (location = 3) in vec2 vertexTexCoords;

uniform mat4 model;
uniform mat4 view;
uniform mat4 projection;

out vec3 fragPos;
out vec3 fragVertexNorm;
out vec3 fragColor;
out vec2 fragTexCoord;

void main()
{
	fragColor = vertexColor;
	fragTexCoord = vertexTexCoords;

	//Calculate final vertex position
	gl_Position = projection * view * model * vec4(vertexPos, 1.0);

}