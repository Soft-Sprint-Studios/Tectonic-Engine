#version 450 core

out vec4 FragColor;

in vec2 TexCoords;

uniform sampler2D sceneTexture;
uniform sampler2D bloomBlur;
uniform sampler2D gPosition;
uniform sampler2D volumetricTexture;

uniform int u_fxaa_enabled;

uniform bool u_colorCorrectionEnabled;
uniform sampler2D colorCorrectionLUT;

uniform bool u_bloomEnabled;
uniform bool u_volumetricsEnabled;

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
uniform bool u_bwEnabled;
uniform float u_bwStrength;
uniform bool u_invertEnabled;
uniform float u_invertStrength;
uniform bool u_isUnderwater;
uniform vec3 u_underwaterColor;

uniform bool u_fadeActive;
uniform float u_fadeAlpha;
uniform vec3 u_fadeColor;
uniform float u_red_flash_intensity;

uniform vec3 u_flareLightWorldPos;
uniform mat4 u_view;

uniform bool u_ssaoEnabled;
uniform sampler2D ssao;

layout(std430, binding = 1) readonly buffer ExposureData {
    float u_exposure;
};

float rand(vec2 co) {
    return fract(sin(dot(co.xy, vec2(12.9898, 78.233)) + time * 60.0) * 43758.5453);
}

struct FlareResult {
    vec3 ghosts;
    vec3 glare;
};

FlareResult lensflare(vec2 uv, vec2 pos)
{
    vec2 main = uv - pos;
    float glare_dist = length(main);
    float f0 = max(0.01 - pow(glare_dist, 1.5), 0.0) * 0.8;
    vec3 glare = vec3(f0);
    float f1 = max(0.01 - pow(length(uv + 1.2 * pos), 1.9), 0.0) * 7.0;
    float f2 = max(1.0 / (1.0 + 32.0 * pow(length(uv + 0.8 * pos), 2.0)), 0.0) * 0.25;
    vec2 uvx = mix(uv, uv, -0.5);
    float f4 = max(0.01 - pow(length(uvx + 0.4 * pos), 2.4), 0.0) * 6.0;
    uvx = mix(uv, uv, -0.4);
    float f5 = max(0.01 - pow(length(uvx + 0.2 * pos), 5.5), 0.0) * 2.0;
    uvx = mix(uv, uv, -0.5);
    float f6 = max(0.01 - pow(length(uvx - 0.3 * pos), 1.6), 0.0) * 6.0;
    float ghost_intensity = f1 + f2 + f4 + f5 + f6;
    vec3 ghosts = vec3(ghost_intensity * 1.3);
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

vec2 applyCurvature(vec2 uv, float curvature) {
    vec2 centerCoords = uv - 0.5;
    float dist = dot(centerCoords, centerCoords);
    return uv + centerCoords * dist * curvature;
}

vec3 applyChromaticAberration(vec2 uv, float strength, vec3 baseColor) {
    vec2 offset = strength * normalize(uv - 0.5);
    float r = texture(sceneTexture, uv - offset).r;
    float g = baseColor.g;
    float b = texture(sceneTexture, uv + offset).b;
    return vec3(r, g, b);
}

vec3 applySharpen(vec2 uv, float amount, vec3 baseColor) {
    vec2 texelSize = 1.0 / resolution;
    vec3 blur = vec3(0.0);
    blur += texture(sceneTexture, uv + vec2(-texelSize.x, -texelSize.y)).rgb * 1.0;
    blur += texture(sceneTexture, uv + vec2(0.0, -texelSize.y)).rgb * 2.0;
    blur += texture(sceneTexture, uv + vec2(texelSize.x, -texelSize.y)).rgb * 1.0;
    blur += texture(sceneTexture, uv + vec2(-texelSize.x, 0.0)).rgb * 2.0;
    blur += texture(sceneTexture, uv).rgb * 4.0;
    blur += texture(sceneTexture, uv + vec2(texelSize.x, 0.0)).rgb * 2.0;
    blur += texture(sceneTexture, uv + vec2(-texelSize.x, texelSize.y)).rgb * 1.0;
    blur += texture(sceneTexture, uv + vec2(0.0, texelSize.y)).rgb * 2.0;
    blur += texture(sceneTexture, uv + vec2(texelSize.x, texelSize.y)).rgb * 1.0;
    blur /= 16.0;

    vec3 mask = texture(sceneTexture, uv).rgb - blur;
    return baseColor + mask * amount;
}

vec3 applyLensFlare(vec3 color, vec2 uv) {
    vec2 uvScaled = uv - 0.5;
    uvScaled.x *= resolution.x / resolution.y;
    vec2 flare_pos = lightPosOnScreen - 0.5;
    flare_pos.x *= resolution.x / resolution.y;

    vec3 lightPosView = (u_view * vec4(u_flareLightWorldPos, 1.0)).xyz;
    float sceneDepthAtLight = texture(gPosition, lightPosOnScreen).z;
    float occlusionFactor = (lightPosView.z > sceneDepthAtLight - 0.5) ? 1.0 : 0.0;

    if (occlusionFactor > 0.0) {
        FlareResult flare = lensflare(uvScaled, flare_pos);
        vec3 ghostColor = flare.ghosts * vec3(0.7, 0.9, 1.0);
        vec3 glareColor = flare.glare * vec3(1.0, 0.9, 0.7);
        vec3 flareColor = (ghostColor + glareColor) * flareIntensity * u_lensFlareStrength * occlusionFactor;
        color += flareColor;
    }
    return color;
}

vec3 applyFog(vec3 color, vec2 uv) {
    float frag_dist = length(texture(gPosition, uv).xyz);
    if (frag_dist > 0.001) {
        float fogFactor = smoothstep(u_fogStart, u_fogEnd, frag_dist);
        color = mix(color, u_fogColor, fogFactor);
    }
    return color;
}

vec3 applyVignette(vec3 color, vec2 uv) {
    float dist = distance(uv, vec2(0.5, 0.5));
    float vignette = smoothstep(u_vignetteRadius, u_vignetteRadius - u_vignetteStrength * 0.5, dist);
    return color * vignette;
}

vec3 applyScanline(vec3 color, vec2 uv) {
    float scanlineY = uv.y * resolution.y;
    float scanline = sin(scanlineY * 0.8) * u_scanlineStrength + (1.0 - u_scanlineStrength);
    return color * scanline;
}

vec3 applyGrain(vec3 color, vec2 uv) {
    float noise = rand(uv * resolution);
    return color + (noise - 0.5) * u_grainIntensity * 0.02;
}

vec3 applyBlackAndWhite(vec3 color) {
    float luminance = dot(color, vec3(0.2126, 0.7152, 0.0722));
    vec3 grayscale = vec3(luminance);
    return mix(color, grayscale, u_bwStrength);
}

vec2 underwater_distort(vec2 uv, float t) {
    return uv + vec2(sin(uv.y * 10.0 + t) * 0.01, cos(uv.x * 8.0 + t * 0.7) * 0.01);
}

vec3 applyLUT(vec3 color) {
    float blueSlice = color.b * 15.0;
    float sliceOffset = floor(blueSlice);
    float sliceLerp = fract(blueSlice);

    vec2 uv1 = vec2((color.r * 15.0 + sliceOffset) / 16.0, color.g);
    vec2 uv2 = vec2((color.r * 15.0 + sliceOffset + 1.0) / 16.0, color.g);

    vec3 color1 = texture(colorCorrectionLUT, uv1).rgb;
    vec3 color2 = texture(colorCorrectionLUT, uv2).rgb;

    return mix(color1, color2, sliceLerp);
}

vec3 applyFXAA(sampler2D tex, vec2 uv) {
    float FXAA_SPAN_MAX = 8.0;
    float FXAA_REDUCE_MUL = 1.0/8.0;
    float FXAA_REDUCE_MIN = 1.0/128.0;

    vec3 rgbNW = texture(tex, uv + (vec2(-1.0, -1.0) / resolution)).xyz;
    vec3 rgbNE = texture(tex, uv + (vec2(1.0, -1.0) / resolution)).xyz;
    vec3 rgbSW = texture(tex, uv + (vec2(-1.0, 1.0) / resolution)).xyz;
    vec3 rgbSE = texture(tex, uv + (vec2(1.0, 1.0) / resolution)).xyz;
    vec3 rgbM  = texture(tex, uv).xyz;

    vec3 luma = vec3(0.299, 0.587, 0.114);
    float lumaNW = dot(rgbNW, luma);
    float lumaNE = dot(rgbNE, luma);
    float lumaSW = dot(rgbSW, luma);
    float lumaSE = dot(rgbSE, luma);
    float lumaM  = dot(rgbM,  luma);

    float lumaMin = min(lumaM, min(min(lumaNW, lumaNE), min(lumaSW, lumaSE)));
    float lumaMax = max(lumaM, max(max(lumaNW, lumaNE), max(lumaSW, lumaSE)));

    vec2 dir;
    dir.x = -((lumaNW + lumaNE) - (lumaSW + lumaSE));
    dir.y =  ((lumaNW + lumaSW) - (lumaNE + lumaSE));

    float dirReduce = max(
        (lumaNW + lumaNE + lumaSW + lumaSE) * (0.25 * FXAA_REDUCE_MUL),
        FXAA_REDUCE_MIN);

    float rcpDirMin = 1.0 / (min(abs(dir.x), abs(dir.y)) + dirReduce);

    dir = min(vec2(FXAA_SPAN_MAX,  FXAA_SPAN_MAX),
          max(vec2(-FXAA_SPAN_MAX, -FXAA_SPAN_MAX),
          dir * rcpDirMin)) / resolution;

    vec3 rgbA = (1.0/2.0) * (
        texture(tex, uv + dir * (1.0/3.0 - 0.5)).xyz +
        texture(tex, uv + dir * (2.0/3.0 - 0.5)).xyz);
    vec3 rgbB = rgbA * (1.0/2.0) + (1.0/4.0) * (
        texture(tex, uv + dir * (0.0/3.0 - 0.5)).xyz +
        texture(tex, uv + dir * (3.0/3.0 - 0.5)).xyz);
        
    float lumaB = dot(rgbB, luma);

    if((lumaB < lumaMin) || (lumaB > lumaMax)) {
        return rgbA;
    } else {
        return rgbB;
    }
}

void main()
{
    vec2 uv = TexCoords;
    if (u_postEnabled) {
        uv = applyCurvature(uv, u_crtCurvature);
    }
	
    if (u_isUnderwater) {
        uv = underwater_distort(uv, time);
    }

    vec3 baseColor;

    if (u_fxaa_enabled == 1) {
        baseColor = applyFXAA(sceneTexture, uv);
    } else {
        baseColor = texture(sceneTexture, uv).rgb;
    }

    vec3 color;

    if (u_chromaticAberrationEnabled) {
        color = applyChromaticAberration(uv, u_chromaticAberrationStrength, baseColor);
    } else {
        color = baseColor;
    }

    if (u_ssaoEnabled) {
        float occlusion = texture(ssao, uv).r;
        color *= occlusion;
    }

    if (u_sharpenEnabled) {
        color = applySharpen(uv, u_sharpenAmount, color);
    }

    if (u_volumetricsEnabled) {
        color += texture(volumetricTexture, uv).rgb;
    }

    if (u_fogEnabled == 1) {
        color = applyFog(color, uv);
    }

    if (u_postEnabled && u_lensFlareEnabled) {
        color = applyLensFlare(color, TexCoords);
    }

    color *= u_exposure;
    color = aces(color);
	if (u_bloomEnabled) {
        color += texture(bloomBlur, uv).rgb;
    }
    color = gammaCorrect(color, 2.2);
	
    if (u_colorCorrectionEnabled) {
        color = applyLUT(color);
    }

    if (u_postEnabled) {
        color = applyScanline(color, TexCoords);
        color = applyGrain(color, TexCoords);
        color = applyVignette(color, TexCoords);
    }

    if (u_bwEnabled) {
        color = applyBlackAndWhite(color);
    }
	
    if (u_invertEnabled) {
        vec3 invertedColor = vec3(1.0) - color;
        color = mix(color, invertedColor, u_invertStrength);
    }
	
    if (u_isUnderwater) {
        color = mix(color, u_underwaterColor, 0.6);
        color *= 0.8;
    }
	
    if (u_fadeActive) {
        color = mix(color, u_fadeColor, u_fadeAlpha);
    }
	
    if (u_red_flash_intensity > 0.0) {
        vec3 red_flash_color = vec3(1.0, 0.1, 0.1);

        float dist = distance(TexCoords, vec2(0.5));

        float vignette_strength = pow(dist * 1.4, 2.5);
        vignette_strength = clamp(vignette_strength, 0.0, 1.0);

        float mix_factor = u_red_flash_intensity * vignette_strength;
        
        color = mix(color, red_flash_color, mix_factor);
    }

    FragColor = vec4(color, 1.0);
}
