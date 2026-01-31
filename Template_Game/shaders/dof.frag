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