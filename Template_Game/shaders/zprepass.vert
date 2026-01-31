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
layout (location = 0) in vec3 aPos;
layout (location = 10) in ivec4 aBoneIndices;
layout (location = 11) in vec4 aBoneWeights;

uniform mat4 model;
uniform mat4 view;
uniform mat4 projection;
uniform bool u_hasAnimation;
uniform mat4 u_boneMatrices[128];

void main()
{
    mat4 boneTransform = mat4(1.0);
    if(u_hasAnimation)
    {
        boneTransform  = aBoneWeights[0] * u_boneMatrices[aBoneIndices[0]];
        boneTransform += aBoneWeights[1] * u_boneMatrices[aBoneIndices[1]];
        boneTransform += aBoneWeights[2] * u_boneMatrices[aBoneIndices[2]];
        boneTransform += aBoneWeights[3] * u_boneMatrices[aBoneIndices[3]];
    }
    gl_Position = projection * view * model * boneTransform * vec4(aPos, 1.0);
    gl_Position.z += 0.0001;
}