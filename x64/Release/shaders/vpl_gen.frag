#version 450 core
layout (location = 0) out vec4 out_Position;
layout (location = 1) out vec4 out_Normal;
layout (location = 2) out vec4 out_Albedo;
layout (location = 3) out vec2 out_PBRParams;

in vec3 v_worldPos;
in vec3 v_worldNormal;
in vec2 v_texCoords;

uniform sampler2D diffuseMap;
uniform sampler2D rmaMap;

void main()
{
    vec3 rma = texture(rmaMap, v_texCoords).rgb;
    out_Position = vec4(v_worldPos, 1.0);
    out_Normal = vec4(normalize(v_worldNormal), 1.0);
    out_Albedo = texture(diffuseMap, v_texCoords);
    out_PBRParams = rma.gb;
}