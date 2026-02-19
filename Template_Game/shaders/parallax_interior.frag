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
layout (location = 0) out vec4 out_LitColor;
layout (location = 1) out vec3 out_Position;
layout (location = 2) out vec3 out_Normal;
layout (location = 3) out vec4 out_AlbedoSpec;
layout (location = 4) out vec4 out_PBRParams;

in VS_OUT {
    vec3 FragPos_world;
    mat3 TBN;
} fs_in;

uniform vec3 viewPos;
uniform samplerCube roomCubemap;
uniform float roomDepth;
uniform mat4 view;
uniform mat4 model;

void main() {
    vec3 viewDir_world = normalize(fs_in.FragPos_world - viewPos);
    vec3 rd_local = transpose(fs_in.TBN) * viewDir_world;
    vec3 ro_local = vec3(0.0, 0.0, 0.0);
    vec3 boxMin = vec3(-0.5, -0.5, -roomDepth);
    vec3 boxMax = vec3(0.5, 0.5, 0.0);

    mat4 invModel = inverse(model);
    vec3 ro_world_transformed = vec3(invModel * vec4(viewPos, 1.0));
    vec3 rd_world_transformed = mat3(invModel) * viewDir_world;

    vec3 invDir = 1.0 / rd_world_transformed;
    vec3 t1 = (boxMin - ro_world_transformed) * invDir;
    vec3 t2 = (boxMax - ro_world_transformed) * invDir;

    vec3 tMin = min(t1, t2);
    vec3 tMax = max(t1, t2);

    float tNear = max(max(tMin.x, tMin.y), tMin.z);
    float tFar = min(min(tMax.x, tMax.y), tMax.z);

    if (tNear > tFar || tFar < 0.0) {
        discard;
    }
    
    vec3 hitPos_world = viewPos + viewDir_world * tFar;
    vec3 roomCenter_world = vec3(model * vec4(0.0, 0.0, -roomDepth * 0.5, 1.0));
    vec3 sampleVec_world = hitPos_world - roomCenter_world;
    vec3 roomColor = texture(roomCubemap, sampleVec_world).rgb;

    vec3 roomColor_Linear = gammaCorrect(roomColor, 2.2) * 0.10;
	vec3 finalColor = aces(roomColor_Linear);

    out_LitColor = vec4(finalColor, 1.0); 
    out_Position = vec3(view * vec4(fs_in.FragPos_world, 1.0));
    
    vec3 worldNormal = normalize(fs_in.TBN[2]);
    out_Normal = normalize(mat3(transpose(inverse(view))) * worldNormal);
    
    out_AlbedoSpec = vec4(finalColor, 1.0); 
    
    out_PBRParams = vec4(0.0, 1.0, 1.0, 1.0); 
}
