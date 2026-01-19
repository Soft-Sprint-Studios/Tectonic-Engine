#version 450 core
out vec4 FragColor;

in vec2 TexCoords;
in vec4 ParticleColor;
in float ParticleViewZ;
in vec3 worldPos;

uniform sampler2D particleTexture;
uniform sampler2D gPosition;
uniform vec2 screenSize;
uniform float softness;

struct Sun {
    bool enabled;
    vec3 direction;
    vec3 color;
    float intensity;
};

struct Flashlight {
    bool enabled;
    vec3 position;
    vec3 direction;
};

struct AmbientProbe {
    vec3 position;
    vec3 colors[6];
    vec3 dominant_direction;
};

uniform Sun sun;
uniform Flashlight flashlight;
uniform vec3 viewPos;
uniform AmbientProbe u_probes[8];
uniform int u_numAmbientProbes;
uniform bool u_useLighting;

void main()
{
    vec4 texColor = texture(particleTexture, TexCoords);
    if (texColor.a < 0.1)
        discard;

    vec2 screenUV = gl_FragCoord.xy / screenSize;
    float sceneViewZ = texture(gPosition, screenUV).z;

    float depthDelta = ParticleViewZ - sceneViewZ;
    float fade = clamp(depthDelta / softness, 0.0, 1.0);
    texColor.a *= fade;

    vec3 finalColor;

    if (!u_useLighting) {
        finalColor = (texColor * ParticleColor).rgb;
    } else {
        vec3 N = normalize(viewPos - worldPos);

        vec3 ambient = vec3(0.0);
        if (u_numAmbientProbes > 0) {
            vec3 total_color = vec3(0.0);
            float total_weight = 0.0;

            for (int i = 0; i < 8; i++) {
                if (length(u_probes[i].position) < 0.01) continue;

                float dist_sq = max(0.001, dot(worldPos - u_probes[i].position,
                                                worldPos - u_probes[i].position));
                float weight = 1.0 / dist_sq;

                vec3 probe_color = vec3(0.0);
                probe_color += u_probes[i].colors[0] * max(0.0, dot(N, vec3( 1,  0,  0)));
                probe_color += u_probes[i].colors[1] * max(0.0, dot(N, vec3(-1,  0,  0)));
                probe_color += u_probes[i].colors[2] * max(0.0, dot(N, vec3( 0,  1,  0)));
                probe_color += u_probes[i].colors[3] * max(0.0, dot(N, vec3( 0, -1,  0)));
                probe_color += u_probes[i].colors[4] * max(0.0, dot(N, vec3( 0,  0,  1)));
                probe_color += u_probes[i].colors[5] * max(0.0, dot(N, vec3( 0,  0, -1)));

                total_color += probe_color * weight;
                total_weight += weight;
            }

            if (total_weight > 0.0)
                ambient = total_color / total_weight;
        }

        vec3 Lo = vec3(0.0);

        if (sun.enabled) {
            vec3 lightDir = -sun.direction;
            float NdotL = max(dot(N, lightDir), 0.0);
            vec3 radiance = sun.color * sun.intensity;
            Lo += radiance * NdotL;
        }

        if (flashlight.enabled) {
            vec3 L = normalize(flashlight.position - worldPos);
            float NdotL = max(dot(N, L), 0.0);
            float distance = length(flashlight.position - worldPos);
            float attenuation =
                pow(max(0.0, 1.0 - distance / 35.0), 2.0) /
                (distance * distance + 1.0);

            float theta = dot(L, -flashlight.direction);
            float innerCutOff = cos(radians(12.5));
            float outerCutOff = cos(radians(17.5));

            if (theta > outerCutOff) {
                float cone_intensity =
                    clamp((theta - outerCutOff) /
                          (innerCutOff - outerCutOff), 0.0, 1.0);

                Lo += vec3(10.0) * NdotL * attenuation * cone_intensity;
            }
        }

        vec3 finalLight = ambient + Lo;
        finalColor = (texColor * ParticleColor).rgb * finalLight;
    }

    FragColor = vec4(finalColor, texColor.a * ParticleColor.a);
}
