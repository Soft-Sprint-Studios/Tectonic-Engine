#version 460 core
out vec4 FragColor;

in vec2 TexCoords;

uniform sampler2D sceneTexture;
uniform sampler2D bloomBlur;
uniform sampler2D gPosition;
uniform sampler2D volumetricTexture;

uniform int u_fogEnabled;
uniform vec3 u_fogColor;
uniform float u_fogStart;
uniform float u_fogEnd;

uniform float time;
uniform vec2 resolution;
uniform vec2 lightPosOnScreen;
uniform float flareIntensity;

uniform bool u_postEnabled;
uniform float u_crtCurvature;
uniform float u_vignetteStrength;
uniform float u_vignetteRadius;
uniform bool u_lensFlareEnabled;
uniform float u_lensFlareStrength;
uniform float u_scanlineStrength;
uniform float u_grainIntensity;
uniform bool u_chromaticAberrationEnabled;
uniform float u_chromaticAberrationStrength;
uniform bool u_sharpenEnabled;
uniform float u_sharpenAmount;

uniform bool u_ssaoEnabled;
uniform sampler2D ssao;

uniform float u_exposure;

float rand(vec2 co) {
    return fract(sin(dot(co.xy, vec2(12.9898, 78.233)) + time * 60.0) * 43758.5453);
}

struct FlareResult { vec3 ghosts; vec3 glare; };
FlareResult lensflare(vec2 uv, vec2 pos)
{
	vec2 main = uv - pos;
	vec2 uvd = uv * (length(uv));
	float ang = atan(main.x, main.y);
	float dist = length(main);
    dist = pow(dist, 0.1);
	float f1 = max(0.01 - pow(length(uv + 1.2 * pos), 1.9), 0.0) * 7.0;
	float f2 = max(1.0 / (1.0 + 32.0 * pow(length(uvd + 0.8 * pos), 2.0)), 0.0) * 0.25;
	float f22 = max(1.0 / (1.0 + 32.0 * pow(length(uvd + 0.85 * pos), 2.0)), 0.0) * 0.23;
	float f23 = max(1.0 / (1.0 + 32.0 * pow(length(uvd + 0.9 * pos), 2.0)), 0.0) * 0.21;
	vec2 uvx = mix(uv, uvd, -0.5);
	float f4 = max(0.01 - pow(length(uvx + 0.4 * pos), 2.4), 0.0) * 6.0;
	float f42 = max(0.01 - pow(length(uvx + 0.45 * pos), 2.4), 0.0) * 5.0;
	float f43 = max(0.01 - pow(length(uvx + 0.5 * pos), 2.4), 0.0) * 3.0;
	uvx = mix(uv, uvd, -0.4);
	float f5 = max(0.01 - pow(length(uvx + 0.2 * pos), 5.5), 0.0) * 2.0;
	float f52 = max(0.01 - pow(length(uvx + 0.4 * pos), 5.5), 0.0) * 2.0;
	float f53 = max(0.01 - pow(length(uvx + 0.6 * pos), 5.5), 0.0) * 2.0;
	uvx = mix(uv, uvd, -0.5);
	float f6 = max(0.01 - pow(length(uvx - 0.3 * pos), 1.6), 0.0) * 6.0;
	float f62 = max(0.01 - pow(length(uvx - 0.325 * pos), 1.6), 0.0) * 3.0;
	float f63 = max(0.01 - pow(length(uvx - 0.35 * pos), 1.6), 0.0) * 5.0;
	vec3 ghosts = vec3(0.0);
	ghosts.r += f1 + f2 + f4 + f5 + f6;
    ghosts.g += f22 + f42 + f52 + f62;
    ghosts.b += f23 + f43 + f53 + f63;
    ghosts = ghosts * 1.3 - vec3(length(uvd) * 0.05);
    float glare_dist = length(main);
    float glare_falloff = smoothstep(0.25, 0.0, glare_dist);
    glare_falloff += rand(vec2(ang * 8.0, dist * 24.0)) * 0.05;
    vec3 glare = vec3(glare_falloff);
    return FlareResult(ghosts, glare);
}

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
    vec2 curvedTexCoords = TexCoords;
    if (u_postEnabled) {
        vec2 centerCoords = TexCoords - 0.5;
        float dist = dot(centerCoords, centerCoords);
        curvedTexCoords = TexCoords + centerCoords * dist * u_crtCurvature;
    }

    vec3 finalColor;
    if (u_postEnabled && u_chromaticAberrationEnabled) {
        vec2 offset = u_chromaticAberrationStrength * normalize(curvedTexCoords - 0.5);
        finalColor.r = texture(sceneTexture, curvedTexCoords - offset).r;
        finalColor.g = texture(sceneTexture, curvedTexCoords).g;
        finalColor.b = texture(sceneTexture, curvedTexCoords + offset).b;
    } else {
        finalColor = texture(sceneTexture, curvedTexCoords).rgb;
    }
	
    if(u_ssaoEnabled) {
        float occlusion = texture(ssao, curvedTexCoords).r;
        finalColor *= occlusion;
    }

    if (u_postEnabled && u_sharpenEnabled) {
        vec2 texelSize = 1.0 / resolution;

        vec3 blur = vec3(0.0);
        blur += texture(sceneTexture, curvedTexCoords + vec2(-texelSize.x, -texelSize.y)).rgb * 1.0;
        blur += texture(sceneTexture, curvedTexCoords + vec2(0.0, -texelSize.y)).rgb * 2.0;
        blur += texture(sceneTexture, curvedTexCoords + vec2(texelSize.x, -texelSize.y)).rgb * 1.0;

        blur += texture(sceneTexture, curvedTexCoords + vec2(-texelSize.x, 0.0)).rgb * 2.0;
        blur += texture(sceneTexture, curvedTexCoords).rgb * 4.0;
        blur += texture(sceneTexture, curvedTexCoords + vec2(texelSize.x, 0.0)).rgb * 2.0;

        blur += texture(sceneTexture, curvedTexCoords + vec2(-texelSize.x, texelSize.y)).rgb * 1.0;
        blur += texture(sceneTexture, curvedTexCoords + vec2(0.0, texelSize.y)).rgb * 2.0;
        blur += texture(sceneTexture, curvedTexCoords + vec2(texelSize.x, texelSize.y)).rgb * 1.0;

        blur /= 16.0;

        vec3 original = texture(sceneTexture, curvedTexCoords).rgb;
        vec3 mask = original - blur;

        finalColor = original + mask * u_sharpenAmount;
    }

    vec3 bloomColor = texture(bloomBlur, curvedTexCoords).rgb;
    finalColor += bloomColor;

    vec3 volumetricColor = texture(volumetricTexture, curvedTexCoords).rgb;
    finalColor += volumetricColor;

    if (u_fogEnabled == 1)
    {
        float frag_dist = length(texture(gPosition, curvedTexCoords).xyz);
        if (frag_dist > 0.001)
        {
            float fogFactor = smoothstep(u_fogStart, u_fogEnd, frag_dist);
            finalColor = mix(finalColor, u_fogColor, fogFactor);
        }
    }

    if (u_postEnabled && u_lensFlareEnabled) {
        vec2 uv = TexCoords - 0.5;
        uv.x *= resolution.x / resolution.y;
        vec2 flare_pos = lightPosOnScreen - 0.5;
        flare_pos.x *= resolution.x / resolution.y;
        FlareResult flare = lensflare(uv, flare_pos);
        float ghost_strength = 0.5;
        float glare_strength = 1.0;
        vec3 glare_tint = vec3(1.0, 0.8, 0.6);
        vec3 ghosts_component = flare.ghosts * ghost_strength;
        vec3 glare_component = flare.glare * glare_tint * glare_strength;
        vec3 flareColor = (ghosts_component + glare_component) * flareIntensity * u_lensFlareStrength;
        finalColor += flareColor;
    }

    finalColor *= u_exposure;
    finalColor = aces(finalColor);
    finalColor = gammaCorrect(finalColor, 2.2);

    if (u_postEnabled) {
        float scanlineY = TexCoords.y * resolution.y;
        float scanline = sin(scanlineY * 0.8) * u_scanlineStrength + (1.0 - u_scanlineStrength);
        finalColor *= scanline;

        float noise = rand(TexCoords * resolution);
        finalColor += (noise - 0.5) * u_grainIntensity * 0.02;

        float dist = distance(TexCoords, vec2(0.5, 0.5));
        float vignette = smoothstep(u_vignetteRadius, u_vignetteRadius - u_vignetteStrength * 0.5, dist);
        finalColor *= vignette;
    }

    FragColor = vec4(finalColor, 1.0);
}