#version 450 core
out vec4 FragColor;

in vec2 TexCoords;
in vec4 ParticleColor;

uniform sampler2D particleTexture;

void main()
{
    vec4 texColor = texture(particleTexture, TexCoords);
    if (texColor.a < 0.1)
        discard;

    FragColor = texColor * ParticleColor;
}