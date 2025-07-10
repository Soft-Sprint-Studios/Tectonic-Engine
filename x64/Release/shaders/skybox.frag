#version 450 core
#define PI 3.14159265359

layout(location = 0) out vec4 FragColor;

in vec3 v_worldPos;

uniform bool u_use_cubemap;
uniform samplerCube u_skybox_cubemap;

uniform vec3 sunDirection;
uniform vec3 cameraPos;
uniform sampler2D cloudMap;
uniform float time;

#define iSteps 12
#define jSteps 6

vec2 rsi(vec3 r0, vec3 rd, float sr) {
    float a = dot(rd, rd);
    float b = 2.0 * dot(rd, r0);
    float c = dot(r0, r0) - (sr * sr);
    float d = (b*b) - 4.0*a*c;
    if (d < 0.0) return vec2(1e5, -1e5);
    return vec2(
        (-b - sqrt(d))/(2.0*a),
        (-b + sqrt(d))/(2.0*a)
    );
}

vec3 atmosphere(vec3 r, vec3 r0, vec3 pSun, float iSun, float rPlanet, float rAtmos, vec3 kRlh, float kMie, float shRlh, float shMie, float g) {
    pSun = normalize(pSun);
    r = normalize(r);

    vec2 p = rsi(r0, r, rAtmos);
    if (p.x > p.y) return vec3(0, 0, 0);
    p.y = min(p.y, rsi(r0, r, rPlanet).x);
    float iStepSize = (p.y - p.x) / float(iSteps);

    float iTime = 0.0;
    vec3 totalRlh = vec3(0, 0, 0);
    vec3 totalMie = vec3(0, 0, 0);

    float iOdRlh = 0.0;
    float iOdMie = 0.0;
    
    float mu = dot(r, pSun);
    float mumu = mu * mu;
    float gg = g * g;
    float pRlh = 3.0 / (16.0 * PI) * (1.0 + mumu);
    float pMie = 3.0 / (8.0 * PI) * ((1.0 - gg) * (mumu + 1.0)) / (pow(1.0 + gg - 2.0 * mu * g, 1.5) * (2.0 + gg));

    for (int i = 0; i < iSteps; i++) {
        vec3 iPos = r0 + r * (iTime + iStepSize * 0.5);
        float iHeight = length(iPos) - rPlanet;
        if (iHeight < 0.0) break;

        float odStepRlh = exp(-iHeight / shRlh) * iStepSize;
        float odStepMie = exp(-iHeight / shMie) * iStepSize;
        
        iOdRlh += odStepRlh;
        iOdMie += odStepMie;

        float jStepSize = rsi(iPos, pSun, rAtmos).y / float(jSteps);
        float jTime = 0.0;
        float jOdRlh = 0.0;
        float jOdMie = 0.0;

        for (int j = 0; j < jSteps; j++) {
            vec3 jPos = iPos + pSun * (jTime + jStepSize * 0.5);
            float jHeight = length(jPos) - rPlanet;
            if (jHeight > 0.0) {
                 jOdRlh += exp(-jHeight / shRlh) * jStepSize;
                 jOdMie += exp(-jHeight / shMie) * jStepSize;
            }
            jTime += jStepSize;
        }

        vec3 attn = exp(-(kMie * (iOdMie + jOdMie) + kRlh * (iOdRlh + jOdRlh)));
        
        totalRlh += odStepRlh * attn;
        totalMie += odStepMie * attn;
        
        iTime += iStepSize;
    }

    return iSun * (pRlh * kRlh * totalRlh + pMie * kMie * totalMie);
}

vec2 getSphericalUV(vec3 p) {
    vec3 n = normalize(p);
    return vec2(atan(n.z, n.x) / (2.0 * PI) + 0.5, asin(n.y) / PI + 0.5);
}

float getCloudLighting(vec3 p, vec3 sunDir) {
    vec3 normal = normalize(p);
    float directLight = max(dot(normal, sunDir), 0.0);
    float ambientLight = 0.35;
    float totalLight = ambientLight + (1.0 - ambientLight) * pow(directLight, 4.0);
    return totalLight;
}

float rand(vec3 p) {
    p = fract(p * 0.3183099 + 0.1);
    p *= 17.0;
    return fract(p.x * p.y * p.z * (p.x + p.y + p.z));
}

float noise(vec3 p) {
    vec3 i = floor(p);
    vec3 f = fract(p);
    vec3 u = f * f * (3.0 - 2.0 * f);
    return mix(
        mix(
            mix(rand(i + vec3(0,0,0)), rand(i + vec3(1,0,0)), u.x),
            mix(rand(i + vec3(0,1,0)), rand(i + vec3(1,1,0)), u.x),
            u.y
        ),
        mix(
            mix(rand(i + vec3(0,0,1)), rand(i + vec3(1,0,1)), u.x),
            mix(rand(i + vec3(0,1,1)), rand(i + vec3(1,1,1)), u.x),
            u.y
        ),
        u.z
    );
}

float fbm(vec3 p, int octaves) {
    float value = 0.0;
    float amplitude = 0.5;
    for (int i = 0; i < octaves; i++) {
        value += amplitude * noise(p);
        p *= 2.0;
        amplitude *= 0.5;
    }
    return value;
}

vec3 getCloudColor(vec3 r0, vec3 rd, vec3 sunDir) {
    const float cloudSpeed = 0.04;
    const float cloudScale = 1.5;
    const int   cloudOctaves = 8;
    vec3 p_sample = rd * cloudScale + vec3(time * cloudSpeed, 0.0, 0.0);
    float density = fbm(p_sample, cloudOctaves);
    density = smoothstep(0.45, 0.65, density);
    float viewFade = smoothstep(0.0, 0.25, rd.y);
    float tCloud;
    float cloudRadius = 6471e3 + 2000.0;
    vec2 t = rsi(r0, rd, cloudRadius);
    if (t.x > t.y) {
        return vec3(0.0);
    }
    tCloud = t.x;
    vec3 pCloud = r0 + rd * tCloud;
    float light = getCloudLighting(pCloud, sunDir);
    float alpha = density * 0.6 * viewFade * light;
    return vec3(1.0) * alpha;
}

vec3 gammaCorrect(vec3 color, float gamma) {
    return pow(color, vec3(1.0 / gamma));
}

void main()
{
    if (u_use_cubemap) {
        FragColor = texture(u_skybox_cubemap, v_worldPos);
        return;
    }
    vec3 rayOrigin = vec3(0.0, cameraPos.y + 6371e3, 0.0);
    vec3 rayDir = normalize(v_worldPos);
    vec3 color = atmosphere(
        rayDir,
        rayOrigin,
        sunDirection,
        22.0,
        6371e3,
        6471e3,
        vec3(5.5e-6, 13.0e-6, 22.4e-6),
        21e-6,
        8e3,
        1.2e3,
        0.758
    );
    vec3 cloudCol = getCloudColor(rayOrigin, rayDir, sunDirection);
    color += cloudCol;
    color = 1.0 - exp(-1.0 * color);
    color = gammaCorrect(color, 2.2);
    FragColor = vec4(color, 1.0);
}
