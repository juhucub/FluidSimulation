#version 410 core

in vec2 TexCoords;
out vec4 fragColor;

uniform vec3 circleColor;
uniform float alpha;		//opacity

uniform sampler2D screenTexture;

void main()
{
	
	fragColor = vec4(circleColor, alpha);

}