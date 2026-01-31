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
layout (location = 1) in vec3 aNormal;
layout (location = 2) in vec2 aTexCoords;
layout (location = 8) in vec2 aTexCoordsLightmap;

out vec3 v_incident;
out vec3 v_bitangent;
out vec3 v_normal;
out vec3 v_tangent;
out vec2 v_texCoord;
out vec4 FragPosSunLightSpace;
out vec3 FragPos_world;
out vec2 v_texCoordLightmap;
out vec4 v_clipSpace;
out vec3 FragPos_view;

uniform mat4 model;
uniform mat4 view;
uniform mat4 projection;
uniform vec3 viewPos;
uniform mat4 sunLightSpaceMatrix;

void main()
{
    vec4 worldPos4 = model * vec4(aPos, 1.0);
    FragPos_world = worldPos4.xyz;
    v_incident = FragPos_world - viewPos;

    mat3 normalMatrix = mat3(transpose(inverse(model)));
    v_normal = normalize(normalMatrix * aNormal);
    
    if (abs(v_normal.y) > 0.999) {
        v_tangent = normalize(cross(vec3(0.0, 1.0, 0.0), v_normal));
    } else {
        v_tangent = normalize(cross(v_normal, vec3(0.0, 1.0, 0.0)));
    }
    v_bitangent = normalize(cross(v_normal, v_tangent));

    v_texCoord = aTexCoords;
    v_texCoordLightmap = aTexCoordsLightmap;
    FragPosSunLightSpace = sunLightSpaceMatrix * worldPos4;

    vec4 viewPos4 = view * worldPos4;
    FragPos_view = viewPos4.xyz;

    gl_Position = projection * viewPos4;
    v_clipSpace = gl_Position;
}