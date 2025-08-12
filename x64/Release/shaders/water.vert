#version 450 core
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

    gl_Position = projection * view * worldPos4;
    v_clipSpace = gl_Position;
}