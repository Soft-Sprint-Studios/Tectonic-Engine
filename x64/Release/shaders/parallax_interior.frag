#version 450 core

layout (location = 0) out vec4 out_LitColor;
layout (location = 1) out vec3 out_Position;
layout (location = 2) out vec3 out_Normal;
layout (location = 3) out vec4 out_AlbedoSpec;
layout (location = 4) out vec4 out_PBRParams;
layout (location = 5) out vec2 out_Velocity;

in VS_OUT {
    vec3 FragPos_world;
    mat3 TBN;
} fs_in;

uniform vec3 viewPos;
uniform samplerCube roomCubemap;
uniform float roomDepth;
uniform mat4 view;
uniform mat4 model;

vec3 aces(vec3 x) {
  const float a = 2.51;
  const float b = 0.03;
  const float c = 2.43;
  const float d = 0.59;
  const float e = 0.14;
  return clamp((x * (a * x + b)) / (x * (c * x + d) + e), 0.0, 1.0);
}

vec3 gammaCorrect(vec3 color, float gamma) {
    return pow(color, vec3(1.0 / gamma));
}

void main()
{
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
    
    out_Velocity = vec2(0.0);
}
