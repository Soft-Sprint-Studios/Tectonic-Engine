#version 450 core

// Shader based on Pathos engine's monitors.bss

out vec4 FragColor;

in vec2 TexCoords;

uniform sampler2D u_renderTexture;
uniform sampler2D u_scanlineTexture;
uniform int u_grayscale; 

vec3 aces(vec3 x) {
    const float a = 2.51;
    const float b = 0.03;
    const float c = 2.43;
    const float d = 0.59;
    const float e = 0.14;
    return clamp((x * (a * x + b)) / (x * (c * x + d) + e), 0.0, 1.0);
}

vec3 gammaCorrect(vec3 color, float gamma) {
    return pow(color, vec3(1.0 / gamma));
}

void main()
{
    vec4 color = texture(u_renderTexture, TexCoords);

    if (u_grayscale == 1) {
        float gray = dot(color.rgb, vec3(0.299, 0.587, 0.114));
        color.rgb = vec3(gray);
    }

    vec4 scanline = texture(u_scanlineTexture, TexCoords * 4.0);
    color.rgb = mix(color.rgb, scanline.rgb, scanline.a);

    color.rgb = aces(color.rgb);
    color.rgb = gammaCorrect(color.rgb, 2.2);

    FragColor = vec4(color.rgb * 3.0, 1.0);
}