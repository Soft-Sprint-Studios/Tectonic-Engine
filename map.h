/*
 * Copyright © 2025 Soft Sprint Studios
 * All rights reserved.
 *
 * This file is proprietary and confidential. Unauthorized reproduction,
 * modification, or distribution is strictly prohibited unless explicit
 * written permission is granted by Soft Sprint Studios.
 */
#ifndef MAP_H
#define MAP_H

#include <SDL.h>
#include <GL/glew.h>
#include <stdbool.h>
#include "math_lib.h"
#include "texturemanager.h"
#include "model_loader.h"
#include "physics_wrapper.h"
#include "particle_system.h"
#include "dsp_reverb.h"
#include <AL/al.h>

#ifdef __cplusplus
extern "C" {
#endif

#define MAX_LIGHTS 256
#define MAX_BRUSHES 16384
#define MAX_DECALS 16384
#define MAX_SOUNDS 256
#define MAX_PARTICLE_EMITTERS 256
#define MAX_VIDEO_PLAYERS 64
#define MAX_PARALLAX_ROOMS 256
#define MAX_BRUSH_VERTS 65536
#define MAX_BRUSH_FACES 32768
#define MAX_VPLS 4096
#define VPL_GEN_TEXTURE_SIZE 1024

#define PLAYER_HEIGHT_NORMAL 1.83f
#define PLAYER_HEIGHT_CROUCH 1.37f

typedef enum {
    ENTITY_NONE, ENTITY_MODEL, ENTITY_BRUSH, ENTITY_LIGHT, ENTITY_PLAYERSTART, ENTITY_DECAL, ENTITY_SOUND, ENTITY_PARTICLE_EMITTER, ENTITY_VIDEO_PLAYER, ENTITY_PARALLAX_ROOM
} EntityType;

typedef enum { LIGHT_POINT, LIGHT_SPOT } LightType;

typedef struct {
    char targetname[64];
    LightType type;
    Vec3 position;
    Vec3 direction;
    Vec3 rot;
    Vec3 color;
    float intensity;
    float base_intensity;
    bool is_on;
    float radius;
    float cutOff;
    float outerCutOff;
    GLuint shadowFBO;
    GLuint shadowMapTexture;
    uint64_t shadowMapHandle;
    char cookiePath[128];
    GLuint cookieMap;
    uint64_t cookieMapHandle;
    float shadowFarPlane;
    float shadowBias;
    float volumetricIntensity;
} Light;

typedef struct {
    Vec4 position;
    Vec4 direction;
    Vec4 color;
    Vec4 params1;
    Vec4 params2;
    unsigned int shadowMapHandle[2];
    unsigned int cookieMapHandle[2];
} ShaderLight;

typedef struct {
    bool enabled;
    Vec3 direction;
    Vec3 color;
    float intensity;
    float volumetricIntensity;
} Sun;

typedef struct {
    bool enabled;
    Vec3 color;
    float start;
    float end;
} Fog;

typedef struct {
    bool enabled;
    float crtCurvature;
    float vignetteStrength;
    float vignetteRadius;
    bool lensFlareEnabled;
    float lensFlareStrength;
    float scanlineStrength;
    float grainIntensity;
    bool dofEnabled;
    float dofFocusDistance;
    float dofAperture;
    bool chromaticAberrationEnabled;
    float chromaticAberrationStrength;
    bool sharpenEnabled;
    float sharpenAmount;
    bool bwEnabled;
    float bwStrength;
} PostProcessSettings;

typedef struct {
    Vec3 position;
    float yaw, pitch;
    bool isCrouching;
    float currentHeight;
    RigidBodyHandle physicsBody;
} Camera;

typedef struct {
    Vec3 position;
    unsigned int packedColor;
    unsigned int packedNormal;
    unsigned int _padding[1];
} VPL;

typedef struct {
    GLuint mainShader, pointDepthShader, spotDepthShader, skyboxShader;
    GLuint lightingCompositeShader;
    GLuint postProcessShader;
    GLuint presentShader;
    GLuint quadVAO, quadVBO;
    GLuint skyboxVAO, skyboxVBO;
    GLuint gBufferFBO;
    GLuint gPosition, gNormal, gLitColor, gAlbedo, gPBRParams;
    GLuint gVelocity;
    GLuint cloudTexture;
    GLuint vplGenerationFBO;
    GLuint vplPosTex, vplNormalTex, vplAlbedoTex;
    GLuint vplGenerationShader;
    GLuint vplComputeShader;
    GLuint vplSSBO;
    GLuint brdfLUTTexture;
    GLuint decalVAO, decalVBO;
    GLuint parallaxRoomVAO, parallaxRoomVBO;
    GLuint sunShadowFBO;
    GLuint sunShadowMap;
    GLuint finalRenderFBO;
    GLuint finalRenderTexture;
    GLuint bloomShader;
    GLuint bloomBlurShader;
    GLuint bloomFBO;
    GLuint bloomBrightnessTexture;
    GLuint pingpongFBO[2];
    GLuint pingpongColorbuffers[2];
    GLuint volumetricShader;
    GLuint volumetricBlurShader;
    GLuint volumetricFBO;
    GLuint volumetricTexture;
    GLuint volPingpongFBO[2];
    GLuint volPingpongTextures[2];
    GLuint dofShader;
    GLuint finalDepthTexture;
    GLuint ssaoFBO, ssaoBlurFBO;
    GLuint ssaoColorBuffer, ssaoBlurColorBuffer;
    GLuint ssaoShader, ssaoBlurShader;
    GLuint ssaoNoiseTex;
    Vec3 ssaoKernel[64];
    GLuint postProcessFBO;
    GLuint postProcessTexture;
    GLuint histogramShader;
    GLuint exposureShader;
    GLuint histogramSSBO;
    GLuint exposureSSBO;
    GLuint depthAaShader;
    GLuint motionBlurShader;
    GLuint waterShader;
    GLuint dudvMap;
    GLuint waterNormalMap;
    GLuint parallaxInteriorShader;
    GLuint lightSSBO;
    GLuint debugBufferShader;
    float currentExposure;
    Mat4 prevViewProjection;
} Renderer;

typedef struct {
    char targetname[64];
    float mass;
    bool isPhysicsEnabled;
    char modelPath[270];
    Vec3 pos;
    Vec3 rot;
    Vec3 scale;
    Mat4 modelMatrix;
    LoadedModel* model;
    RigidBodyHandle physicsBody;
} SceneObject;

typedef struct {
    Vec3 pos;
    Vec4 color;
} BrushVertex;

typedef struct {
    Material* material;
    Material* material2;
    Material* material3;
    Material* material4;
    Vec2 uv_offset;
    Vec2 uv_scale;
    float uv_rotation;
    Vec2 uv_offset2;
    Vec2 uv_scale2;
    float uv_rotation2;
    Vec2 uv_offset3;
    Vec2 uv_scale3;
    float uv_rotation3;
    Vec2 uv_offset4;
    Vec2 uv_scale4;
    float uv_rotation4;
    int* vertexIndices;
    int numVertexIndices;
} BrushFace;

typedef struct {
    char targetname[64];
    bool isTrigger;
    bool playerIsTouching;
    Vec3 pos;
    Vec3 rot;
    Vec3 scale;
    Mat4 modelMatrix;
    BrushVertex* vertices;
    int numVertices;
    BrushFace* faces;
    int numFaces;
    GLuint vao, vbo;
    int totalRenderVertexCount;
    RigidBodyHandle physicsBody;
    bool isReflectionProbe;
    bool isWater;
    GLuint cubemapTexture;
    char name[64];
    bool isDSP;
    ReverbPreset reverbPreset;
} Brush;

typedef struct {
    char targetname[64];
    Vec3 pos;
    Vec3 rot;
    Vec3 size;
    Mat4 modelMatrix;
    Material* material;
} Decal;

typedef struct {
    Vec3 position;
} PlayerStart;

typedef struct {
    char targetname[64];
    char soundPath[128];
    Vec3 pos;
    unsigned int bufferID;
    unsigned int sourceID;
    float volume;
    float pitch;
    float maxDistance;
    bool is_looping;
    bool play_on_start;
} SoundEntity;

typedef enum {
    VP_STOPPED,
    VP_PLAYING,
    VP_PAUSED
} VideoPlayerState;

typedef struct plm_t plm_t;

typedef struct {
    char targetname[64];
    char videoPath[128];
    Vec3 pos;
    Vec3 rot;
    Vec2 size;
    Mat4 modelMatrix;

    bool playOnStart;
    bool loop;
    VideoPlayerState state;

    plm_t* plm;
    GLuint textureID;
    ALuint audioSource;
    ALuint audioBuffers[4];
    double time;
    double nextFrameTime;
} VideoPlayer;

typedef struct {
    char targetname[64];
    char cubemapPath[128];
    Vec3 pos;
    Vec3 rot;
    Vec2 size;
    float roomDepth;
    Mat4 modelMatrix;
    GLuint cubemapTexture;
} ParallaxRoom;

typedef struct ParticleEmitter {
    char parFile[128];
    char targetname[64];
    bool is_on;
    bool on_by_default;
    ParticleSystem* system;
    Vec3 pos;
    Particle particles[MAX_PARTICLES_PER_SYSTEM];
    int activeParticles;
    float timeSinceLastSpawn;
    GLuint vao;
    GLuint vbo;
} ParticleEmitter;

typedef struct {
    char mapPath[256];
    Light lights[MAX_LIGHTS];
    int numActiveLights;
    SceneObject* objects;
    int numObjects;
    Brush brushes[MAX_BRUSHES];
    int numBrushes;
    VPL vpls[MAX_VPLS];
    int num_vpls;
    PlayerStart playerStart;
    Decal decals[MAX_DECALS];
    int numDecals;
    SoundEntity soundEntities[MAX_SOUNDS];
    int numSoundEntities;
    ParticleEmitter particleEmitters[MAX_PARTICLE_EMITTERS];
    int numParticleEmitters;
    VideoPlayer videoPlayers[MAX_VIDEO_PLAYERS];
    int numVideoPlayers;
    ParallaxRoom parallaxRooms[MAX_PARALLAX_ROOMS];
    int numParallaxRooms;
    Fog fog;
    PostProcessSettings post;
    Sun sun;
} Scene;

typedef struct {
    SDL_Window* window;
    SDL_GLContext context;
    bool running;
    bool flashlight_on;
    float deltaTime, lastFrame;
    Camera camera;
    PhysicsWorldHandle physicsWorld;
} Engine;

void Light_InitShadowMap(Light* light);
void Calculate_Sun_Light_Space_Matrix(Mat4* outMatrix, const Sun* sun, Vec3 cameraPosition);
void Light_DestroyShadowMap(Light* light);
void Brush_SetVerticesFromBox(Brush* b, Vec3 size);
void Brush_UpdateMatrix(Brush* b);
void Brush_CreateRenderData(Brush* b);
void Brush_FreeData(Brush* b);
void Brush_DeepCopy(Brush* dest, const Brush* src);
void Brush_Clip(Brush* b, Vec3 plane_normal, float plane_d);
void Decal_UpdateMatrix(Decal* d);
void ParallaxRoom_UpdateMatrix(ParallaxRoom* p);
void Scene_Clear(Scene* scene, Engine* engine);
bool Scene_LoadMap(Scene* scene, Renderer* renderer, const char* mapPath, Engine* engine);
void Scene_SaveMap(Scene* scene, const char* mapPath);

#ifdef __cplusplus
}
#endif

#endif // MAP_H