#version 460 core
out vec4 FragColor;

in vec2 TexCoords;

uniform sampler2D screenTexture;
uniform sampler2D depthTexture;

uniform float u_focusDistance;
uniform float u_aperture;

void main()
{
    float depth = texture(depthTexture, TexCoords).r;

    float coc = abs(depth - u_focusDistance) * u_aperture;
    coc = clamp(coc, 0.0, 1.0);

    if (coc < 0.01)
    {
        FragColor = texture(screenTexture, TexCoords);
        return;
    }

    int kernelSize = int(coc * 8.0);
    vec2 texelSize = 1.0 / textureSize(screenTexture, 0);
    vec3 result = vec3(0.0);
    float total = 0.0;

    for (int x = -kernelSize; x <= kernelSize; ++x)
    {
        for (int y = -kernelSize; y <= kernelSize; ++y)
        {
            result += texture(screenTexture, TexCoords + vec2(x, y) * texelSize).rgb;
            total += 1.0;
        }
    }

    FragColor = vec4(result / total, 1.0);
}