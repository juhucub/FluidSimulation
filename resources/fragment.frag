#version 410 core

in vec3 fragColor;
in vec2 fragTexCoord;

out vec4 color;

uniform sampler2D textureSampler;

//float depth = LinearizeDepth

void main()
{
	
	vec4 sampleColor = texture(textureSampler, fragTexCoord);
	color = sampleColor * vec4(fragColor, 1.0);

}