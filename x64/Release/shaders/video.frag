#version 450 core
out vec4 FragColor;

in vec2 TexCoords;

uniform sampler2D videoTexture;

void main()
{
    vec2 flipped_coords = vec2(TexCoords.x, 1.0 - TexCoords.y);
    FragColor = texture(videoTexture, flipped_coords);
}