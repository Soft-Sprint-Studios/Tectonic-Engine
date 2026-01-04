#version 450 core

// Shader based on Pathos engine's monitors.bss

out vec4 FragColor;

in vec2 TexCoords;

uniform sampler2D u_renderTexture;
uniform sampler2D u_scanlineTexture;
uniform int u_grayscale; 

void main()
{
    vec4 color = texture(u_renderTexture, TexCoords);

    if (u_grayscale == 1) {
        float gray = dot(color.rgb, vec3(0.299, 0.587, 0.114));
        color.rgb = vec3(gray);
    }

    vec4 scanline = texture(u_scanlineTexture, TexCoords * 4.0);

    color.rgb = mix(color.rgb, scanline.rgb, scanline.a);

    FragColor = color;
}