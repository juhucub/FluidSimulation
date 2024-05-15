#version 410 core

out vec4 color;

uniform vec3 circleColor;

in vec2 TexCoords;

uniform sampler2D screenTexture;

void main()
{
	
	color = texture(screenTexture, TexCoords);

}