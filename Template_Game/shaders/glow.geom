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
layout (points) in;
layout (triangle_strip, max_vertices = 4) out;

in VStoGS {
    float size;
    vec3 color;
} gs_in[];

out vec2 TexCoords;
out vec3 GlowColor;

uniform mat4 view;
uniform mat4 projection;

void main()
{
    vec3 pos = gl_in[0].gl_Position.xyz;
    float size = gs_in[0].size;
    GlowColor = gs_in[0].color;
    
    vec3 camRight_worldspace = vec3(view[0][0], view[1][0], view[2][0]);
    vec3 camUp_worldspace = vec3(view[0][1], view[1][1], view[2][1]);
    
    vec3 p0 = pos - camRight_worldspace * size * 0.5 - camUp_worldspace * size * 0.5;
    vec3 p1 = pos + camRight_worldspace * size * 0.5 - camUp_worldspace * size * 0.5;
    vec3 p2 = pos - camRight_worldspace * size * 0.5 + camUp_worldspace * size * 0.5;
    vec3 p3 = pos + camRight_worldspace * size * 0.5 + camUp_worldspace * size * 0.5;
    
    gl_Position = projection * view * vec4(p2, 1.0);
    TexCoords = vec2(0.0, 1.0);
    EmitVertex();

    gl_Position = projection * view * vec4(p0, 1.0);
    TexCoords = vec2(0.0, 0.0);
    EmitVertex();

    gl_Position = projection * view * vec4(p3, 1.0);
    TexCoords = vec2(1.0, 1.0);
    EmitVertex();

    gl_Position = projection * view * vec4(p1, 1.0);
    TexCoords = vec2(1.0, 0.0);
    EmitVertex();
    
    EndPrimitive();
}