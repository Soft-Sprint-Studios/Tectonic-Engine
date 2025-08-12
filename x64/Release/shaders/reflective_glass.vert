#version 450 core
layout (location = 0) in vec3 aPos;
layout (location = 1) in vec3 aNormal;
layout (location = 2) in vec2 aTexCoords;

out vec3 v_normal;
out vec2 v_texCoord;
out vec3 FragPos_world;
out vec4 v_clipSpace;
out mat3 v_tbn;

uniform mat4 model;
uniform mat4 view;
uniform mat4 projection;

void main()
{
    vec4 worldPos4 = model * vec4(aPos, 1.0);
    FragPos_world = worldPos4.xyz;

    mat3 normalMatrix = mat3(transpose(inverse(model)));
    v_normal = normalize(normalMatrix * aNormal);

    vec3 tangent = normalize(cross(v_normal, vec3(0.0, 1.0, 0.0)));
    if (length(tangent) < 0.1) {
        tangent = normalize(cross(v_normal, vec3(1.0, 0.0, 0.0)));
    }
    vec3 bitangent = normalize(cross(v_normal, tangent));
    v_tbn = mat3(tangent, bitangent, v_normal);

    v_texCoord = aTexCoords;
    
    gl_Position = projection * view * worldPos4;
    v_clipSpace = gl_Position;
}