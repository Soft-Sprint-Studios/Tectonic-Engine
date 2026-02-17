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
#include "engine.h"
#include <SDL.h>
#include <SDL_image.h>
#include <GL/glew.h>
#include <SDL_opengl.h>
#include <stdio.h>
#include <time.h>
#include "cvar.h"
#include "commands.h"
#include "gl_renderer.h"
#include "map.h"
#include "map_misc.h"
#include "physics_wrapper.h"
#include "editor.h"
#include <stdlib.h>
#include <float.h>
#include "sound_system.h"
#include "io_system.h"
#include "binds.h"
#include "gameconfig.h"
#include "discord_wrapper.h"
#include "main_menu.h"
#include "network.h"
#include "dsp_reverb.h"
#include "gl_video_player.h"
#include "gl_blackholes.h"
#include "gl_geometry.h"
#include "gl_bloom.h"
#include "gl_keypad.h"
#include "gl_note.h"
#include "gl_misc.h"
#include "gl_render_misc.h"
#include "gl_overlay.h"
#include "gl_skybox.h"
#include "gl_planar.h"
#include "gl_postprocess.h"
#include "gl_ssao.h"
#include "gl_volumetrics.h"
#include "gl_monitor.h"
#include "gl_loading_screen.h"
#include "weapons.h"
#include "sentry_wrapper.h"
#include "water_manager.h"
#include "lightmapper.h"
#include "ipc_system.h"
#include "game_data.h"
#include "gl_shadows.h"
#include "engine_commands.h"
#include "engine_game.h"
#include "engine_api.h"
#include "animations.h"
#ifdef PLATFORM_LINUX
#include <dirent.h>
#include <sys/stat.h>
#include <sys/file.h>
#include <dlfcn.h>
#endif

Int g_argc_stored = 0;
Char** g_argv_stored = nullptr;

Bool g_screenshot_requested = false;
Char g_screenshot_path[256] = { 0 };
Int g_last_deactivation_cvar_state = -1;
Int g_last_monitor_cvar_state = -1;
Bool g_sent_initial_ipc_data = false;
Int g_last_rawinput_cvar_state = -1;

Bool g_player_input_disabled = false;

#ifdef PLATFORM_WINDOWS
static HANDLE g_hMutex = nullptr;
#else
static Int g_lockFileFd = -1;
#endif

static void init_scene(void);

Engine g_engine_instance;
Engine* g_engine = &g_engine_instance;
Renderer g_renderer;
Scene g_scene;
EngineMode g_current_mode = MODE_GAME;
EngineModeTransition g_pending_mode_transition = TRANSITION_NONE;
Bool g_is_editor_mode;
Bool g_quit_requested = false;
Bool g_restart_requested = false;
Int g_last_water_cvar_state = -1;

static Uint32 g_fps_last_update = 0;
static Int g_fps_frame_count = 0;
static Float g_fps_display = 0.0f;

static Uint g_frame_counter = 0;

Uint g_flashlight_sound_buffer = 0;
Uint g_footstep_sound_buffer = 0;
Uint g_jump_sound_buffer = 0;
Uint g_geiger_tick_sound_buffer = 0;
Float g_geiger_timer = 0.0f;
constexpr int FPS_GRAPH_SAMPLES = 8000;
static Float g_fps_history[FPS_GRAPH_SAMPLES] = { 0.0f };
static Int g_fps_history_index = 0;
Vec3 g_last_player_pos = { 0.0f, 0.0f, 0.0f };
Float g_distance_walked = 0.0f;
Float FOOTSTEP_DISTANCE = 2.0f;
Int g_current_reverb_zone_index = -1;
static Int g_last_vsync_cvar_state = -1;
Float g_current_friction_modifier = 1.0f;
Bool g_player_on_ladder = false;
Vec3 g_ladder_normal;

void init_engine(SDL_Window* window, SDL_GLContext context) {
    g_engine->width = g_startup_width;
    g_engine->height = g_startup_height;
    g_engine->window = window; g_engine->context = context; g_engine->running = true; g_engine->deltaTime = 0.0f; g_engine->lastFrame = 0.0f;
    g_engine->unscaledDeltaTime = 0.0f; g_engine->scaledTime = 0.0f;
    g_engine->cursor = nullptr;
    SDL_Surface* cursor_surface = IMG_Load("media/cursor.png");
    if (cursor_surface) {
        g_engine->cursor = SDL_CreateColorCursor(cursor_surface, 0, 0);
        SDL_FreeSurface(cursor_surface);
        if (g_engine->cursor) {
            SDL_SetCursor(g_engine->cursor);
        }
        else {
            Console_Printf_Error("Failed to create cursor: %s", SDL_GetError());
        }
    }
    else {
        Console_Printf_Warning("Could not load cursor.png. Using system default cursor.");
    }
    SDL_ShowCursor(SDL_ENABLE);
    IPC_Init();
    g_engine->camera = Camera{ {0,1,5}, 0,0, false, PLAYER_HEIGHT_NORMAL, nullptr, 100.0f };  g_engine->flashlight_on = false;
    g_engine->flashlight_on = false;
    g_engine->camera.radiation_level = 0.0f;
    g_engine->camera.rads_per_second = 0.0f;
    g_engine->active_camera_brush_index = -1;
    g_player_input_disabled = false;
    g_engine->keypad_active = false;
    g_engine->active_keypad_entity_index = -1;
    memset(g_engine->keypad_input_buffer, 0, sizeof(g_engine->keypad_input_buffer));
    g_engine->note_active = false;
    g_engine->active_note_entity_index = -1;
    memset(g_engine->note_title, 0, sizeof(g_engine->note_title));
    memset(g_engine->note_content, 0, sizeof(g_engine->note_content));
    g_engine->heldObject = nullptr;
    g_engine->holdDistance = 0.0f;
    g_engine->credits_active = false;
    g_engine->credits_text = nullptr;
    g_engine->credits_entity_index = -1;
    for (Int i = 0; i < MAX_GAME_TEXT_MESSAGES; ++i) {
        g_engine->active_messages[i].state = TEXT_STATE_IDLE;
    }
    g_engine->prev_health = g_engine->camera.health;
    g_engine->red_flash_intensity = 0.0f;
    g_engine->prev_player_y_velocity = 0.0f;
    g_engine->current_fov_offset = 0.0f;
    g_engine->current_roll_angle = 0.0f;
    GameConfig_Init();
    UI_Init(window, context);
    SoundSystem_Init();
    Cvar_Init();
    Log_Init("logs.txt");
    Cvar_Init();
    Commands_Init();
    RegisterEngineCommandsAndCvars();
    Cvar_Load("cvars.txt");
    IO_Init();
    Binds_Init();
    GameData_Init("tectonic.tgd");
    Sentry_Init();
    SDL_SetHint(SDL_HINT_MOUSE_RELATIVE_MODE_WARP, Cvar_GetInt("in_rawinput") ? "0" : "1");
    g_last_rawinput_cvar_state = Cvar_GetInt("in_rawinput");
    FILE* autoexec_file = fopen("autoexec.cfg", "r");
    if (autoexec_file) {
        fclose(autoexec_file);
        Char* autoexec_argv[] = { "exec", "autoexec.cfg" };
        Commands_Execute(2, autoexec_argv);
    }
    else {
        Console_Printf_Warning("autoexec.cfg not found, skipping.");
    }
    Network_Init();
    g_flashlight_sound_buffer = SoundSystem_LoadSound("sounds/flashlight01.wav");
    g_footstep_sound_buffer = SoundSystem_LoadSound("sounds/footstep.wav");
    g_jump_sound_buffer = SoundSystem_LoadSound("sounds/jump.wav");
    g_geiger_tick_sound_buffer = SoundSystem_LoadSound("sounds/geiger_tick.wav");
    Console_SetCommandHandler(Commands_Execute);
    TextureManager_Init();
    TextureManager_ParseMaterialsFromFile("materials.def");
    Renderer_Init(&g_renderer, g_engine);
    DSP_Reverb_Thread_Init();
    init_scene();
    Discord_Init();
    Weapons_Init();
    g_current_mode = MODE_MAINMENU;
    if (!MainMenu_Init(g_engine->width, g_engine->height)) {
        Console_Printf_Error("Failed to initialize Main Menu.");
        g_engine->running = false;
    }
    LoadingScreen_Init(g_engine->width, g_engine->height);
    PrintSystemInfo();
    Console_Printf("Tectonic Engine initialized.\n");
    Console_Printf("Build: %d (%s, %s) on %s\n", Compat_GetBuildNumber(), __DATE__, __TIME__, ARCH_STRING);
    Console_Printf("Engine branch: %s\n", BRANCH_NAME);
    SDL_SetRelativeMouseMode(SDL_FALSE);
}

void init_scene() {
    memset(&g_scene, 0, sizeof(Scene));
    const GameConfig* config = GameConfig_Get();
    g_engine->camera.health = 100.0f;
    if (strlen(config->startmap) > 0 && strcmp(config->startmap, "none") != 0) {
        Scene_LoadMap(&g_scene, &g_renderer, config->startmap, g_engine);
    }
    strncpy(g_scene.mapPath, config->startmap, sizeof(g_scene.mapPath) - 1);
    g_scene.mapPath[sizeof(g_scene.mapPath) - 1] = '\0';
    g_last_player_pos = g_scene.playerStart.pos;
}

void cleanup() {
    Physics_DestroyWorld(g_engine->physicsWorld);
    for (Int i = 0; i < g_scene.numParticleEmitters; i++) {
        ParticleEmitter_Free(&g_scene.particleEmitters[i]);
        ParticleSystem_Free(g_scene.particleEmitters[i].system);
    }
    for (Int i = 0; i < g_scene.numParallaxRooms; ++i) {
        if (g_scene.parallaxRooms[i].cubemapTexture) glDeleteTextures(1, &g_scene.parallaxRooms[i].cubemapTexture);
    }
    for (Int i = 0; i < g_scene.numActiveLights; i++) Light_DestroyShadowMap(&g_scene.lights[i]);
    for (Int i = 0; i < g_scene.numBrushes; ++i) {
        if (strcmp(g_scene.brushes[i].classname, "env_reflectionprobe") == 0) {
            glDeleteTextures(1, &g_scene.brushes[i].cubemapTexture);
        }
        Brush_FreeData(&g_scene.brushes[i]);
    }
    if (g_scene.objects) {
        for (Int i = 0; i < g_scene.numObjects; ++i) {
            if (g_scene.objects[i].model) Model_Free(g_scene.objects[i].model);
        }
        delete[] g_scene.objects;
        g_scene.objects = nullptr;
    }
    Renderer_Shutdown(&g_renderer);
    WaterManager_Shutdown();
    LoadingScreen_Shutdown();
    SoundSystem_DeleteBuffer(g_flashlight_sound_buffer);
    SoundSystem_DeleteBuffer(g_footstep_sound_buffer);
    SoundSystem_DeleteBuffer(g_jump_sound_buffer);
    SoundSystem_DeleteBuffer(g_geiger_tick_sound_buffer);
    ModelLoader_Shutdown();
    TextureManager_Shutdown();
    SoundSystem_Shutdown();
    IO_Shutdown();
    Binds_Shutdown();
    Commands_Shutdown();
    Cvar_Save("cvars.txt");
    DSP_Reverb_Thread_Shutdown();
    Editor_Shutdown();
    GameData_Shutdown();
    Weapons_Shutdown();
    Network_Shutdown();
    UI_Shutdown();
    Sentry_Shutdown();
    Discord__Shutdown();
    Log_Shutdown();
    IPC_Shutdown();
    if (g_engine->cursor) {
        SDL_FreeCursor(g_engine->cursor);
        g_engine->cursor = nullptr;
    }
#ifdef PLATFORM_WINDOWS
    if (g_hMutex) {
        ReleaseMutex(g_hMutex);
        CloseHandle(g_hMutex);
    }
#else
    if (g_lockFileFd != -1) {
        ::flock(g_lockFileFd, LOCK_UN);
        close(g_lockFileFd);
    }
#endif
    SDL_GL_DeleteContext(g_engine->context);
    SDL_DestroyWindow(g_engine->window);
    IMG_Quit();
    SDL_Quit();
}

static Int Engine_Initialize(Int argc, Char* argv[]) {
    GameConfig_ParseCommandLine(argc, argv);

#ifdef PLATFORM_WINDOWS
    if (!g_allow_multiple_instances) {
        const Char* mutexName = "TectonicEngine_Instance_Mutex_9A4F";
        g_hMutex = CreateMutex(nullptr, TRUE, mutexName);
        if (GetLastError() == ERROR_ALREADY_EXISTS) {
            SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_INFORMATION, "Engine Already Running", "An instance of Tectonic Engine is already running.", nullptr);
            if (g_hMutex) CloseHandle(g_hMutex);
            return 0;
        }
    }
#else
    if (!g_allow_multiple_instances) {
        const Char* lockFilePath = "/tmp/TectonicEngine.lock";
        g_lockFileFd = open(lockFilePath, O_CREAT | O_RDWR, 0666);
        if (g_lockFileFd == -1) {
            SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, "Lock File Error", "Could not create or open the lock file.", nullptr);
            return 0;
        }
        if (flock(g_lockFileFd, LOCK_EX | LOCK_NB) == -1 && errno == EWOULDBLOCK) {
            SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_INFORMATION, "Engine Already Running", "An instance of Tectonic Engine is already running.", nullptr);
            close(g_lockFileFd);
            return 0;
        }
    }
#endif

    SDL_Init(SDL_INIT_VIDEO);
    IMG_Init(IMG_INIT_PNG | IMG_INIT_JPG);

    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 4);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 5);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);
#ifndef GAME_RELEASE
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_FLAGS, SDL_GL_CONTEXT_DEBUG_FLAG);
#endif

    PreParse_GetResolution(&g_startup_width, &g_startup_height);

    return 1;
}

static SDL_Window* Engine_CreateWindow() {
    Uint32 window_flags = SDL_WINDOW_OPENGL;
    if (g_start_fullscreen && !g_start_windowed) window_flags |= SDL_WINDOW_FULLSCREEN_DESKTOP;

    for (Int i = 1; i < g_argc_stored; ++i) {
        if (_stricmp(g_argv_stored[i], "-w") == 0 && i + 1 < g_argc_stored) g_startup_width = atoi(g_argv_stored[++i]);
        if (_stricmp(g_argv_stored[i], "-h") == 0 && i + 1 < g_argc_stored) g_startup_height = atoi(g_argv_stored[++i]);
    }

#ifdef BRANCH_NOCTURNE
    return SDL_CreateWindow("Nocturne Descent", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, g_startup_width, g_startup_height, window_flags);
#else
    return SDL_CreateWindow("Tectonic Engine", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, g_startup_width, g_startup_height, window_flags);
#endif
}

static SDL_GLContext Engine_CreateContext(SDL_Window* window) {
    SDL_GLContext context = SDL_GL_CreateContext(window);
    glewExperimental = GL_TRUE;
    glewInit();
    GL_InitDebugOutput();
    if (!GLEW_ARB_bindless_texture) {
        SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, "GPU Feature Missing", "Your graphics card does not support bindless textures (GL_ARB_bindless_texture), which is required by this engine.", window);
        return nullptr;
    }
    if (!GL_ARB_shading_language_include) {
        SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, "GPU Feature Missing", "Your graphics card does not support glsl includes (GL_ARB_shading_language_include), which is required by this engine.", window);
        return nullptr;
    }
    return context;
}

static void Engine_RenderGame() {
    Char details_str[128];
    sprintf(details_str, "Map: %s", g_scene.mapPath);
    Discord_Update("Playing", details_str);

    Vec3 forward = {
        cosf(g_engine->camera.pitch) * sinf(g_engine->camera.yaw),
        sinf(g_engine->camera.pitch),
        -cosf(g_engine->camera.pitch) * cosf(g_engine->camera.yaw)
    };
    vec3_normalize(&forward);
    Vec3 target = vec3_add(g_engine->camera.position, forward);
    Mat4 view = mat4_lookAt(g_engine->camera.position, target, Vec3{ 0, 1, 0 });

    if (g_engine->shake_amplitude > 0.0f) {
        Float shake_offset_x = rand_float_range(-1.0f, 1.0f) * g_engine->shake_amplitude * 0.015f;
        Float shake_offset_y = rand_float_range(-1.0f, 1.0f) * g_engine->shake_amplitude * 0.015f;
        Mat4 shake_matrix = mat4_translate(Vec3{ shake_offset_x, shake_offset_y, 0.0f });
        mat4_multiply(&view, &shake_matrix, &view);
    }

    Vec3 velocity = Physics_GetLinearVelocity(g_engine->camera.physicsBody);
    Float speed = sqrtf(velocity.x * velocity.x + velocity.z * velocity.z);
    if (speed > 0.1f) {
        Float bob_cycle = g_engine->scaledTime * (Cvar_GetFloat("g_bobcycle") * 5.0f);
        Float bob_amt = Cvar_GetFloat("g_bob");

        Mat4 bob_matrix;
        mat4_identity(&bob_matrix);
        bob_matrix.m[13] = -fabs(sin(bob_cycle)) * bob_amt;
        bob_matrix.m[12] = cos(bob_cycle * 2.0f) * bob_amt * 0.5f;

        mat4_multiply(&view, &view, &bob_matrix);
    }

    const Uint8* k_state = SDL_GetKeyboardState(nullptr);
    Float target_fov_offset = 0.0f;
    Float base_fov = Cvar_GetFloat("fov_vertical");
    Bool is_zoomed = k_state[SDL_SCANCODE_Z] && !Console_IsVisible();

    if (is_zoomed) {
        target_fov_offset = Cvar_GetFloat("g_zoom_fov") - base_fov;
    }
    else if (k_state[SDL_SCANCODE_LSHIFT] && !g_engine->camera.isCrouching && speed > 0.1f) {
        target_fov_offset = Cvar_GetFloat("g_sprint_fov");
    }

    Float zoom_speed = Cvar_GetFloat("g_zoom_speed");
    g_engine->current_fov_offset += (target_fov_offset - g_engine->current_fov_offset) * g_engine->deltaTime * zoom_speed;

    Float roll_max = Cvar_GetFloat("g_roll_angle");
    Float roll_speed = Cvar_GetFloat("g_roll_speed");
    Float target_roll = 0.0f;
    if (k_state[SDL_SCANCODE_A]) target_roll = roll_max;
    if (k_state[SDL_SCANCODE_D]) target_roll = -roll_max;

    g_engine->current_roll_angle += (target_roll - g_engine->current_roll_angle) * g_engine->deltaTime * roll_speed;
    Mat4 roll_mat = mat4_rotate_z(g_engine->current_roll_angle * (M_PI / 180.0f));
    mat4_multiply(&view, &roll_mat, &view);

    Float fov_degrees = Cvar_GetFloat("fov_vertical");
    Mat4 projection = mat4_perspective((fov_degrees + g_engine->current_fov_offset) * (M_PI / 180.f),
        (Float)g_engine->width / (Float)g_engine->height, 0.1f, 1000.f);

    Mat4 sunLightSpaceMatrix;
    mat4_identity(&sunLightSpaceMatrix);

    if (Cvar_GetInt("r_shadows")) {
        if ((g_frame_counter % 2) == 0) 
            Shadows_RenderPointAndSpot(&g_renderer, &g_scene, g_engine);
        if (g_scene.sun.enabled) {
            Calculate_Sun_Light_Space_Matrix(&sunLightSpaceMatrix, &g_scene.sun, g_engine->camera.position);
            if ((g_frame_counter % 2) == 0) 
                Shadows_RenderSun(&g_renderer, &g_scene, &sunLightSpaceMatrix);
        }
    }

    if (Cvar_GetInt("r_planar")) 
        Planar_RenderReflections(&g_renderer, &g_scene, g_engine, &view, &projection, &sunLightSpaceMatrix, &g_engine->camera);

    Monitor_RenderCameras(&g_scene, &g_renderer, g_engine, &sunLightSpaceMatrix);
    Geometry_RenderPass(&g_renderer, &g_scene, g_engine, &view, &projection, &sunLightSpaceMatrix, g_engine->camera.position, g_is_unlit_mode, false);

    if (Cvar_GetInt("r_water")) 
        Planar_RenderWater(&g_renderer, &g_scene, g_engine, &view, &projection, &sunLightSpaceMatrix);

    if (Cvar_GetInt("r_ssao")) 
        SSAO_RenderPass(&g_renderer, g_engine, &projection);

    if (Cvar_GetInt("r_volumetrics")) 
        Volumetrics_RenderPass(&g_renderer, &g_scene, g_engine, &view, &projection, &sunLightSpaceMatrix);

    if (Cvar_GetInt("r_bloom")) 
        Bloom_RenderPass(&g_renderer, g_engine);

    MiscRender_AutoexposurePass(&g_renderer, g_engine);

    const Int LOW_RES_WIDTH = g_engine->width / Cvar_GetFloat("r_geometry_downsample");
    const Int LOW_RES_HEIGHT = g_engine->height / Cvar_GetFloat("r_geometry_downsample");
    glBindFramebuffer(GL_READ_FRAMEBUFFER, g_renderer.gBufferFBO);
    glBindFramebuffer(GL_DRAW_FRAMEBUFFER, g_renderer.finalRenderFBO);
    glBlitFramebuffer(0, 0, LOW_RES_WIDTH, LOW_RES_HEIGHT, 0, 0, g_engine->width, g_engine->height, GL_COLOR_BUFFER_BIT, GL_LINEAR);
    glBlitFramebuffer(0, 0, LOW_RES_WIDTH, LOW_RES_HEIGHT, 0, 0, g_engine->width, g_engine->height, GL_DEPTH_BUFFER_BIT, GL_NEAREST);

    glBindFramebuffer(GL_FRAMEBUFFER, g_renderer.finalRenderFBO);

    if (Cvar_GetInt("r_skybox")) 
        Skybox_Render(&g_renderer, &g_scene, g_engine, &view, &projection);

    Blackhole_Render(&g_renderer, &g_scene, g_engine, &view, &projection);
    Planar_RenderReflectiveGlass(&g_renderer, &g_scene, g_engine, &view, &projection);
    MiscRender_RefractiveGlass(&g_renderer, &g_scene, g_engine, &view, &projection);
    Monitor_RenderBrushes(&g_scene, &g_renderer, g_engine, &view, &projection);

    GLuint read_tex = g_renderer.finalRenderTexture;
    GLuint write_fbo = g_renderer.postProcessFBO;

    PostProcess_RenderPass(&g_renderer, &g_scene, g_engine, &view, &projection, read_tex, write_fbo, g_engine->width, g_engine->height);

    GLuint source_fbo = write_fbo;
    GLuint source_tex = (source_fbo == g_renderer.finalRenderFBO) ? g_renderer.finalRenderTexture : g_renderer.postProcessTexture;

    Bool debug_view_active = false;
    if (Cvar_GetInt("r_debug_albedo")) { Renderer_RenderDebugBuffer(&g_renderer, g_engine, g_renderer.gAlbedo, 5); debug_view_active = true; }
    else if (Cvar_GetInt("r_debug_normals")) { Renderer_RenderDebugBuffer(&g_renderer, g_engine, g_renderer.gNormal, 5); debug_view_active = true; }
    else if (Cvar_GetInt("r_debug_position")) { Renderer_RenderDebugBuffer(&g_renderer, g_engine, g_renderer.gPosition, 5); debug_view_active = true; }
    else if (Cvar_GetInt("r_debug_metallic")) { Renderer_RenderDebugBuffer(&g_renderer, g_engine, g_renderer.gPBRParams, 1); debug_view_active = true; }
    else if (Cvar_GetInt("r_debug_roughness")) { Renderer_RenderDebugBuffer(&g_renderer, g_engine, g_renderer.gPBRParams, 2); debug_view_active = true; }
    else if (Cvar_GetInt("r_debug_ao")) { Renderer_RenderDebugBuffer(&g_renderer, g_engine, g_renderer.ssaoBlurColorBuffer, 1); debug_view_active = true; }
    else if (Cvar_GetInt("r_debug_velocity")) { Renderer_RenderDebugBuffer(&g_renderer, g_engine, g_renderer.gVelocity, 0); debug_view_active = true; }
    else if (Cvar_GetInt("r_debug_volumetric")) { Renderer_RenderDebugBuffer(&g_renderer, g_engine, g_renderer.volPingpongTextures[0], 0); debug_view_active = true; }
    else if (Cvar_GetInt("r_debug_bloom")) { Renderer_RenderDebugBuffer(&g_renderer, g_engine, g_renderer.bloomBrightnessTexture, 0); debug_view_active = true; }

    if (!debug_view_active) Renderer_Present(source_fbo, g_engine);

    Overlay_Render(&g_scene, g_engine);

    Mat4 currentViewProjection;
    mat4_multiply(&currentViewProjection, &projection, &view);
    g_renderer.prevViewProjection = currentViewProjection;

    const Char* texts[MAX_GAME_TEXT_MESSAGES];
    Float positions_x[MAX_GAME_TEXT_MESSAGES];
    Float positions_y[MAX_GAME_TEXT_MESSAGES];
    Vec4 colors[MAX_GAME_TEXT_MESSAGES];
    Float alphas[MAX_GAME_TEXT_MESSAGES];
    Int states[MAX_GAME_TEXT_MESSAGES];
    Float scales[MAX_GAME_TEXT_MESSAGES];

    for (Int i = 0; i < MAX_GAME_TEXT_MESSAGES; ++i) {
        texts[i] = g_engine->active_messages[i].text;
        positions_x[i] = g_engine->active_messages[i].x;
        positions_y[i] = g_engine->active_messages[i].y;
        colors[i] = g_engine->active_messages[i].color;
        alphas[i] = g_engine->active_messages[i].currentAlpha;
        states[i] = g_engine->active_messages[i].state;
        scales[i] = g_engine->active_messages[i].scale;
    }

    if (Cvar_GetInt("g_drawhud")) {
        UI_RenderGameHUD(g_renderer.stats.modelsDrawn, g_renderer.stats.totalModels,
            g_renderer.stats.brushesDrawn, g_renderer.stats.totalBrushes,
            g_fps_display, g_engine->camera.position.x,
            g_engine->camera.position.y, g_engine->camera.position.z,
            g_engine->camera.health, g_engine->canUse,
            g_engine->camera.radiation_level, g_engine->camera.rads_per_second,
            g_fps_history, FPS_GRAPH_SAMPLES);

        UI_RenderGameText(MAX_GAME_TEXT_MESSAGES, texts, positions_x, positions_y, colors, alphas, states, scales);
    }

    UI_RenderDeveloperOverlay();
    Keypad_RenderUI(&g_scene, g_engine);
    Note_RenderUI(&g_scene, g_engine);

    if (g_engine->credits_active) {
        UI_RenderCredits(g_engine->credits_active, g_engine->credits_text, g_engine->credits_timer, g_engine->credits_duration);
    }

    if (g_screenshot_requested) {
        MiscRender_SaveScreenshot(g_engine, g_screenshot_path);
        g_screenshot_requested = false;
    }
}

static void Engine_RunLoop(SDL_Window* window) {
    g_fps_last_update = SDL_GetTicks();

    while (g_engine->running) {
        Uint32 frameStartTicks = SDL_GetTicks();

        if (g_pending_mode_transition != TRANSITION_NONE) {
            if (g_pending_mode_transition == TRANSITION_TO_EDITOR) {
                g_current_mode = MODE_EDITOR;
                SDL_SetRelativeMouseMode(SDL_FALSE);
                Editor_Init(g_engine, &g_renderer, &g_scene);
            }
            else if (g_pending_mode_transition == TRANSITION_TO_GAME) {
                Editor_Shutdown();
                g_current_mode = MODE_GAME;
                SDL_SetRelativeMouseMode(SDL_TRUE);
            }
            g_pending_mode_transition = TRANSITION_NONE;
        }

        Float currentFrame = (Float)SDL_GetTicks() / 1000.0f;
        g_engine->unscaledDeltaTime = currentFrame - g_engine->lastFrame;
        g_engine->lastFrame = currentFrame;

        if (g_engine->unscaledDeltaTime > 0.0f) {
            g_fps_history[g_fps_history_index] = 1.0f / g_engine->unscaledDeltaTime;
            g_fps_history_index = (g_fps_history_index + 1) % FPS_GRAPH_SAMPLES;
        }

        g_engine->deltaTime = g_engine->unscaledDeltaTime * fmaxf(Cvar_GetFloat("timescale"), 0.0f);
        g_engine->scaledTime += g_engine->deltaTime;
        g_fps_frame_count++;

        Uint32 currentTicks = SDL_GetTicks();
        if (currentTicks - g_fps_last_update >= 1000) {
            g_fps_display = (Float)g_fps_frame_count / ((Float)(currentTicks - g_fps_last_update) / 1000.0f);
            g_fps_last_update = currentTicks;
            g_fps_frame_count = 0;
        }

        UI_BeginFrame();
        IPC_ReceiveCommands(Commands_Execute);
        process_input();
        update_state();

        switch (g_current_mode) {
        case MODE_MAINMENU:
        case MODE_INGAMEMENU:
            MainMenu_Update(g_engine->unscaledDeltaTime);
            MainMenu_Render();
            break;

        case MODE_EDITOR:
            glClear(GL_COLOR_BUFFER_BIT);
            Editor_RenderAllViewports(g_engine, &g_renderer, &g_scene);
            Editor_RenderUI(g_engine, &g_scene, &g_renderer);
            break;

        case MODE_GAME:
            Engine_RenderGame();
            break;

        default:
            break;
        }

        Console_Draw();

        if (g_quit_requested) 
            UI_OpenPopup("Quit Confirmation");

        if (UI_BeginPopupModal("Quit Confirmation", nullptr, 1 << 3)) {
            UI_Text("Are you sure you want to quit?");
            UI_Spacing();
            if (UI_Button("Quit")) 
                Cvar_EngineSet("engine_running", "0");
            UI_SameLine();
            if (UI_Button("Cancel")) {
                g_quit_requested = false;
                UI_CloseCurrentPopup();
            }
            UI_EndPopup();
        }

        g_frame_counter++;
        UI_EndFrame(window);

        Int vsync_enabled = Cvar_GetInt("r_vsync");
        Int fps_max = Cvar_GetInt("fps_max");
        if (vsync_enabled == 0 && fps_max > 0) {
            Float targetFrameTimeMs = 1000.0f / (Float)fps_max;
            Uint32 frameTicks = SDL_GetTicks() - frameStartTicks;
            if (frameTicks < targetFrameTimeMs) 
                SDL_Delay((Uint32)(targetFrameTimeMs - frameTicks));
        }
    }
}

static void Engine_Cleanup() {
    cleanup();
#ifdef PLATFORM_WINDOWS
    if (g_hMutex) CloseHandle(g_hMutex);
#else
    if (g_lockFileFd != -1) close(g_lockFileFd);
#endif
}

ENGINE_API EngineReturn Engine_Main(Int argc, Char* argv[]) {
    g_argc_stored = argc;
    g_argv_stored = argv;

    if (!Engine_Initialize(g_argc_stored, g_argv_stored))
        return ENGINE_RETURN_ERROR;

    SDL_Window* window = Engine_CreateWindow();
    if (!window) 
        return ENGINE_RETURN_ERROR;

    SDL_GLContext context = Engine_CreateContext(window);
    if (!context) 
        return ENGINE_RETURN_ERROR;

    init_engine(window, context);

    if (g_start_with_console) 
        Console_Toggle();

    if (g_dev_mode_requested) 
        Cvar_Set("developer", "1");

    Engine_RunLoop(window);

    Engine_Cleanup();

    return g_restart_requested ? ENGINE_RETURN_RESTART : ENGINE_RETURN_NONE;
}