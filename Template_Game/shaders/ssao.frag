/*
 * MIT License
 *
 * Copyright (c) 2025-2026 Soft Sprint Studios
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */
out float FragColor;

in vec2 TexCoords;
uniform sampler2D gGeometryNormal;
uniform sampler2D gPosition;
uniform vec2 screenSize;

const float sensitivity = 1.2;
const float threshold = 0.25;
const float intensity = 10.0;
const int radius = 3;

void main() {
    vec3 viewPos = texture(gPosition, TexCoords).xyz;
    if (viewPos.z == 0.0) {
        FragColor = 1.0;
        return;
    }
    vec2 texelSize = 1.0 / screenSize;
    vec3 centerNormal = normalize(texture(gGeometryNormal, TexCoords).rgb);
    float fragDepth = abs(texture(gPosition, TexCoords).z);

    float occlusion = 0.0;
    int count = 0;

    for (int x = -radius; x <= radius; x += 4) {
        for (int y = -radius; y <= radius; y += 4) {
            if (x == 0 && y == 0) continue;

            vec2 offset = TexCoords + vec2(x, y) * texelSize;
            vec3 sampleNormal = normalize(texture(gGeometryNormal, offset).rgb);
            
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
