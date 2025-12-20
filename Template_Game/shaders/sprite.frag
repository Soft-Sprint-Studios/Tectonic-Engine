#version 450 core
out vec4 FragColor;

in vec2 TexCoords;

uniform sampler2D spriteTexture;

void main()
{
    vec4 texColor = texture(spriteTexture, TexCoords);
    if (texColor.a < 0.1)
        discard;

    FragColor = texColor;
}