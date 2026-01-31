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
// Shader based on Pathos engine's blackhole.bss

in vec2 TexCoords;
out vec4 oColor;

uniform sampler2D screenTexture;

uniform vec2 screensize;
uniform vec2 screenpos;
uniform float distance_uniform;
uniform float size;
uniform float rotation_angle;

void main()
{
	vec2 scrcoords = TexCoords;

	vec2 balanced = scrcoords - screenpos;

	balanced.x *= screensize.x / screensize.y;

	float cosA = cos(rotation_angle);
	float sinA = sin(rotation_angle);
	mat2 rotationMatrix = mat2(
		cosA, -sinA,
		sinA,  cosA
	);
	balanced = rotationMatrix * balanced;

	vec2 normalized = normalize(balanced);
	float dist = length(balanced);

	float scaled = dist * distance_uniform * 0.05 * (1.0 / size);
	float strength = 1.0 / (scaled * scaled);

	vec3 rayDirection = vec3(0, 0, 1);
	vec3 surfaceNormal = normalize(vec3(normalized, 1.0 / strength));

	vec3 newBeam = refract(rayDirection, surfaceNormal, 1.6);
	vec2 newPos = scrcoords + vec2(newBeam.x, newBeam.y);

	vec4 finalColor = texture(screenTexture, newPos);

	finalColor *= length(newBeam);
	finalColor.a = 1.0;

	oColor = finalColor;
}
