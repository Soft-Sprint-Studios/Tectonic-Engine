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

uniform vec3 u_color;
uniform float u_time;

float rand(vec2 co){
    return fract(sin(dot(co.xy ,vec2(12.9898,78.233))) * 43758.5453);
}

void main()
{
    vec2 centered_uv = TexCoords - 0.5;

    float core_width = 0.05;
    float core = 1.0 - smoothstep(core_width, core_width + 0.1, abs(centered_uv.x));

    float glow = pow(1.0 - abs(centered_uv.x * 2.0), 4.0);

    float flicker = sin(TexCoords.y * 30.0 + u_time * 20.0) * 0.5 + 0.5;
    float slow_pulse = sin(TexCoords.y * 5.0 - u_time * 5.0) * 0.5 + 0.5;

    float dynamic_effect = (flicker * 0.7 + slow_pulse * 0.3) * glow;

    float intensity = core * 1.5 + glow * 0.3 + dynamic_effect * 0.5;

    vec3 final_color = u_color * intensity;
    float alpha = clamp(intensity * 0.3, 0.0, 1.0);
    
    FragColor = vec4(final_color, alpha);
}