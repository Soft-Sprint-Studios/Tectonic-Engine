/*
 * MIT License
 *
 * Copyright (c) 2025-2026 Soft Sprint Studios
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
#pragma once
#ifndef MAP_H
#define MAP_H

 //----------------------------------------//
 // Brief: Main map and renderer (will refractor)
 //----------------------------------------//

#include <SDL.h>
#include <GL/glew.h>
#include "math_lib.h"
#include "texturemanager.h"
#include "water_manager.h"
#include "model_loader.h"
#include "physics_wrapper.h"
#include "gl_particle_system.h"
#include "dsp_reverb.h"
#include <AL/al.h>


constexpr int MAX_LIGHTS = 256;
constexpr int MAX_BRUSHES = 8192;
constexpr int MAX_MODELS = 8192;
constexpr int MAX_DECALS = 8192;
constexpr int MAX_SOUNDS = 2048;
constexpr int MAX_PARTICLE_EMITTERS = 2048;
constexpr int MAX_SPRITES = 8192;
constexpr int MAX_VIDEO_PLAYERS = 32;
constexpr int MAX_PARALLAX_ROOMS = 128;
constexpr int MAX_BRUSH_VERTS = 32768;
constexpr int MAX_BRUSH_FACES = 16384;
constexpr int MAX_LOGIC_ENTITIES = 8192;
constexpr int MAX_ENTITY_PROPERTIES = 32;

constexpr int MIN_MAP_VERSION = 18;
constexpr int MAP_VERSION = 22;

constexpr float PLAYER_HEIGHT_NORMAL = 1.83f;

constexpr int LIGHTMAPPADDING = 2;

    typedef enum {
        ENTITY_NONE, ENTITY_MODEL, ENTITY_BRUSH, ENTITY_LIGHT, ENTITY_PLAYERSTART, ENTITY_DECAL, ENTITY_SOUND, ENTITY_PARTICLE_EMITTER, ENTITY_VIDEO_PLAYER, ENTITY_PARALLAX_ROOM, ENTITY_LOGIC, ENTITY_SPRITE
    } EntityType;

    typedef enum { LIGHT_POINT, LIGHT_SPOT, LIGHT_AREA } LightType;

    typedef struct {
        Char targetname[64];
        LightType type;
        Vec3 pos;
        Vec3 direction;
        Vec3 rot;
        Vec3 color;
        Float intensity;
        Float base_intensity;
        Bool is_on;
        Int is_static; // 0 = Dynamic, 1 = Static (Fully Baked), 2 = Mixed (Baked GI)
        Bool is_static_shadow;
        Bool has_rendered_static_shadow;
        Float width;
        Float height;
        Float radius;
        Float cutOff;
        Float outerCutOff;
        GLuint shadowFBO;
        GLuint shadowMapTexture;
        uint64_t shadowMapHandle;
        Char cookiePath[128];
        GLuint cookieMap;
        uint64_t cookieMapHandle;
        Float shadowFarPlane;
        Float shadowBias;
        Float volumetricIntensity;
        Int preset;
        Char custom_style_string[256];
        Float preset_time;
        Int preset_index;
        Bool isGrouped;
        Char groupName[64];
    } Light;

    typedef struct {
        Vec3 position;
        Vec3 colors[6];
        Vec3 dominant_direction;
    } AmbientProbe;

    typedef struct {
        Vec4 position;
        Vec4 direction;
        Vec4 color;
        Vec4 params1;
        Vec4 params2;
        Uint shadowMapHandle[2];
        Uint cookieMapHandle[2];
    } ShaderLight;

    typedef struct {
        Bool enabled;
        Vec3 direction;
        Vec3 color;
        Float intensity;
        Float volumetricIntensity;
        Vec3 windDirection;
        Float windStrength;
    } Sun;

    typedef struct {
        Bool enabled;
        Float crtCurvature;
        Float vignetteStrength;
        Float vignetteRadius;
        Bool lensFlareEnabled;
        Float lensFlareStrength;
        Float scanlineStrength;
        Float grainIntensity;
        Bool chromaticAberrationEnabled;
        Float chromaticAberrationStrength;
        Bool sharpenEnabled;
        Float sharpenAmount;
        Bool bwEnabled;
        Float bwStrength;
        Bool invertEnabled;
        Float invertStrength;
        Bool fade_active;
        Float fade_alpha;
        Vec3 fade_color;
        Bool isUnderwater;
        Vec3 underwaterColor;
    } PostProcessSettings;

    typedef struct {
        Bool enabled;
        Char lutPath[128];
        GLuint lutTexture;
    } ColorCorrectionSettings;

    typedef struct {
        Vec3 position;
        Float yaw, pitch;
        Bool isCrouching;
        Float currentHeight;
        RigidBodyHandle physicsBody;
        Float health;
        Float radiation_level;
        Float rads_per_second;
    } Camera;

    typedef struct {
        Int modelsDrawn;
        Int brushesDrawn;
        Int totalModels;
        Int totalBrushes;
    } RenderStats;

    typedef struct {
        GLuint mainShader, pointDepthShader, spotDepthShader, skyboxShader;
        GLuint zPrepassShader;
        GLuint zPrepassTessShader;
        GLuint wireframeShader;
        GLuint lightingCompositeShader;
        GLuint postProcessShader;
        GLuint quadVAO, quadVBO;
        GLuint skyboxVAO, skyboxVBO;
        GLuint gBufferFBO;
        GLuint gPosition, gNormal, gLitColor, gAlbedo, gPBRParams, gVelocity;
        GLuint gGeometryNormal;
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
        GLuint ssaoFBO, ssaoBlurFBO;
        GLuint ssaoColorBuffer, ssaoBlurColorBuffer;
        GLuint ssaoShader, ssaoBlurShader;
        GLuint postProcessFBO;
        GLuint postProcessTexture;
        GLuint histogramShader;
        GLuint exposureShader;
        GLuint histogramSSBO;
        GLuint exposureSSBO;
        GLuint waterShader;
        GLuint reflectiveGlassShader;
        GLuint parallaxInteriorShader;
        GLuint monitorShader;
        GLuint scanlineTexture;
        GLuint glassShader;
        GLuint lightSSBO;
        GLuint debugBufferShader;
        GLuint blackholeShader;
        GLuint reflectionFBO;
        GLuint reflectionTexture;
        GLuint reflectionDepthRBO;
        Float currentExposure;
        Mat4 prevViewProjection;
        RenderStats stats;
    } Renderer;

    typedef struct {
        Char targetname[64];
        Float mass;
        Float fadeStartDist;
        Float fadeEndDist;
        Bool isPhysicsEnabled;
        Bool swayEnabled;
        Bool casts_shadows;
        Char modelPath[270];
        Vec3 pos;
        Vec3 rot;
        Vec3 scale;
        Mat4 modelMatrix;
        LoadedModel* model;
        Vec4* bakedVertexColors;
        Vec4* bakedVertexDirections;
        Bool useLightmap;
        Float lightmapScale;
        GLuint lightmapTexture;
        GLuint dirLightmapTexture;
        uint64_t lightmapHandle;
        uint64_t dirLightmapHandle;
        Int lightmapWidth;
        Int lightmapHeight;
        RigidBodyHandle physicsBody;
        Int current_animation;
        Float animation_time;
        Bool animation_playing;
        Bool animation_looping;
        Mat4* bone_matrices;
        Mat4 animated_local_transform;
        Bool isGrouped;
        Char groupName[64];
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
        Float uv_rotation;
        Vec2 uv_offset2;
        Vec2 uv_scale2;
        Float uv_rotation2;
        Vec2 uv_offset3;
        Vec2 uv_scale3;
        Float uv_rotation3;
        Vec2 uv_offset4;
        Vec2 uv_scale4;
        Float uv_rotation4;
        Int* vertexIndices;
        Int numVertexIndices;
        Vec4 atlas_coords;
        Float lightmap_scale;
        Bool isGrouped;
        Char groupName[64];
    } BrushFace;

    typedef struct {
        Char key[64];
        Char value[1024];
    } KeyValue;

    typedef enum {
        DOOR_STATE_CLOSED,
        DOOR_STATE_OPENING,
        DOOR_STATE_OPEN,
        DOOR_STATE_CLOSING
    } DoorState;

    typedef enum {
        PLAT_STATE_TOP,
        PLAT_STATE_BOTTOM,
        PLAT_STATE_UP,
        PLAT_STATE_DOWN
    } PlatState;

    typedef struct {
        Char targetname[64];
        Vec3 pos;
        Vec3 rot;
        Vec3 scale;
        Mat4 modelMatrix;
        BrushVertex* vertices;
        Int numVertices;
        BrushFace* faces;
        Int numFaces;
        GLuint vao, vbo;
        GLuint lightmapAtlas;
        uint64_t lightmapAtlasHandle;
        GLuint directionalLightmapAtlas;
        uint64_t directionalLightmapAtlasHandle;
        Vec2 lightmap_atlas_size;
        Int totalRenderVertexCount;
        RigidBodyHandle physicsBody;
        Float mass;
        Bool isPhysicsEnabled;
        GLuint cubemapTexture;
        Char name[64];
        Bool isGrouped;
        Char groupName[64];
        Char classname[64];
        KeyValue properties[MAX_ENTITY_PROPERTIES];
        Int numProperties;

        DoorState door_state;
        Vec3 door_start_pos;
        Vec3 door_end_pos;
        Vec3 door_move_dir;
        Vec3 pendulum_start_pos;
        Vec3 pendulum_swing_dir;
        Bool runtime_is_visible;
        Bool runtime_was_pressed;
        Bool runtime_playerIsTouching;
        Bool runtime_hasFired;
        Bool runtime_active;
        Float current_angular_velocity;
        Float target_angular_velocity;
        Vec3 start_pos;
        Vec3 end_pos;
        Vec3 move_dir;
        PlatState plat_state;
        Float wait_timer;
        Bool useVertexLighting;
        Bool casts_shadows;
        Vec4* bakedVertexColors;
        Vec4* bakedVertexDirections;
    } Brush;

    typedef struct {
        Char targetname[64];
        Vec3 pos;
        Vec3 rot;
        Vec3 size;
        Mat4 modelMatrix;
        Material* material;
        GLuint lightmapAtlas;
        GLuint directionalLightmapAtlas;
        Float lightmap_scale;
        Vec2 uv_offset;
        Vec2 uv_scale;
        Float uv_rotation;
        Bool isGrouped;
        Char groupName[64];
    } Decal;

    typedef struct {
        Vec3 pos;
        Float yaw;
        Float pitch;
    } PlayerStart;

    typedef struct {
        Char targetname[64];
        Char soundPath[128];
        Vec3 pos;
        Uint bufferID;
        Uint sourceID;
        Float volume;
        Float pitch;
        Float maxDistance;
        Bool is_looping;
        Bool play_on_start;
        Bool isGlobal;
        Bool isGrouped;
        Char groupName[64];
    } SoundEntity;

    typedef enum {
        VP_STOPPED,
        VP_PLAYING,
        VP_PAUSED
    } VideoPlayerState;

    typedef struct plm_t plm_t;

    typedef struct {
        Char targetname[64];
        Char videoPath[128];
        Vec3 pos;
        Vec3 rot;
        Vec2 size;
        Mat4 modelMatrix;

        Bool playOnStart;
        Bool loop;
        VideoPlayerState state;

        plm_t* plm;
        GLuint textureY;
        GLuint textureCb;
        GLuint textureCr;
        ALuint audioSource;
        ALuint audioBuffers[4];
        Double time;
        Double nextFrameTime;
        Bool isGrouped;
        Char groupName[64];
    } VideoPlayer;

    typedef struct {
        Char targetname[64];
        Char cubemapPath[128];
        Vec3 pos;
        Vec3 rot;
        Vec2 size;
        Float roomDepth;
        Mat4 modelMatrix;
        GLuint cubemapTexture;
        Bool isGrouped;
        Char groupName[64];
    } ParallaxRoom;

    typedef struct ParticleEmitter {
        Char parFile[128];
        Char targetname[64];
        Bool is_on;
        Bool on_by_default;
        ParticleSystem* system;
        Vec3 pos;
        Particle particles[MAX_PARTICLES_PER_SYSTEM];
        Int activeParticles;
        Float timeSinceLastSpawn;
        GLuint vao;
        GLuint vbo;
        Bool isGrouped;
        Char groupName[64];
    } ParticleEmitter;

    typedef struct {
        Char targetname[64];
        Vec3 pos;
        Float scale;
        Material* material;
        Bool visible;
        Bool isGrouped;
        Char groupName[64];
    } Sprite;

    typedef struct {
        Char targetname[64];
        Char classname[64];
        Vec3 pos;
        Vec3 rot;

        KeyValue properties[MAX_ENTITY_PROPERTIES];
        Int numProperties;

        Bool runtime_active;
        Float runtime_float_a;
        Int runtime_int_a;
        Float runtime_float_b;
        Bool isGrouped;
        Char groupName[64];

        // monitor stuff
        GLuint monitor_fbo;
        GLuint monitor_texture;
        GLuint monitor_depth;
        Int monitor_resolution;
        Bool monitor_render_once;
        Bool monitor_has_rendered;
        Float monitor_fov;

    } LogicEntity;

    typedef struct {
        Char mapPath[256];
        Char originalMapPath[256];
        Light lights[MAX_LIGHTS];
        Int numActiveLights;
        SceneObject* objects;
        Int numObjects;
        Brush brushes[MAX_BRUSHES];
        Int numBrushes;
        PlayerStart playerStart;
        Decal decals[MAX_DECALS];
        Int numDecals;
        SoundEntity soundEntities[MAX_SOUNDS];
        Int numSoundEntities;
        ParticleEmitter particleEmitters[MAX_PARTICLE_EMITTERS];
        Int numParticleEmitters;
        Sprite sprites[MAX_SPRITES];
        Int numSprites;
        LogicEntity logicEntities[MAX_LOGIC_ENTITIES];
        Int numLogicEntities;
        VideoPlayer videoPlayers[MAX_VIDEO_PLAYERS];
        Int numVideoPlayers;
        ParallaxRoom parallaxRooms[MAX_PARALLAX_ROOMS];
        Int numParallaxRooms;
        PostProcessSettings post;
        Sun sun;
        Bool use_cubemap_skybox;
        Char skybox_path[128];
        GLuint skybox_cubemap;
        ColorCorrectionSettings colorCorrection;
        Bool static_shadows_generated;
        Int lightmapResolution;
        AmbientProbe* ambient_probes;
        Int num_ambient_probes;
    } Scene;

    constexpr int MAX_GAME_TEXT_MESSAGES = 4;

    typedef enum {
        TEXT_STATE_IDLE,
        TEXT_STATE_FADING_IN,
        TEXT_STATE_HOLDING,
        TEXT_STATE_FADING_OUT
    } GameTextState;

    typedef struct {
        Char text[256];
        Float x, y;
        Vec4 color;
        Float scale;
        Float fadeInTime;
        Float fadeOutTime;
        Float holdTime;
        Int channel;

        GameTextState state;
        Float timer;
        Float currentAlpha;
    } GameTextMessage;

    typedef struct {
        SDL_Window* window;
        SDL_GLContext context;
        SDL_Cursor* cursor;
        Bool running;
        Bool flashlight_on;
        Float shake_amplitude;
        Float shake_frequency;
        Float shake_duration_timer;
        Float red_flash_intensity;
        Float prev_health;
        Float prev_player_y_velocity;
        Float deltaTime, unscaledDeltaTime, lastFrame, scaledTime;
        Float current_fov_offset;
        Float current_roll_angle;
        Camera camera;
        PhysicsWorldHandle physicsWorld;
        Int width;
        Int height;
        Bool canUse;
        Int active_camera_brush_index;
        Float camera_transition_timer;
        Vec3 camera_original_pos;
        Float camera_original_yaw;
        Float camera_original_pitch;
        Bool keypad_active;
        Int active_keypad_entity_index;
        Char keypad_input_buffer[32];
        Bool note_active;
        Int active_note_entity_index;
        Char note_title[64];
        Char note_content[1024];
        RigidBodyHandle heldObject;
        Float holdDistance;
        Bool credits_active;
        Char* credits_text;
        Float credits_scroll_y;
        Float credits_duration;
        Float credits_timer;
        Int credits_entity_index;
        GameTextMessage active_messages[MAX_GAME_TEXT_MESSAGES];
    } Engine;

    void LogicSystem_Update(Scene* scene, Float deltaTime);
    void Scene_Clear(Scene* scene, Engine* engine);
    Bool Scene_LoadMap(Scene* scene, Renderer* renderer, const Char* mapPath, Engine* engine);
    Bool Scene_SaveMap(Scene* scene, Engine* engine, const Char* mapPath);


#endif // MAP_H