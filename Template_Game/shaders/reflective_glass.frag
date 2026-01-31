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
out vec4 fragColor;

in vec3 v_normal;
in vec2 v_texCoord;
in vec3 FragPos_world;
in vec4 v_clipSpace;
in mat3 v_tbn;

uniform sampler2D reflectionTexture;
uniform sampler2D normalMap;

void main() {
    vec2 ndc = (v_clipSpace.xy / v_clipSpace.w) * 0.5 + 0.5;

    vec3 normalMapSample = texture(normalMap, v_texCoord).rgb * 2.0 - 1.0;
    vec3 N = normalize(v_tbn * normalMapSample);

    vec2 distortion = N.xy * 0.0;
    vec2 reflectTexCoords = vec2(ndc.x, 1.0 - ndc.y) + distortion;

    vec3 reflectionColor = texture(reflectionTexture, reflectTexCoords).rgb;

    fragColor = vec4(clamp(reflectionColor * 2.5, 0.0, 1.0), 1.0);
}
