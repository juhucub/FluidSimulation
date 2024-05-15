#version 410 core

out vec4 color;

in vec2 TexCoords;

uniform sampler2D screenTexture;

void main()
{
    vec2 texOffset = 1.0 / textureSize(screenTexture, 0); // gets size of single texel
    float blurSize = 1.0; // Adjust this value to control the blur intensity

    vec3 result = texture(screenTexture, TexCoords).rgb * 0.2270270270;
    result += texture(screenTexture, TexCoords + texOffset * vec2(-1.0, -1.0)).rgb * 0.1945945946;
    result += texture(screenTexture, TexCoords + texOffset * vec2(1.0, -1.0)).rgb * 0.1945945946;
    result += texture(screenTexture, TexCoords + texOffset * vec2(-1.0, 1.0)).rgb * 0.1945945946;
    result += texture(screenTexture, TexCoords + texOffset * vec2(1.0, 1.0)).rgb * 0.1945945946;
    result += texture(screenTexture, TexCoords + texOffset * vec2(0.0, -1.0)).rgb * 0.1945945946;
    result += texture(screenTexture, TexCoords + texOffset * vec2(0.0, 1.0)).rgb * 0.1945945946;
    result += texture(screenTexture, TexCoords + texOffset * vec2(-1.0, 0.0)).rgb * 0.1945945946;
    result += texture(screenTexture, TexCoords + texOffset * vec2(1.0, 0.0)).rgb * 0.1945945946;

    color = vec4(result, 1.0);
}