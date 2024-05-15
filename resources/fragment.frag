#version 410 core

out vec4 color;

uniform vec3 circleColor;

void main()
{
	
	color = vec4(circleColor, 1.0);

}