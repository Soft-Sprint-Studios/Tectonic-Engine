#version 460 core
out vec4 FragColor;

in vec2 TexCoords;

uniform sampler2D sceneTexture;
uniform sampler2D velocityTexture;

const int NUM_SAMPLES = 8;
const float BLUR_STRENGTH = 0.7;

void main()
{
    vec2 velocity = texture(velocityTexture, TexCoords).rg;

    if(length(velocity) < 0.001)
    {
        FragColor = texture(sceneTexture, TexCoords);
        return;
    }

    vec3 result = texture(sceneTexture, TexCoords).rgb;

    for(int i = 1; i < NUM_SAMPLES; ++i)
    {
        vec2 offset = velocity * (float(i) / float(NUM_SAMPLES - 1) - 0.5) * BLUR_STRENGTH;
        vec2 sampleCoord = clamp(TexCoords + offset, 0.0, 1.0);
        result += texture(sceneTexture, sampleCoord).rgb;
    }

    result /= float(NUM_SAMPLES);

    FragColor = vec4(result, 1.0);
}