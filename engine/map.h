/*
 * MIT License
 *
 * Copyright (c) 2025 Soft Sprint Studios
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */
#ifndef MAP_H
#define MAP_H

 //----------------------------------------//
 // Brief: Main map and renderer (will refractor)
 //----------------------------------------//

#include <SDL.h>
#include <GL/glew.h>
#include <stdbool.h>
#include "math_lib.h"
#include "texturemanager.h"
#include "water_manager.h"
#include "model_loader.h"
#include "physics_wrapper.h"
#include "particle_system.h"
#include "dsp_reverb.h"
#include <AL/al.h>

#ifdef __cplusplus
extern "C" {
#endif

#define WINDOW_WIDTH 1920
#define WINDOW_HEIGHT 1080

#define GEOMETRY_PASS_DOWNSAMPLE_FACTOR 1.05

#define MAX_LIGHTS 256
#define MAX_BRUSHES 8192
#define MAX_MODELS 8192
#define MAX_DECALS 8192
#define MAX_SOUNDS 2048
#define MAX_PARTICLE_EMITTERS 2048
#define MAX_SPRITES 8192
#define MAX_VIDEO_PLAYERS 32
#define MAX_PARALLAX_ROOMS 128
#define MAX_BRUSH_VERTS 32768
#define MAX_BRUSH_FACES 16384
#define MAX_LOGIC_ENTITIES 8192
#define MAX_ENTITY_PROPERTIES 32

#define MAP_VERSION 11

#define PLAYER_HEIGHT_NORMAL 1.83f
#define PLAYER_HEIGHT_CROUCH 1.37f

#define LIGHTMAPPADDING 2

    typedef enum {
        ENTITY_NONE, ENTITY_MODEL, ENTITY_BRUSH, ENTITY_LIGHT, ENTITY_PLAYERSTART, ENTITY_DECAL, ENTITY_SOUND, ENTITY_PARTICLE_EMITTER, ENTITY_VIDEO_PLAYER, ENTITY_PARALLAX_ROOM, ENTITY_LOGIC, ENTITY_SPRITE
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
        bool is_static;
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
        int preset;
        float preset_time;
        int preset_index;
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
        Vec3 windDirection;
        float windStrength;
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
        bool isUnderwater;
        Vec3 underwaterColor;
    } PostProcessSettings;

    typedef struct {
        bool enabled;
        char lutPath[128];
        GLuint lutTexture;
    } ColorCorrectionSettings;

    typedef struct {
        Vec3 position;
        float yaw, pitch;
        bool isCrouching;
        float currentHeight;
        RigidBodyHandle physicsBody;
    } Camera;

    typedef struct {
        GLuint mainShader, pointDepthShader, spotDepthShader, skyboxShader;
        GLuint zPrepassShader;
        GLuint wireframeShader;
        GLuint lightingCompositeShader;
        GLuint postProcessShader;
        GLuint quadVAO, quadVBO;
        GLuint skyboxVAO, skyboxVBO;
        GLuint gBufferFBO;
        GLuint gPosition, gNormal, gLitColor, gAlbedo, gPBRParams, gVelocity;
        GLuint spriteShader;
        GLuint spriteVAO, spriteVBO;
        GLuint cloudTexture;
        GLuint brdfLUTTexture;
        GLuint decalVAO, decalVBO;
        GLuint parallaxRoomVAO, parallaxRoomVBO;
        GLuint sunShadowFBO;
        GLuint sunShadowMap;
        GLuint finalRenderFBO;
        GLuint finalRenderTexture;
        GLuint finalDepthTexture;
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
        GLuint ssaoFBO, ssaoBlurFBO;
        GLuint ssaoColorBuffer, ssaoBlurColorBuffer;
        GLuint ssaoShader, ssaoBlurShader;
        GLuint postProcessFBO;
        GLuint postProcessTexture;
        GLuint histogramShader;
        GLuint exposureShader;
        GLuint histogramSSBO;
        GLuint exposureSSBO;
        GLuint motionBlurShader;
        GLuint waterShader;
        GLuint parallaxInteriorShader;
        GLuint glassShader;
        GLuint lightSSBO;
        GLuint debugBufferShader;
        float currentExposure;
        Mat4 prevViewProjection;
    } Renderer;

    typedef struct {
        char targetname[64];
        float mass;
        float fadeStartDist;
        float fadeEndDist;
        bool isPhysicsEnabled;
        bool swayEnabled;
        char modelPath[270];
        Vec3 pos;
        Vec3 rot;
        Vec3 scale;
        Mat4 modelMatrix;
        LoadedModel* model;
        Vec4* bakedVertexColors;
        Vec4* bakedVertexDirections;
        RigidBodyHandle physicsBody;
    } SceneObject;

    typedef struct {
        Vec3 pos;
        Vec4 color;
        Vec2 lightmap_uv;
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
        Vec4 atlas_coords;
        float lightmap_scale;
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
        GLuint lightmapAtlas;
        GLuint directionalLightmapAtlas;
        int totalRenderVertexCount;
        RigidBodyHandle physicsBody;
        float mass;
        bool isPhysicsEnabled;
        bool isReflectionProbe;
        bool isWater;
        WaterDef* waterDef;
        GLuint cubemapTexture;
        char name[64];
        bool isDSP;
        ReverbPreset reverbPreset;
        bool isGlass;
        float refractionStrength;
        Material* glassNormalMap;
    } Brush;

    typedef struct {
        char targetname[64];
        Vec3 pos;
        Vec3 rot;
        Vec3 size;
        Mat4 modelMatrix;
        Material* material;
        GLuint lightmapAtlas;
        GLuint directionalLightmapAtlas;
    } Decal;

    typedef struct {
        Vec3 position;
        float yaw;
        float pitch;
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
        uint8_t* rgb_buffer;
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
        char targetname[64];
        Vec3 pos;
        float scale;
        Material* material;
        bool visible;
    } Sprite;

    typedef struct {
        char key[64];
        char value[128];
    } KeyValue;

    typedef struct {
        char targetname[64];
        char classname[64];
        Vec3 pos;
        Vec3 rot;

        KeyValue properties[MAX_ENTITY_PROPERTIES];
        int numProperties;

        bool runtime_active;
        float runtime_float_a;

    } LogicEntity;

    typedef struct {
        char mapPath[256];
        Light lights[MAX_LIGHTS];
        int numActiveLights;
        SceneObject* objects;
        int numObjects;
        Brush brushes[MAX_BRUSHES];
        int numBrushes;
        PlayerStart playerStart;
        Decal decals[MAX_DECALS];
        int numDecals;
        SoundEntity soundEntities[MAX_SOUNDS];
        int numSoundEntities;
        ParticleEmitter particleEmitters[MAX_PARTICLE_EMITTERS];
        int numParticleEmitters;
        Sprite sprites[MAX_SPRITES];
        int numSprites;
        LogicEntity logicEntities[MAX_LOGIC_ENTITIES];
        int numLogicEntities;
        VideoPlayer videoPlayers[MAX_VIDEO_PLAYERS];
        int numVideoPlayers;
        ParallaxRoom parallaxRooms[MAX_PARALLAX_ROOMS];
        int numParallaxRooms;
        Fog fog;
        PostProcessSettings post;
        Sun sun;
        bool use_cubemap_skybox;
        char skybox_path[128];
        GLuint skybox_cubemap;
        ColorCorrectionSettings colorCorrection;
        bool static_shadows_generated;
        int lightmapResolution;
    } Scene;

    typedef struct {
        SDL_Window* window;
        SDL_GLContext context;
        bool running;
        bool flashlight_on;
        float deltaTime, unscaledDeltaTime, lastFrame, scaledTime;
        Camera camera;
        PhysicsWorldHandle physicsWorld;
    } Engine;

    void Light_InitShadowMap(Light* light);
    void Calculate_Sun_Light_Space_Matrix(Mat4* outMatrix, const Sun* sun, Vec3 cameraPosition);
    void Light_DestroyShadowMap(Light* light);
    void Brush_SetVerticesFromBox(Brush* b, Vec3 size);
    void Brush_SetVerticesFromCylinder(Brush* b, Vec3 size, int num_sides);
    void Brush_SetVerticesFromWedge(Brush* b, Vec3 size);
    void Brush_SetVerticesFromSpike(Brush* b, Vec3 size, int num_sides);
    void Brush_UpdateMatrix(Brush* b);
    void Brush_CreateRenderData(Brush* b);
    void Brush_FreeData(Brush* b);
    void Brush_DeepCopy(Brush* dest, const Brush* src);
    void Brush_Clip(Brush* b, Vec3 plane_normal, float plane_d);
    void Decal_UpdateMatrix(Decal* d);
    void ParallaxRoom_UpdateMatrix(ParallaxRoom* p);
    void LogicSystem_Update(Scene* scene, float deltaTime);
    void Scene_Clear(Scene* scene, Engine* engine);
    bool Scene_LoadMap(Scene* scene, Renderer* renderer, const char* mapPath, Engine* engine);
    bool Scene_SaveMap(Scene* scene, Engine* engine, const char* mapPath);

#ifdef __cplusplus
}
#endif

#endif // MAP_H