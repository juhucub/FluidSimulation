#version 410 core

in vec2 fragTexCoord;

out vec4 color;

uniform sampler2D textureSampler;

//float depth = LinearizeDepth

void main()
{
	
	color = texture(textureSampler, fragTexCoord);


}