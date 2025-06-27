#version 460 core
layout(location = 0) in vec3 aPos;
layout(location = 1) in vec3 aNormal;
layout(location = 2) in vec2 aTexCoords;
layout(location = 3) in vec3 aTangent;
layout(location = 4) in vec4 aColor;
layout(location = 5) in vec2 aTexCoords2;
layout(location = 6) in vec2 aTexCoords3;
layout(location = 7) in vec2 aTexCoords4;

out vec3 FragPos_view;
out vec3 Normal_view;

out vec3 FragPos_world;
out vec2 TexCoords;
out vec2 TexCoords2;
out vec2 TexCoords3;
out vec2 TexCoords4;
out mat3 TBN;
out vec4 FragPosSunLightSpace;
out vec2 Velocity;

out vec3 TangentViewPos;
out vec3 TangentFragPos;
out vec4 v_Color;

uniform mat4 model;
uniform mat4 view;
uniform mat4 projection;
uniform mat4 sunLightSpaceMatrix;
uniform mat4 prevViewProjection;
uniform vec3 viewPos;

void main()
{
    FragPos_world = vec3(model * vec4(aPos, 1.0));
    TexCoords = aTexCoords;
    TexCoords2 = aTexCoords2;
    TexCoords3 = aTexCoords3;
    TexCoords4 = aTexCoords4;
    v_Color = aColor;

    FragPos_view = vec3(view * model * vec4(aPos, 1.0));
    Normal_view = mat3(transpose(inverse(view * model))) * aNormal;

    mat3 normalMatrix = mat3(transpose(inverse(model)));
    vec3 N_world = normalize(normalMatrix * aNormal);
    vec3 T_world = normalize(normalMatrix * aTangent);
    T_world = normalize(T_world - dot(T_world, N_world) * N_world);
    vec3 B_world = cross(N_world, T_world);
    TBN = mat3(T_world, B_world, N_world);
	
	mat3 TBN_inv = transpose(TBN);
    TangentViewPos = TBN_inv * viewPos;
    TangentFragPos = TBN_inv * FragPos_world;
	
    FragPosSunLightSpace = sunLightSpaceMatrix * vec4(FragPos_world, 1.0);
	
    vec4 currentPos = projection * view * model * vec4(aPos, 1.0);
    vec4 prevPos = prevViewProjection * model * vec4(aPos, 1.0);
    
    vec2 currentNDC = currentPos.xy / currentPos.w;
    vec2 prevNDC = prevPos.xy / prevPos.w;
    
    Velocity = (currentNDC - prevNDC);

    gl_Position = projection * view * vec4(FragPos_world, 1.0);
}