#version 460 core
out float FragColor;

in vec2 TexCoords;
uniform sampler2D gNormal;
uniform sampler2D gPosition;
uniform vec2 screenSize;

const float sensitivity = 1.2;
const float threshold = 0.10;
const float intensity = 5.0;
const int radius = 10;

void main() {
    vec2 texelSize = 1.0 / screenSize;
    vec3 centerNormal = normalize(texture(gNormal, TexCoords).rgb);
    float fragDepth = abs(texture(gPosition, TexCoords).z);

    float occlusion = 0.0;
    int count = 0;

    for (int x = -radius; x <= radius; x += 3) {
        for (int y = -radius; y <= radius; y += 3) {
            if (x == 0 && y == 0) continue;

            vec2 offset = TexCoords + vec2(x, y) * texelSize;
            vec3 sampleNormal = normalize(texture(gNormal, offset).rgb);
            
            float diff = length(centerNormal - sampleNormal);
            if (diff > threshold) {
                occlusion += diff * sensitivity;
            }
            count++;
        }
    }

    occlusion = clamp(occlusion / float(count), 0.0, 1.0);

    float depthFade = clamp(1.0 - fragDepth * 0.5, 0.2, 1.0);
    occlusion *= depthFade;

    FragColor = pow(1.0 - occlusion, intensity);
}
