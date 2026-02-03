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
#include "checksum.h"
#include "water_manager.h"
#include "lightmapper.h"
#include "ipc_system.h"
#include "game_data.h"
#include "gl_shadows.h"
#include "engine_commands.h"
#include "engine_api.h"
#include "animations.h"
#ifdef PLATFORM_LINUX
#include <dirent.h>
#include <sys/stat.h>
#include <dlfcn.h>
#include <sys/file.h>
#endif

int g_argc_stored = 0;
char** g_argv_stored = NULL;

bool g_screenshot_requested = false;
char g_screenshot_path[256] = { 0 };
static int g_last_deactivation_cvar_state = -1;
static int g_last_monitor_cvar_state = -1;
static bool g_sent_initial_ipc_data = false;
static int g_last_rawinput_cvar_state = -1;

bool g_player_input_disabled = false;

#ifdef PLATFORM_WINDOWS
static HANDLE g_hMutex = NULL;
#else
static int g_lockFileFd = -1;
#endif

static void init_scene(void);

Engine g_engine_instance;
Engine* g_engine = &g_engine_instance;
Renderer g_renderer;
Scene g_scene;
EngineMode g_current_mode = MODE_GAME;
EngineModeTransition g_pending_mode_transition = TRANSITION_NONE;
bool g_is_editor_mode;
bool g_quit_requested = false;
bool g_restart_requested = false;
int g_last_water_cvar_state = -1;

static Uint32 g_fps_last_update = 0;
static int g_fps_frame_count = 0;
static float g_fps_display = 0.0f;

static unsigned int g_frame_counter = 0;

unsigned int g_flashlight_sound_buffer = 0;
unsigned int g_footstep_sound_buffer = 0;
unsigned int g_jump_sound_buffer = 0;
unsigned int g_geiger_tick_sound_buffer = 0;
static float g_geiger_timer = 0.0f;
#define FPS_GRAPH_SAMPLES 8000
static float g_fps_history[FPS_GRAPH_SAMPLES] = { 0.0f };
static int g_fps_history_index = 0;
static Vec3 g_last_player_pos = { 0.0f, 0.0f, 0.0f };
static float g_distance_walked = 0.0f;
float FOOTSTEP_DISTANCE = 2.0f;
static int g_current_reverb_zone_index = -1;
static int g_last_vsync_cvar_state = -1;
static float g_current_friction_modifier = 1.0f;
static bool g_player_on_ladder = false;
static Vec3 g_ladder_normal;

const char* g_light_styles[] = {
    "m",
    "mmnmmommommnonmmonqnmmo",
    "abcdefghijklmnopqrstuvwxyzyxwvutsrqponmlkjihgfedcba",
    "mmmmmaaaaammmmmaaaaaabcdefgabcdefg",
    "mamamamamama",
    "jklmnopqrstuvwxyzyxwvutsrqponmlkj",
    "nmonqnmomnmomomno",
    "mmmaaaabcdefgmmmmaaaammmaamm",
    "mmmaaammmaaammmabcdefaaaammmmabcdefmmmaaaa",
    "aaaaaaaazzzzzzzz",
    "mmamammmmammamamaaamammma",
    "abcdefghijklmnopqrrqponmlkjihgfedcba",
    "mmnnmmnnnmmnn"
};
const int NUM_LIGHT_STYLES = sizeof(g_light_styles) / sizeof(g_light_styles[0]);

void init_engine(SDL_Window* window, SDL_GLContext context) {
    g_engine->width = g_startup_width;
    g_engine->height = g_startup_height;
    g_engine->window = window; g_engine->context = context; g_engine->running = true; g_engine->deltaTime = 0.0f; g_engine->lastFrame = 0.0f;
    g_engine->unscaledDeltaTime = 0.0f; g_engine->scaledTime = 0.0f;
    g_engine->cursor = NULL;
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
    g_engine->camera = Camera{ {0,1,5}, 0,0, false, PLAYER_HEIGHT_NORMAL, NULL, 100.0f };  g_engine->flashlight_on = false;
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
    g_engine->heldObject = NULL;
    g_engine->holdDistance = 0.0f;
    g_engine->credits_active = false;
    g_engine->credits_text = NULL;
    g_engine->credits_entity_index = -1;
    for (int i = 0; i < MAX_GAME_TEXT_MESSAGES; ++i) {
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
        char* autoexec_argv[] = { "exec", "autoexec.cfg" };
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

void process_input() {
    SDL_Event event;
    while (SDL_PollEvent(&event)) {
        if (event.type == SDL_QUIT) {
            Cvar_EngineSet("engine_running", "0");
            return;
        }
        UI_ProcessEvent(&event);
        SDL_SetCursor(g_engine->cursor);
        if (event.type == SDL_MOUSEWHEEL && g_current_mode == MODE_GAME && !Console_IsVisible()) {
            if (event.wheel.y > 0) {
                Weapons_SwitchPrev();
            }
            else if (event.wheel.y < 0) {
                Weapons_SwitchNext();
            }
        }

        if (event.type == SDL_MOUSEBUTTONDOWN && g_current_mode == MODE_GAME && !Console_IsVisible()) {
            if (event.button.button == SDL_BUTTON_LEFT) {
                if (g_engine->heldObject) {
                    Vec3 forward = { cosf(g_engine->camera.pitch) * sinf(g_engine->camera.yaw), sinf(g_engine->camera.pitch), -cosf(g_engine->camera.pitch) * cosf(g_engine->camera.yaw) };
                    vec3_normalize(&forward);

                    Physics_SetCcdEnabled(g_engine->heldObject, true, 1e-7f);
                    Physics_SetGravityEnabled(g_engine->heldObject, true);
                    Physics_ApplyCentralImpulse(g_engine->heldObject, vec3_muls(forward, 10.0f));
                    g_engine->heldObject = NULL;
                    return;
                }
                Weapons_TryFire(g_engine, &g_scene);
            }
        }
        if (g_current_mode == MODE_MAINMENU || g_current_mode == MODE_INGAMEMENU) {
            MainMenuAction action = MainMenu_HandleEvent(&event);
            if (action == MAINMENU_ACTION_START_GAME) {
                const GameConfig* config = GameConfig_Get();
                if (strlen(config->startmap) > 0 && strcmp(config->startmap, "none") != 0) {
                    char map_name_no_ext[128];
                    strncpy(map_name_no_ext, config->startmap, sizeof(map_name_no_ext) - 1);
                    map_name_no_ext[sizeof(map_name_no_ext) - 1] = '\0';

                    char* dot = strrchr(map_name_no_ext, '.');
                    if (dot) {
                        *dot = '\0';
                    }

                    char* argv[] = { (char*)"map", map_name_no_ext };
                    Commands_Execute(2, argv);
                }
                else {
                    Console_Printf_Error("No startmap defined in gameconf.txt! Cannot start game.");
                }
            }
            else if (action == MAINMENU_ACTION_CONTINUE_GAME) {
                g_current_mode = MODE_GAME;
                SDL_SetRelativeMouseMode(SDL_TRUE);
            }
            else if (action == MAINMENU_ACTION_QUIT) {
                g_quit_requested = true;
            }
        }
        else if (g_current_mode == MODE_EDITOR) {
            Editor_ProcessEvent(&event, &g_scene, g_engine);
        }

        if (event.type == SDL_KEYDOWN && event.key.repeat == 0) {
            if (event.key.keysym.sym == SDLK_e && g_current_mode == MODE_GAME && !Console_IsVisible()) {
                if (g_engine->note_active) {
                    g_engine->note_active = false;
                    g_player_input_disabled = false;
                    SDL_SetRelativeMouseMode(SDL_TRUE);
                    if (g_engine->active_note_entity_index != -1) {
                        IO_FireOutput(ENTITY_LOGIC, g_engine->active_note_entity_index, "OnRead", g_engine->lastFrame, NULL);
                    }
                    return;
                }
                if (g_engine->heldObject) {
                    Physics_SetGravityEnabled(g_engine->heldObject, true);
                    g_engine->heldObject = NULL;
                    return;
                }
                Vec3 forward = { cosf(g_engine->camera.pitch) * sinf(g_engine->camera.yaw), sinf(g_engine->camera.pitch), -cosf(g_engine->camera.pitch) * cosf(g_engine->camera.yaw) };
                vec3_normalize(&forward);

                Vec3 ray_end = vec3_add(g_engine->camera.position, vec3_muls(forward, 3.0f));

                for (int i = 0; i < g_scene.numBrushes; ++i) {
                    Brush* brush = &g_scene.brushes[i];
                    if (strcmp(brush->classname, "func_button") == 0) {
                        Vec3 brush_local_min, brush_local_max;
                        Brush_GetLocalAABB(brush, &brush_local_min, &brush_local_max);

                        float t;
                        if (RayIntersectsOBB(g_engine->camera.position, forward,
                            &brush->modelMatrix,
                            brush_local_min,
                            brush_local_max,
                            &t) && t < 3.0f) {
                            bool is_locked = (atoi(Brush_GetProperty(brush, "locked", "0")) == 1);
                            if (is_locked) {
                                IO_FireOutput(ENTITY_BRUSH, i, "OnUseLocked", g_engine->lastFrame, NULL);
                            }
                            else {
                                const char* delay_str = Brush_GetProperty(brush, "delay", "0");
                                float fire_time = g_engine->lastFrame + atof(delay_str);
                                IO_FireOutput(ENTITY_BRUSH, i, "OnPressed", fire_time, NULL);
                            }
                        }
                    }
                    if (strcmp(brush->classname, "func_door") == 0) {
                        if (atoi(Brush_GetProperty(brush, "OpenOnUse", "1")) == 1) {
                            Vec3 brush_local_min, brush_local_max;
                            Brush_GetLocalAABB(brush, &brush_local_min, &brush_local_max);

                            float t;
                            if (RayIntersectsOBB(g_engine->camera.position, forward, &brush->modelMatrix, brush_local_min, brush_local_max, &t) && t < 3.0f) {
                                if (brush->door_state == DOOR_STATE_CLOSED || brush->door_state == DOOR_STATE_CLOSING) {
                                    brush->door_state = DOOR_STATE_OPENING;
                                }
                                else if (brush->door_state == DOOR_STATE_OPEN || brush->door_state == DOOR_STATE_OPENING) {
                                    brush->door_state = DOOR_STATE_CLOSING;
                                }
                                IO_FireOutput(ENTITY_BRUSH, i, "OnUsed", g_engine->lastFrame, NULL);
                            }
                        }
                    }
                    if (strcmp(brush->classname, "func_healthcharger") == 0) {
                        Vec3 brush_local_min, brush_local_max;
                        Brush_GetLocalAABB(brush, &brush_local_min, &brush_local_max);

                        float t;
                        if (RayIntersectsOBB(g_engine->camera.position, forward, &brush->modelMatrix, brush_local_min, brush_local_max, &t) && t < 3.0f) {
                            if (g_engine->camera.health < 100.0f) {
                                const char* heal_str = Brush_GetProperty(brush, "heal_amount", "25");
                                float heal_amount = atof(heal_str);

                                g_engine->camera.health += heal_amount;
                                if (g_engine->camera.health > 100.0f) {
                                    g_engine->camera.health = 100.0f;
                                }

                                IO_FireOutput(ENTITY_BRUSH, i, "OnUse", g_engine->lastFrame, NULL);
                            }
                        }
                    }
                }

                for (int i = 0; i < g_scene.numLogicEntities; ++i) {
                    LogicEntity* ent = &g_scene.logicEntities[i];
                    if (strcmp(ent->classname, "item_note") == 0) {
                        float radius = atof(LogicEntity_GetProperty(ent, "radius", "1.0"));
                        float dist_sq = vec3_length_sq(vec3_sub(g_engine->camera.position, ent->pos));

                        Vec3 to_ent = vec3_sub(ent->pos, g_engine->camera.position);
                        vec3_normalize(&to_ent);
                        float dot = vec3_dot(forward, to_ent);

                        if (dist_sq < (radius * radius) + 4.0f && dot > 0.9f) {
                            ExecuteInput(ent->targetname, "Use", "", &g_scene, g_engine);
                            return;
                        }
                    }
                }

                RaycastHitInfo hitInfo;
                if (Physics_Raycast(g_engine->physicsWorld, g_engine->camera.position, ray_end, &hitInfo)) {
                    if (hitInfo.hitBody && Physics_GetMass(hitInfo.hitBody) < 2.0f && Physics_GetMass(hitInfo.hitBody) > 0.0f) {
                        g_engine->heldObject = hitInfo.hitBody;
                        g_engine->holdDistance = vec3_length(vec3_sub(hitInfo.point, g_engine->camera.position));
                        Physics_SetGravityEnabled(g_engine->heldObject, false);
                        Physics_SetCcdEnabled(g_engine->heldObject, false, 0.0f);
                    }
                }
            }
            if (event.key.keysym.sym == SDLK_ESCAPE) {
                if (g_engine->keypad_active) {
                    g_engine->keypad_active = false;
                    g_player_input_disabled = false;
                    SDL_SetRelativeMouseMode(SDL_TRUE);
                    return;
                }
                if (g_engine->note_active) {
                    g_engine->note_active = false;
                    g_player_input_disabled = false;
                    SDL_SetRelativeMouseMode(SDL_TRUE);
                    if (g_engine->active_note_entity_index != -1) {
                        IO_FireOutput(ENTITY_LOGIC, g_engine->active_note_entity_index, "OnRead", g_engine->lastFrame, NULL);
                    }
                    return;
                }
                if (g_current_mode == MODE_GAME) {
                    g_current_mode = MODE_INGAMEMENU;
                    bool map_is_currently_loaded = (g_scene.numObjects > 0 || g_scene.numBrushes > 0);
                    MainMenu_SetInGameMenuMode(true, map_is_currently_loaded);
                    SDL_SetRelativeMouseMode(SDL_FALSE);
                }
                else if (g_current_mode == MODE_INGAMEMENU) {
                    g_current_mode = MODE_GAME;
                    SDL_SetRelativeMouseMode(SDL_TRUE);
                }
            }
            else if (event.key.keysym.sym == SDLK_BACKQUOTE) {
                if (Cvar_GetInt("con_enable")) {
                    Console_Toggle();
                    if (g_current_mode == MODE_GAME || g_current_mode == MODE_INGAMEMENU) {
                        SDL_SetRelativeMouseMode(Console_IsVisible() ? SDL_FALSE : SDL_TRUE);
                    }
                }
            }
#ifndef GAME_RELEASE
            else if (event.key.keysym.sym == SDLK_F5) {
                if (g_current_mode != MODE_MAINMENU) {
                    char* args[] = { "edit" };
                    Commands_Execute(1, args);
                }
            }
#endif
            else if (event.key.keysym.sym == SDLK_f && g_current_mode == MODE_GAME && !Console_IsVisible()) {
                g_engine->flashlight_on = !g_engine->flashlight_on;
                SoundSystem_PlaySound(g_flashlight_sound_buffer, g_engine->camera.position, 1.0f, 1.0f, 50.0f, false);
            }
            else {
                if (g_current_mode == MODE_GAME && !Console_IsVisible()) {
                    if (event.key.keysym.sym == SDLK_1) {
                        Weapons_Switch(WEAPON_NONE);
                        continue;
                    }
                    if (event.key.keysym.sym == SDLK_2) {
                        Weapons_Switch(WEAPON_PISTOL);
                        continue;
                    }
                    const char* command = Binds_GetCommand(event.key.keysym.sym);
                    if (command) {
                        char cmd_copy[MAX_COMMAND_LENGTH];
                        strcpy(cmd_copy, command);
                        char* argv[16];
                        int argc = 0;
                        char* p = strtok(cmd_copy, " ");
                        while (p != NULL && argc < 16) {
                            argv[argc++] = p;
                            p = strtok(NULL, " ");
                        }
                        if (argc > 0) {
                            Commands_Execute(argc, argv);
                        }
                    }
                }
            }
        }

        if (g_current_mode == MODE_GAME || g_current_mode == MODE_EDITOR) {
            if (event.type == SDL_MOUSEMOTION) {
                bool can_look_in_editor = (g_current_mode == MODE_EDITOR) || (SDL_GetMouseState(NULL, NULL) & SDL_BUTTON(SDL_BUTTON_RIGHT));
                bool can_look_in_game = (g_current_mode == MODE_GAME && !Console_IsVisible() && !g_player_input_disabled);

                if (can_look_in_game || can_look_in_editor) {
                    float sensitivity = Cvar_GetFloat("sensitivity");
                    g_engine->camera.yaw += event.motion.xrel * 0.005f * sensitivity;
                    g_engine->camera.pitch -= event.motion.yrel * 0.005f * sensitivity;
                    if (g_engine->camera.pitch > 1.55f) g_engine->camera.pitch = 1.55f;
                    if (g_engine->camera.pitch < -1.55f) g_engine->camera.pitch = -1.55f;
                }
            }
        }
    }

    if (g_current_mode == MODE_GAME && !Console_IsVisible()) {
        const Uint8* state = SDL_GetKeyboardState(NULL);

        bool noclip = Cvar_GetInt("noclip");
        float speed = (noclip ? Cvar_GetFloat("g_noclip_speed") : 5.0f) * (g_engine->camera.isCrouching ? 0.5f : 1.0f);

        if (g_player_input_disabled) {
            if (!noclip) Physics_SetLinearVelocity(g_engine->camera.physicsBody, Vec3{ 0, 0, 0 });
            return;
        }

        if (noclip) {
            Vec3 forward = { cosf(g_engine->camera.pitch) * sinf(g_engine->camera.yaw), sinf(g_engine->camera.pitch), -cosf(g_engine->camera.pitch) * cosf(g_engine->camera.yaw) };
            vec3_normalize(&forward);
            Vec3 right = vec3_cross(forward, Vec3{ 0, 1, 0 });
            vec3_normalize(&right);

            if (state[SDL_SCANCODE_W]) g_engine->camera.position = vec3_add(g_engine->camera.position, vec3_muls(forward, speed * g_engine->deltaTime));
            if (state[SDL_SCANCODE_S]) g_engine->camera.position = vec3_sub(g_engine->camera.position, vec3_muls(forward, speed * g_engine->deltaTime));
            if (state[SDL_SCANCODE_D]) g_engine->camera.position = vec3_add(g_engine->camera.position, vec3_muls(right, speed * g_engine->deltaTime));
            if (state[SDL_SCANCODE_A]) g_engine->camera.position = vec3_sub(g_engine->camera.position, vec3_muls(right, speed * g_engine->deltaTime));
            if (state[SDL_SCANCODE_SPACE]) g_engine->camera.position.y += speed * g_engine->deltaTime;
            if (state[SDL_SCANCODE_LCTRL]) g_engine->camera.position.y -= speed * g_engine->deltaTime;
        }
        else if (g_player_on_ladder) {
            float ladder_speed = Cvar_GetFloat("g_speed");
            Vec3 wish_vel = { 0, 0, 0 };

            if (state[SDL_SCANCODE_W]) {
                wish_vel.y = ladder_speed;
            }
            if (state[SDL_SCANCODE_S]) {
                wish_vel.y = -ladder_speed;
            }

            Vec3 f_flat = { sinf(g_engine->camera.yaw), 0, -cosf(g_engine->camera.yaw) };
            Vec3 r_flat = { f_flat.z, 0, -f_flat.x };
            Vec3 strafe_move = { 0, 0, 0 };
            if (state[SDL_SCANCODE_A]) strafe_move = vec3_add(strafe_move, r_flat);
            if (state[SDL_SCANCODE_D]) strafe_move = vec3_sub(strafe_move, r_flat);

            vec3_normalize(&strafe_move);
            wish_vel.x = strafe_move.x * ladder_speed;
            wish_vel.z = strafe_move.z * ladder_speed;

            Vec3 horizontal_vel = { wish_vel.x, 0, wish_vel.z };
            horizontal_vel = vec3_sub(horizontal_vel, vec3_muls(g_ladder_normal, vec3_dot(horizontal_vel, g_ladder_normal)));

            Vec3 final_vel = { horizontal_vel.x, wish_vel.y, horizontal_vel.z };

            final_vel = vec3_add(final_vel, vec3_muls(g_ladder_normal, -1.0f));

            Physics_SetLinearVelocity(g_engine->camera.physicsBody, final_vel);
            Physics_Activate(g_engine->camera.physicsBody);
        }
        else {
            Vec3 f_flat = { sinf(g_engine->camera.yaw), 0, -cosf(g_engine->camera.yaw) };
            Vec3 r_flat = { f_flat.z, 0, -f_flat.x };
            Vec3 move = { 0,0,0 };

            if (state[SDL_SCANCODE_W]) move = vec3_add(move, f_flat);
            if (state[SDL_SCANCODE_S]) move = vec3_sub(move, f_flat);
            if (state[SDL_SCANCODE_A]) move = vec3_add(move, r_flat);
            if (state[SDL_SCANCODE_D]) move = vec3_sub(move, r_flat);

            vec3_normalize(&move);
            float max_wish_speed = Cvar_GetFloat("g_speed");
            if (state[SDL_SCANCODE_LSHIFT] && !g_engine->camera.isCrouching) {
                max_wish_speed = Cvar_GetFloat("g_sprint_speed");
            }
            if (g_engine->camera.isCrouching) {
                max_wish_speed *= 0.5f;
            }

            float accel = Cvar_GetFloat("g_accel");
            float friction = Cvar_GetFloat("g_friction") * g_current_friction_modifier;

            Vec3 current_vel = Physics_GetLinearVelocity(g_engine->camera.physicsBody);
            Vec3 current_vel_flat = { current_vel.x, 0, current_vel.z };

            Vec3 wish_vel = vec3_muls(move, max_wish_speed);

            Vec3 vel_delta = vec3_sub(wish_vel, current_vel_flat);

            if (vec3_length_sq(vel_delta) > 0.0001f) {
                float delta_speed = vec3_length(vel_delta);
                float add_speed = delta_speed * accel * friction * g_engine->deltaTime;

                if (add_speed > delta_speed) {
                    add_speed = delta_speed;
                }

                current_vel_flat = vec3_add(current_vel_flat, vec3_muls(vel_delta, add_speed / delta_speed));
            }

            if (vec3_length_sq(move) < 0.01f) {
                float speed = vec3_length(current_vel_flat);
                if (speed > 0.001f) {
                    float drop = speed * friction * g_engine->deltaTime;
                    float new_speed = speed - drop;
                    if (new_speed < 0) new_speed = 0;
                    current_vel_flat = vec3_muls(current_vel_flat, new_speed / speed);
                }
                else {
                    current_vel_flat = Vec3{ 0,0,0 };
                }
            }

            Physics_SetLinearVelocity(g_engine->camera.physicsBody, Vec3{ current_vel_flat.x, current_vel.y, current_vel_flat.z });
            Physics_Activate(g_engine->camera.physicsBody);

            if (state[SDL_SCANCODE_SPACE]) {
                Vec3 current_vel = Physics_GetLinearVelocity(g_engine->camera.physicsBody);
                if (current_vel.y <= 0.1f && Physics_CheckGroundContact(g_engine->physicsWorld, g_engine->camera.physicsBody, 0.1f)) {
                    Physics_ApplyCentralImpulse(g_engine->camera.physicsBody, Vec3{ 0, Cvar_GetFloat("g_jump_force"), 0 });
                    SoundSystem_PlaySound(g_jump_sound_buffer, g_engine->camera.position, 1.0f, 1.0f, 50.0f, false);
                }
            }
        }
        g_engine->camera.isCrouching = state[SDL_SCANCODE_LCTRL];
    }
}

void update_state() {
    if (IPC_IsTConsoleConnected() && !g_sent_initial_ipc_data) {
        for (int i = 0; i < Cvar_GetCount(); ++i) {
            const Cvar* c = Cvar_GetCvar(i);
            if (c && !(c->flags & CVAR_HIDDEN)) {
                char buffer[512];
                snprintf(buffer, sizeof(buffer), "register_cvar \"%s\" \"%s\" \"%s\"", c->name, c->stringValue, c->helpText);
                IPC_SendMessage(buffer);
            }
        }
        for (int i = 0; i < Commands_GetCount(); ++i) {
            const Command* cmd = Commands_GetCommand(i);
            if (cmd) {
                char buffer[512];
                snprintf(buffer, sizeof(buffer), "register_cmd \"%s\" \"%s\"", cmd->name, cmd->description);
                IPC_SendMessage(buffer);
            }
        }
        g_sent_initial_ipc_data = true;
    }
    if (!IPC_IsTConsoleConnected()) {
        g_sent_initial_ipc_data = false;
    }
    int current_rawinput_cvar = Cvar_GetInt("in_rawinput");
    if (current_rawinput_cvar != g_last_rawinput_cvar_state) {
        if (!SDL_SetHint(SDL_HINT_MOUSE_RELATIVE_MODE_WARP, current_rawinput_cvar ? "0" : "1")) {
            Console_Printf_Warning("Failed to set raw mouse input hint.");
        }
        g_last_rawinput_cvar_state = current_rawinput_cvar;
    }
    int current_monitor_cvar = Cvar_GetInt("r_monitor");
    if (current_monitor_cvar != g_last_monitor_cvar_state) {
        int num_displays = SDL_GetNumVideoDisplays();
        if (current_monitor_cvar >= 0 && current_monitor_cvar < num_displays) {
            Uint32 window_flags = SDL_GetWindowFlags(g_engine->window);
            bool is_fullscreen = (window_flags & SDL_WINDOW_FULLSCREEN_DESKTOP) || (window_flags & SDL_WINDOW_FULLSCREEN);

            SDL_Rect display_bounds;
            SDL_GetDisplayBounds(current_monitor_cvar, &display_bounds);

            if (is_fullscreen) {
                SDL_SetWindowFullscreen(g_engine->window, 0);
                SDL_SetWindowPosition(g_engine->window, display_bounds.x, display_bounds.y);
                SDL_SetWindowFullscreen(g_engine->window, SDL_WINDOW_FULLSCREEN_DESKTOP);
            }
            else {
                int w, h;
                SDL_GetWindowSize(g_engine->window, &w, &h);
                SDL_SetWindowPosition(g_engine->window,
                    display_bounds.x + (display_bounds.w - w) / 2,
                    display_bounds.y + (display_bounds.h - h) / 2);
            }
            g_last_monitor_cvar_state = current_monitor_cvar;
        }
        else {
            Cvar_Set("r_monitor", "0");
        }
    }

    g_is_unlit_mode = Cvar_GetInt("r_fullbright");
    int deactivation_cvar = Cvar_GetInt("p_disable_deactivation");
    if (deactivation_cvar != g_last_deactivation_cvar_state) {
        if (g_engine->physicsWorld) {
            Physics_SetDeactivationEnabled(g_engine->physicsWorld, deactivation_cvar == 0);
        }
        g_last_deactivation_cvar_state = deactivation_cvar;
    }
    if (g_engine->camera.health < g_engine->prev_health) {
        g_engine->red_flash_intensity = 1.0f;
        g_engine->shake_amplitude = 4.0f;
        g_engine->shake_frequency = 50.0f;
        g_engine->shake_duration_timer = 0.2f;
    }
    g_engine->prev_health = g_engine->camera.health;

    if (g_engine->red_flash_intensity > 0.0f) {
        g_engine->red_flash_intensity -= g_engine->deltaTime * 2.0f;
        if (g_engine->red_flash_intensity < 0.0f) {
            g_engine->red_flash_intensity = 0.0f;
        }
    }
    if (g_engine->shake_duration_timer > 0) {
        g_engine->shake_duration_timer -= g_engine->deltaTime;
        if (g_engine->shake_duration_timer <= 0) {
            g_engine->shake_amplitude = 0.0f;
            g_engine->shake_frequency = 0.0f;
        }
    }
    g_engine->running = Cvar_GetInt("engine_running");
    if (Cvar_GetInt("s_mute")) {
        SoundSystem_SetMasterVolume(0.0f);
    }
    else {
        SoundSystem_SetMasterVolume(Cvar_GetFloat("s_volume"));
    }
    g_engine->camera.rads_per_second = 0.0f;
    if (g_current_mode == MODE_GAME) {
        for (int i = 0; i < g_scene.numLogicEntities; ++i) {
            LogicEntity* ent = &g_scene.logicEntities[i];
            if (strcmp(ent->classname, "point_radiation_source") == 0 && ent->runtime_active) {
                float radius = atof(LogicEntity_GetProperty(ent, "radius", "500"));
                float rad_s = atof(LogicEntity_GetProperty(ent, "rad_s", "10.0"));

                float dist_sq = vec3_length_sq(vec3_sub(g_engine->camera.position, ent->pos));
                if (dist_sq < radius * radius) {
                    g_engine->camera.rads_per_second += rad_s / fmaxf(1.0f, dist_sq);
                }
            }
        }

        if (g_engine->camera.rads_per_second > 0.01f) {
            g_engine->camera.radiation_level += g_engine->camera.rads_per_second * g_engine->deltaTime;
        }
        else {
            float decay_rate = 1.5f;
            g_engine->camera.radiation_level -= decay_rate * g_engine->deltaTime;
            if (g_engine->camera.radiation_level < 0.0f) {
                g_engine->camera.radiation_level = 0.0f;
            }
        }

        if (g_engine->camera.radiation_level > 1000.0f) {
            g_engine->camera.radiation_level = 1000.0f;
        }
        if (g_engine->camera.radiation_level >= 1000.0f && Cvar_GetInt("god") == 0) {
            g_engine->camera.health = 0.0f;
        }

        if (g_engine->camera.rads_per_second > 0.1f) {
            g_geiger_timer -= g_engine->deltaTime;
            if (g_geiger_timer <= 0.0f) {
                float interval = 1.0f / (1.0f + g_engine->camera.rads_per_second * 1.5f);
                float pitch = 1.0f + (g_engine->camera.rads_per_second / 50.0f);
                pitch = fminf(pitch, 3.0f);

                SoundSystem_PlaySound(g_geiger_tick_sound_buffer, g_engine->camera.position, 0.5f, pitch, 10.0f, false);

                g_geiger_timer = interval + rand_float_range(0.0f, interval * 0.5f);
            }
        }
    }
    for (int i = 0; i < MAX_GAME_TEXT_MESSAGES; ++i) {
        GameTextMessage* msg = &g_engine->active_messages[i];
        if (msg->state == TEXT_STATE_IDLE) continue;

        msg->timer += g_engine->deltaTime;

        if (msg->state == TEXT_STATE_FADING_IN) {
            if (msg->fadeInTime > 0.0f) {
                msg->currentAlpha = fminf(1.0f, msg->timer / msg->fadeInTime);
            }
            else {
                msg->currentAlpha = 1.0f;
            }
            if (msg->timer >= msg->fadeInTime) {
                msg->state = TEXT_STATE_HOLDING;
                msg->timer = 0.0f;
            }
        }
        else if (msg->state == TEXT_STATE_HOLDING) {
            msg->currentAlpha = 1.0f;
            if (msg->timer >= msg->holdTime) {
                msg->state = TEXT_STATE_FADING_OUT;
                msg->timer = 0.0f;
            }
        }
        else if (msg->state == TEXT_STATE_FADING_OUT) {
            if (msg->fadeOutTime > 0.0f) {
                msg->currentAlpha = 1.0f - fminf(1.0f, msg->timer / msg->fadeOutTime);
            }
            else {
                msg->currentAlpha = 0.0f;
            }
            if (msg->timer >= msg->fadeOutTime) {
                msg->state = TEXT_STATE_IDLE;
            }
        }
    }
    g_engine->canUse = false;
    if (g_current_mode == MODE_GAME && !g_player_input_disabled && !Console_IsVisible()) {
        if (g_engine->heldObject == NULL) {
        Vec3 forward = { cosf(g_engine->camera.pitch) * sinf(g_engine->camera.yaw), sinf(g_engine->camera.pitch), -cosf(g_engine->camera.pitch) * cosf(g_engine->camera.yaw) };
        vec3_normalize(&forward);
        Vec3 ray_end = vec3_add(g_engine->camera.position, vec3_muls(forward, 3.0f));

        for (int i = 0; i < g_scene.numBrushes; ++i) {
            Brush* brush = &g_scene.brushes[i];
            bool is_usable = false;
            if (strcmp(brush->classname, "func_button") == 0) {
                is_usable = true;
            }
            else if (strcmp(brush->classname, "func_door") == 0) {
                if (atoi(Brush_GetProperty(brush, "OpenOnUse", "1")) == 1) {
                    is_usable = true;
                }
            }
            else if (strcmp(brush->classname, "func_healthcharger") == 0) {
                is_usable = true;
            }

            if (is_usable) {
                Vec3 brush_local_min, brush_local_max;
                Brush_GetLocalAABB(brush, &brush_local_min, &brush_local_max);

                float t;
                if (RayIntersectsOBB(g_engine->camera.position, forward, &brush->modelMatrix, brush_local_min, brush_local_max, &t) && t < 3.0f) {
                    g_engine->canUse = true;
                    break;
                }
            }
        }
        for (int i = 0; i < g_scene.numLogicEntities; ++i) {
            LogicEntity* ent = &g_scene.logicEntities[i];
            if (strcmp(ent->classname, "item_note") == 0) {
                float radius = atof(LogicEntity_GetProperty(ent, "radius", "1.0"));
                float dist_sq = vec3_length_sq(vec3_sub(g_engine->camera.position, ent->pos));
                Vec3 to_ent = vec3_sub(ent->pos, g_engine->camera.position);
                vec3_normalize(&to_ent);
                float dot = vec3_dot(forward, to_ent);

                if (dist_sq < (radius * radius) + 4.0f && dot > 0.9f) {
                    g_engine->canUse = true;
                }
            }
        }
        RaycastHitInfo hitInfo;
        if (Physics_Raycast(g_engine->physicsWorld, g_engine->camera.position, ray_end, &hitInfo)) {
            if (hitInfo.hitBody && Physics_GetMass(hitInfo.hitBody) < 2.0f && Physics_GetMass(hitInfo.hitBody) > 0.0f) {
                g_engine->canUse = true;
            }
        }
        }
    }
    IO_ProcessPendingEvents(g_engine->lastFrame, &g_scene, g_engine);
    LogicSystem_Update(&g_scene, g_engine->deltaTime);
    if (g_engine->heldObject) {
        Vec3 forward = { cosf(g_engine->camera.pitch) * sinf(g_engine->camera.yaw), sinf(g_engine->camera.pitch), -cosf(g_engine->camera.pitch) * cosf(g_engine->camera.yaw) };
        vec3_normalize(&forward);
        Vec3 targetPos = vec3_add(g_engine->camera.position, vec3_muls(forward, g_engine->holdDistance));

        Physics_Teleport(g_engine->heldObject, targetPos);
    }
    Scene_UpdateAnimations(&g_scene, g_engine->deltaTime);
    if (g_engine->active_camera_brush_index != -1) {
        Brush* cam_brush = &g_scene.brushes[g_engine->active_camera_brush_index];
        const char* target_name = Brush_GetProperty(cam_brush, "target", "");
        Vec3 target_pos;
        Vec3 target_angles;

        if (IO_FindNamedEntity(&g_scene, target_name, &target_pos, &target_angles)) {
            float moveto_time = atof(Brush_GetProperty(cam_brush, "moveto", "2.0"));
            g_engine->camera_transition_timer += g_engine->unscaledDeltaTime;

            float t = 1.0f;
            if (moveto_time > 0.0f) {
                t = fminf(g_engine->camera_transition_timer / moveto_time, 1.0f);
            }

            g_engine->camera.position = vec3_lerp(g_engine->camera_original_pos, target_pos, t);
            g_engine->camera.yaw = g_engine->camera_original_yaw + (target_angles.y * (M_PI / 180.0f) - g_engine->camera_original_yaw) * t;
            g_engine->camera.pitch = g_engine->camera_original_pitch + (target_angles.x * (M_PI / 180.0f) - g_engine->camera_original_pitch) * t;

            if (t >= 1.0f) {
                float hold_time = atof(Brush_GetProperty(cam_brush, "holdtime", "5.0"));
                if (g_engine->camera_transition_timer >= moveto_time + hold_time) {
                    ExecuteInput(cam_brush->targetname, "Disable", "", &g_scene, g_engine);
                    IO_FireOutput(ENTITY_BRUSH, g_engine->active_camera_brush_index, "OnEnd", g_engine->lastFrame, NULL);
                }
            }
        }
        else {
            ExecuteInput(cam_brush->targetname, "Disable", "", &g_scene, g_engine);
        }
    }
    if (g_engine->credits_active) {
        g_engine->credits_timer += g_engine->unscaledDeltaTime;
        if (g_engine->credits_timer >= g_engine->credits_duration) {
            IO_FireOutput(ENTITY_LOGIC, g_engine->credits_entity_index, "OnCreditsDone", g_engine->lastFrame, NULL);
            g_engine->credits_active = false;
            if (g_engine->credits_text) {
                free(g_engine->credits_text);
                g_engine->credits_text = NULL;
            }
            g_engine->credits_entity_index = -1;
        }
    }
    Weapons_Update(g_engine->deltaTime);
    for (int i = 0; i < g_scene.numActiveLights; ++i) {
        Light* light = &g_scene.lights[i];

        if (!light->is_on) {
            light->intensity = 0.0f;
        }
        else {
            const char* style = NULL;
            if (light->preset > 0 && light->preset <= 12) {
                style = g_light_styles[light->preset];
            }
            else if (light->preset == 13) {
                style = light->custom_style_string;
            }

            if (style && strlen(style) > 0) {
                int style_len = strlen(style);
                light->preset_time += g_engine->deltaTime;
                while (light->preset_time >= 0.1f) {
                    light->preset_time -= 0.1f;
                    light->preset_index = (light->preset_index + 1) % style_len;
                }

                char c = style[light->preset_index];
                float brightness = (float)(c - 'a') / (float)('m' - 'a');
                light->intensity = light->base_intensity * brightness;
            }
            else {
                light->intensity = light->base_intensity;
            }
        }

        if (light->type == LIGHT_SPOT) {
            Mat4 rot_mat = create_trs_matrix(Vec3{ 0, 0, 0 }, light->rot, Vec3{ 1, 1, 1 });
            Vec3 forward = { 0, 0, -1 };
            light->direction = mat4_mul_vec3_dir(&rot_mat, forward);
            vec3_normalize(&light->direction);
        }
    }
    if (g_current_mode == MODE_MAINMENU || g_current_mode == MODE_INGAMEMENU) {
        MainMenu_Update(g_engine->deltaTime);
        return;
    }
    if (g_current_mode == MODE_EDITOR) { Editor_Update(g_engine, &g_scene); return; }
    if (Cvar_GetInt("r_particles")) {
        float particle_cull_dist = Cvar_GetFloat("r_particles_cull_dist");
        float particle_cull_dist_sq = particle_cull_dist * particle_cull_dist;
        for (int i = 0; i < g_scene.numParticleEmitters; ++i) {
            if (vec3_length_sq(vec3_sub(g_scene.particleEmitters[i].pos, g_engine->camera.position)) < particle_cull_dist_sq) {
                ParticleEmitter_Update(&g_scene.particleEmitters[i], g_engine->deltaTime);
            }
        }
    }
    VideoPlayer_UpdateAll(&g_scene, g_engine->deltaTime);
    Vec3 playerPos;
    Physics_GetPosition(g_engine->camera.physicsBody, &playerPos);

    int new_reverb_zone_index = -1;
    for (int i = 0; i < g_scene.numBrushes; ++i) {
        Brush* b = &g_scene.brushes[i];
        if (strcmp(b->classname, "trigger_dspzone") != 0) continue;

        Vec3 min_aabb, max_aabb;
        Brush_GetWorldAABB(b, &min_aabb, &max_aabb);

        if (playerPos.x >= min_aabb.x && playerPos.x <= max_aabb.x &&
            playerPos.y >= min_aabb.y && playerPos.y <= max_aabb.y &&
            playerPos.z >= min_aabb.z && playerPos.z <= max_aabb.z)
        {
            new_reverb_zone_index = i;
            break;
        }
    }

    if (new_reverb_zone_index != g_current_reverb_zone_index) {
        g_current_reverb_zone_index = new_reverb_zone_index;
        if (new_reverb_zone_index != -1) {
            Brush* b = &g_scene.brushes[new_reverb_zone_index];
            const char* preset_str = Brush_GetProperty(b, "reverb_preset", "0");
            SoundSystem_SetCurrentReverb((ReverbPreset)atoi(preset_str));
        }
        else {
            SoundSystem_SetCurrentReverb(REVERB_PRESET_NONE);
        }
    }

    for (int i = 0; i < g_scene.numBrushes; ++i) {
        Brush* b = &g_scene.brushes[i];
        if (strlen(b->classname) == 0 || !b->runtime_active) continue;

        Vec3 min_aabb, max_aabb;
        Brush_GetWorldAABB(b, &min_aabb, &max_aabb);

        bool is_inside = (playerPos.x >= min_aabb.x && playerPos.x <= max_aabb.x &&
            playerPos.y >= min_aabb.y && playerPos.y <= max_aabb.y &&
            playerPos.z >= min_aabb.z && playerPos.z <= max_aabb.z);

        if(is_inside && !b->runtime_playerIsTouching) {
            b->runtime_playerIsTouching = true;
            if (strcmp(b->classname, "trigger_once") == 0) {
                if (!b->runtime_hasFired) {
                    const char* delay_str = Brush_GetProperty(b, "delay", "0");
                    float fire_time = g_engine->lastFrame + atof(delay_str);
                    IO_FireOutput(ENTITY_BRUSH, i, "OnStartTouch", fire_time, NULL);
                    b->runtime_hasFired = true;
                }
            }
            else if (strcmp(b->classname, "trigger_multiple") == 0) {
                const char* delay_str = Brush_GetProperty(b, "delay", "0");
                float fire_time = g_engine->lastFrame + atof(delay_str);
                IO_FireOutput(ENTITY_BRUSH, i, "OnStartTouch", fire_time, NULL);
            }
            else if (strcmp(b->classname, "trigger_teleport") == 0) {
                const char* target_name = Brush_GetProperty(b, "target", "");
                Vec3 target_pos;
                Vec3 target_angles;
                if (strlen(target_name) > 0 && IO_FindNamedEntity(&g_scene, target_name, &target_pos, &target_angles)) {
                    if (g_engine->camera.physicsBody) {
                        Physics_Teleport(g_engine->camera.physicsBody, target_pos);
                    }
                    g_engine->camera.position = target_pos;
                    g_engine->camera.yaw = target_angles.y * (M_PI / 180.0f);
                    g_engine->camera.pitch = target_angles.x * (M_PI / 180.0f);
                }
            }
            else if (strcmp(b->classname, "trigger_camera") == 0) {
                if (g_engine->active_camera_brush_index != i) {
                    ExecuteInput(b->targetname, "Enable", "", &g_scene, g_engine);
                }
            }
            else if (strcmp(b->classname, "trigger_paralyzeplayer") == 0) {
                g_player_input_disabled = true;
            }
        }
        else if (strcmp(b->classname, "trigger_autosave") == 0) {
            if (!b->runtime_hasFired) {
                char save_name[128];
                time_t now = time(NULL);
                strftime(save_name, sizeof(save_name), "autosave_%Y%m%d_%H%M%S", localtime(&now));

                char* argv[] = { (char*)"save", save_name };
                Cmd_SaveGame(2, argv);

                b->runtime_hasFired = true;
            }
        }
        else if (!is_inside && b->runtime_playerIsTouching) {
            b->runtime_playerIsTouching = false;
            if (strcmp(b->classname, "trigger_multiple") == 0 || strcmp(b->classname, "trigger_once") == 0) {
                IO_FireOutput(ENTITY_BRUSH, i, "OnEndTouch", g_engine->lastFrame, NULL);
            }
        }
        if (is_inside && strcmp(b->classname, "trigger_hurt") == 0) {
            const char* damage_str = Brush_GetProperty(b, "damage", "10");
            float damage_per_second = atof(damage_str);
            if (Cvar_GetInt("god") == 0) {
                g_engine->camera.health -= damage_per_second * g_engine->deltaTime;
            }
        }
        else if (is_inside && strcmp(b->classname, "trigger_killplayer") == 0) {
            if (Cvar_GetInt("god") == 0) {
                g_engine->camera.health = 0.0f;
            }
        }
    }
    Vec3 forward = { cosf(g_engine->camera.pitch) * sinf(g_engine->camera.yaw), sinf(g_engine->camera.pitch), -cosf(g_engine->camera.pitch) * cosf(g_engine->camera.yaw) };
    vec3_normalize(&forward); SoundSystem_UpdateListener(g_engine->camera.position, forward, Vec3{ 0, 1, 0 });
    SoundSystem_Update();
    bool noclip = Cvar_GetInt("noclip");
    if (!noclip) {
        Vec3 vel = Physics_GetLinearVelocity(g_engine->camera.physicsBody);
        bool on_ground = fabs(vel.y) < 0.1f;

        if (on_ground) {
            float dx = g_engine->camera.position.x - g_last_player_pos.x;
            float dz = g_engine->camera.position.z - g_last_player_pos.z;
            g_distance_walked += sqrtf(dx * dx + dz * dz);

            if (g_distance_walked >= FOOTSTEP_DISTANCE) {
                SoundSystem_PlaySound(g_footstep_sound_buffer, g_engine->camera.position, 0.5f, 1.0f, 50.0f, false);
                g_distance_walked = 0.0f;
            }
        }
        else {
            g_distance_walked = 0.0f;
        }

        g_last_player_pos = g_engine->camera.position;
    }
    if (g_engine->physicsWorld) {
        for (int i = 0; i < g_scene.numBrushes; ++i) {
            Brush* b = &g_scene.brushes[i];
            // This causes some problems where the player floats in the air temporarily disable
            //if (strcmp(b->classname, "func_water") == 0 && b->numVertices > 0) {
            //    Physics_ApplyBuoyancyInVolume(g_engine->physicsWorld, (const float*)b->vertices, b->numVertices, &b->modelMatrix);
            //}
        }
    }
    g_current_friction_modifier = 1.0f;
    for (int i = 0; i < g_scene.numBrushes; ++i) {
        Brush* b = &g_scene.brushes[i];
        if (strcmp(b->classname, "func_friction") == 0 && b->runtime_playerIsTouching) {
            const char* modifier_str = Brush_GetProperty(b, "modifier", "100");
            float modifier = atof(modifier_str);
            g_current_friction_modifier = modifier / 100.0f;
            break;
        }
    }
    for (int i = 0; i < g_scene.numBrushes; ++i) {
        Brush* b = &g_scene.brushes[i];
        if (strcmp(b->classname, "func_conveyor") != 0) continue;

        const char* speed_str = Brush_GetProperty(b, "speed", "0");
        float speed = atof(speed_str);
        if (speed == 0.0f) continue;

        const char* dir_str = Brush_GetProperty(b, "direction", "0 0 0");
        float pitch, yaw, roll;
        sscanf(dir_str, "%f %f %f", &pitch, &yaw, &roll);

        float pitch_rad = pitch * (M_PI / 180.0f);
        float yaw_rad = yaw * (M_PI / 180.0f);
        float roll_rad = roll * (M_PI / 180.0f);

        Mat4 rot_x_mat = mat4_rotate_x(pitch_rad);
        Mat4 rot_y_mat = mat4_rotate_y(yaw_rad);
        Mat4 rot_z_mat = mat4_rotate_z(roll_rad);
        Mat4 rot_mat;
        mat4_multiply(&rot_mat, &rot_y_mat, &rot_x_mat);
        mat4_multiply(&rot_mat, &rot_z_mat, &rot_mat);

        Vec3 base_dir = { 1, 0, 0 };
        Vec3 move_dir = mat4_mul_vec3_dir(&rot_mat, base_dir);
        Vec3 conveyor_vel = vec3_muls(move_dir, speed);

        if (b->runtime_playerIsTouching) {
            Vec3 player_vel = Physics_GetLinearVelocity(g_engine->camera.physicsBody);
            Physics_SetLinearVelocity(g_engine->camera.physicsBody, Vec3{ conveyor_vel.x, player_vel.y, conveyor_vel.z });
            Physics_Activate(g_engine->camera.physicsBody);
        }

        Vec3 conveyor_min, conveyor_max;
        if (b->numVertices > 0) {
            Brush_GetWorldAABB(b, &conveyor_min, &conveyor_max);

            for (int obj_idx = 0; obj_idx < g_scene.numObjects; ++obj_idx) {
                SceneObject* obj = &g_scene.objects[obj_idx];
                if (obj->mass > 0.0f) {
                    Vec3 obj_pos = obj->pos;
                    if (obj_pos.x > conveyor_min.x && obj_pos.x < conveyor_max.x &&
                        obj_pos.y > conveyor_min.y && obj_pos.y < conveyor_max.y + 1.0f &&
                        obj_pos.z > conveyor_min.z && obj_pos.z < conveyor_max.z)
                    {
                        Vec3 obj_vel = Physics_GetLinearVelocity(obj->physicsBody);
                        Physics_SetLinearVelocity(obj->physicsBody, Vec3{ conveyor_vel.x, obj_vel.y, conveyor_vel.z });
                        Physics_Activate(obj->physicsBody);
                    }
                }
            }
        }
    }
    g_player_on_ladder = false;
    for (int i = 0; i < g_scene.numBrushes; ++i) {
        Brush* b = &g_scene.brushes[i];
        if (strcmp(b->classname, "func_ladder") == 0 && b->runtime_playerIsTouching) {
            Vec3 forward = { cosf(g_engine->camera.pitch) * sinf(g_engine->camera.yaw), sinf(g_engine->camera.pitch), -cosf(g_engine->camera.pitch) * cosf(g_engine->camera.yaw) };
            vec3_normalize(&forward);
            Vec3 ray_end = vec3_add(g_engine->camera.position, vec3_muls(forward, 2.0f));
            RaycastHitInfo hit_info;
            if (Physics_Raycast(g_engine->physicsWorld, g_engine->camera.position, ray_end, &hit_info)) {
                g_player_on_ladder = true;
                g_ladder_normal = hit_info.normal;
                break;
            }
        }
    }
    bool in_gravity_zone = false;
    for (int i = 0; i < g_scene.numBrushes; ++i) {
        Brush* b = &g_scene.brushes[i];
        if (strcmp(b->classname, "trigger_gravity") == 0 && b->runtime_playerIsTouching) {
            float gravity_val = atof(Brush_GetProperty(b, "gravity", "9.81"));
            Physics_SetGravity(g_engine->physicsWorld, Vec3{ 0, -gravity_val, 0 });
            in_gravity_zone = true;
            break;
        }
    }

    if (!in_gravity_zone) {
        Physics_SetGravity(g_engine->physicsWorld, Vec3{ 0, -Cvar_GetFloat("gravity"), 0 });
    }
    if (g_engine->camera.health <= 0.0f) {
        g_engine->camera.health = 100.0f;
        g_engine->prev_health = g_engine->camera.health;
        Physics_Teleport(g_engine->camera.physicsBody, g_scene.playerStart.pos);
    }
    Physics_SetGravityEnabled(g_engine->camera.physicsBody, !noclip);
    if (noclip) {
        Vec3 zeroVelocity = { 0, 0, 0 };
        Physics_SetLinearVelocity(g_engine->camera.physicsBody, zeroVelocity);
    }

    if (g_engine->physicsWorld) {
        Physics_StepSimulation(g_engine->physicsWorld, g_engine->deltaTime);
    }

    if (!noclip && !g_player_input_disabled) {
        Vec3 p;
        Physics_GetPosition(g_engine->camera.physicsBody, &p);

        g_engine->camera.position.x = p.x;
        g_engine->camera.position.z = p.z;

        float target_height = g_engine->camera.isCrouching ? Cvar_GetFloat("g_crouch_height") : PLAYER_HEIGHT_NORMAL;
        float crouch_speed = 10.0f;

        g_engine->camera.currentHeight += (target_height - g_engine->camera.currentHeight) * g_engine->deltaTime * crouch_speed;

        float eyeHeightOffsetFromCenter = (g_engine->camera.currentHeight / 2.0f) * 0.85f;
        g_engine->camera.position.y = p.y + eyeHeightOffsetFromCenter;
    }

    if (!noclip) {
        Vec3 current_vel = Physics_GetLinearVelocity(g_engine->camera.physicsBody);
        bool on_ground = Physics_CheckGroundContact(g_engine->physicsWorld, g_engine->camera.physicsBody, 0.1f);

        if (on_ground && g_engine->prev_player_y_velocity < -1.0f) {
            float fall_speed = -g_engine->prev_player_y_velocity;

            const float min_fall_speed_for_damage = 10.0f;
            const float max_fall_speed_for_damage = 30.0f;
            const float max_damage = 100.0f;

            if (fall_speed > min_fall_speed_for_damage) {
                float damage_lerp_factor = (fall_speed - min_fall_speed_for_damage) / (max_fall_speed_for_damage - min_fall_speed_for_damage);
                damage_lerp_factor = fmaxf(0.0f, fminf(1.0f, damage_lerp_factor));

                float damage = damage_lerp_factor * max_damage;

                if (Cvar_GetInt("god") == 0) {
                    g_engine->camera.health -= damage;
                }
            }
        }
        g_engine->prev_player_y_velocity = current_vel.y;
    }
    for (int i = 0; i < g_scene.numBrushes; ++i) {
        Brush* b = &g_scene.brushes[i];
        if (strcmp(b->classname, "func_door") == 0) {
            if (vec3_length_sq(b->door_move_dir) < 0.001f) {
                b->door_start_pos = b->pos;
                Vec3 move_angles;
                sscanf(Brush_GetProperty(b, "direction", "0 90 0"), "%f %f %f", &move_angles.x, &move_angles.y, &move_angles.z);

                if (move_angles.x == -90) {
                    b->door_move_dir = Vec3{ 0, 1, 0 };
                }
                else if (move_angles.x == 90) {
                    b->door_move_dir = Vec3{ 0, -1, 0 };
                }
                else {
                    float yaw_rad = move_angles.y * (M_PI / 180.0f);
                    b->door_move_dir.x = cosf(yaw_rad);
                    b->door_move_dir.y = 0.0f;
                    b->door_move_dir.z = -sinf(yaw_rad);
                }

                float move_dist = atof(Brush_GetProperty(b, "distance", "0"));

                if (move_dist <= 0) {
                    Vec3 min_aabb_local, max_aabb_local;
                    Brush_GetLocalAABB(b, &min_aabb_local, &max_aabb_local);
                    Vec3 size = vec3_sub(max_aabb_local, min_aabb_local);
                    move_dist = fabsf(size.x * b->door_move_dir.x) + fabsf(size.y * b->door_move_dir.y) + fabsf(size.z * b->door_move_dir.z);
                }

                b->door_end_pos = vec3_add(b->door_start_pos, vec3_muls(b->door_move_dir, move_dist));

                if (atoi(Brush_GetProperty(b, "StartOpen", "0")) == 1) {
                    b->pos = b->door_end_pos;
                    b->door_state = DOOR_STATE_OPEN;
                }
                else {
                    b->door_state = DOOR_STATE_CLOSED;
                }
                Brush_UpdateMatrix(b);
                if (b->physicsBody) Physics_SetWorldTransform(b->physicsBody, b->modelMatrix);
            }

            float speed = atof(Brush_GetProperty(b, "speed", "100"));
            if (b->door_state == DOOR_STATE_OPENING) {
                Vec3 to_end = vec3_sub(b->door_end_pos, b->pos);
                float dist_to_end = vec3_length(to_end);
                float move_dist = speed * g_engine->deltaTime;

                if (move_dist >= dist_to_end) {
                    b->pos = b->door_end_pos;
                    b->door_state = DOOR_STATE_OPEN;
                    IO_FireOutput(ENTITY_BRUSH, i, "OnOpened", g_engine->lastFrame, NULL);
                }
                else {
                    b->pos = vec3_add(b->pos, vec3_muls(b->door_move_dir, move_dist));
                }
                Brush_UpdateMatrix(b);
                if (b->physicsBody) Physics_SetWorldTransform(b->physicsBody, b->modelMatrix);
            }
            else if (b->door_state == DOOR_STATE_CLOSING) {
                Vec3 to_start = vec3_sub(b->door_start_pos, b->pos);
                float dist_to_start = vec3_length(to_start);
                float move_dist = speed * g_engine->deltaTime;

                if (move_dist >= dist_to_start) {
                    b->pos = b->door_start_pos;
                    b->door_state = DOOR_STATE_CLOSED;
                    IO_FireOutput(ENTITY_BRUSH, i, "OnClosed", g_engine->lastFrame, NULL);
                }
                else {
                    b->pos = vec3_add(b->pos, vec3_muls(vec3_muls(b->door_move_dir, -1.0f), move_dist));
                }
                Brush_UpdateMatrix(b);
                if (b->physicsBody) Physics_SetWorldTransform(b->physicsBody, b->modelMatrix);
            }
        }
        if (strcmp(b->classname, "func_wall_toggle") == 0) {
            if (!b->runtime_hasFired) {
                b->runtime_is_visible = (atoi(Brush_GetProperty(b, "StartON", "1")) != 0);
                if (b->physicsBody) {
                    Physics_ToggleCollision(g_engine->physicsWorld, b->physicsBody, b->runtime_is_visible);
                }
                b->runtime_hasFired = true;
            }
        }
        if (strcmp(b->classname, "func_rotating") == 0 && b->runtime_active) {
            bool use_accel = atoi(Brush_GetProperty(b, "AccDcc", "0")) != 0;

            if (use_accel) {
                float friction = atof(Brush_GetProperty(b, "fanfriction", "0"));
                float accel_factor = 1.0f - (friction / 100.0f);
                float lerp_speed = 2.0f + (accel_factor * 8.0f);
                b->current_angular_velocity = vec3_lerp(Vec3{ b->current_angular_velocity, 0, 0 }, Vec3{ b->target_angular_velocity, 0, 0 }, g_engine->deltaTime* lerp_speed).x;
            }
            else {
                b->current_angular_velocity = b->target_angular_velocity;
            }

            if (fabsf(b->current_angular_velocity) > 0.001f) {
                Vec3 rotation_axis = { 0, 0, 1 };
                if (atoi(Brush_GetProperty(b, "XAxis", "0")) != 0) rotation_axis = Vec3{ 1, 0, 0 };
                if (atoi(Brush_GetProperty(b, "YAxis", "0")) != 0) rotation_axis = Vec3{ 0, 1, 0 };

                float deg_per_sec = b->current_angular_velocity;
                Vec3 delta_rot = vec3_muls(rotation_axis, deg_per_sec * g_engine->deltaTime);
                b->rot = vec3_add(b->rot, delta_rot);

                b->rot.x = fmodf(b->rot.x, 360.0f);
                b->rot.y = fmodf(b->rot.y, 360.0f);
                b->rot.z = fmodf(b->rot.z, 360.0f);

                Brush_UpdateMatrix(b);
                if (b->physicsBody) {
                    Physics_SetWorldTransform(b->physicsBody, b->modelMatrix);
                }
            }
        }
    }
    for (int i = 0; i < g_scene.numBrushes; ++i) {
        Brush* b = &g_scene.brushes[i];
        if (strcmp(b->classname, "func_plat") == 0 && b->runtime_active) {
            float speed = atof(Brush_GetProperty(b, "speed", "150"));
            float wait = atof(Brush_GetProperty(b, "wait", "3"));
            bool is_trigger = atoi(Brush_GetProperty(b, "is_trigger", "0")) != 0;

            if (!is_trigger && b->runtime_playerIsTouching && b->plat_state == PLAT_STATE_BOTTOM) {
                b->plat_state = PLAT_STATE_UP;
            }

            if (b->plat_state == PLAT_STATE_UP) {
                Vec3 to_end = vec3_sub(b->end_pos, b->pos);
                float dist_to_end = vec3_length(to_end);
                float move_dist = speed * g_engine->deltaTime;

                if (move_dist >= dist_to_end) {
                    b->pos = b->end_pos;
                    b->plat_state = PLAT_STATE_TOP;
                    b->wait_timer = wait;
                }
                else {
                    b->pos = vec3_add(b->pos, vec3_muls(b->move_dir, move_dist));
                }
            }
            else if (b->plat_state == PLAT_STATE_DOWN) {
                Vec3 to_start = vec3_sub(b->start_pos, b->pos);
                float dist_to_start = vec3_length(to_start);
                float move_dist = speed * g_engine->deltaTime;

                if (move_dist >= dist_to_start) {
                    b->pos = b->start_pos;
                    b->plat_state = PLAT_STATE_BOTTOM;
                }
                else {
                    b->pos = vec3_add(b->pos, vec3_muls(vec3_muls(b->move_dir, -1.0f), move_dist));
                }
            }
            else if (b->plat_state == PLAT_STATE_TOP) {
                if (wait > 0) {
                    b->wait_timer -= g_engine->deltaTime;
                    if (b->wait_timer <= 0) {
                        b->plat_state = PLAT_STATE_DOWN;
                    }
                }
            }

            Brush_UpdateMatrix(b);
            if (b->physicsBody) {
                Physics_SetWorldTransform(b->physicsBody, b->modelMatrix);
            }
        }
        if (strcmp(b->classname, "func_pendulum") == 0) {
            if (vec3_length_sq(b->pendulum_swing_dir) < 0.001f) {
                b->pendulum_start_pos = b->pos;
                Vec3 swing_angles;
                sscanf(Brush_GetProperty(b, "direction", "0 90 0"), "%f %f %f", &swing_angles.x, &swing_angles.y, &swing_angles.z);

                Mat4 rot_mat = create_trs_matrix(Vec3{ 0, 0, 0 }, swing_angles, Vec3{ 1, 1, 1 });
                b->pendulum_swing_dir = mat4_mul_vec3_dir(&rot_mat, Vec3{ 1, 0, 0 });
                vec3_normalize(&b->pendulum_swing_dir);

                if (atoi(Brush_GetProperty(b, "StartON", "1")) == 1) {
                    b->runtime_active = true;
                }
            }

            if (b->runtime_active) {
                float speed = atof(Brush_GetProperty(b, "speed", "1.0"));
                float distance = atof(Brush_GetProperty(b, "distance", "10.0"));

                float sine_wave_pos = sinf(g_engine->scaledTime * speed * 2.0f * M_PI);
                Vec3 offset = vec3_muls(b->pendulum_swing_dir, sine_wave_pos * distance);

                b->pos = vec3_add(b->pendulum_start_pos, offset);

                Brush_UpdateMatrix(b);
                if (b->physicsBody) {
                    Physics_SetWorldTransform(b->physicsBody, b->modelMatrix);
                }
            }
        }
        if (strcmp(b->classname, "func_weight_button") == 0) {
            if (b->physicsBody) {
                float required_weight = atof(Brush_GetProperty(b, "weight", "50"));
                float current_weight = Physics_GetTotalMassOnObject(g_engine->physicsWorld, b->physicsBody);

                bool is_pressed = current_weight >= required_weight;

                if (is_pressed && !b->runtime_was_pressed) {
                    IO_FireOutput(ENTITY_BRUSH, i, "OnPressed", g_engine->lastFrame, NULL);
                }
                else if (!is_pressed && b->runtime_was_pressed) {
                    IO_FireOutput(ENTITY_BRUSH, i, "OnReleased", g_engine->lastFrame, NULL);
                }

                b->runtime_was_pressed = is_pressed;
            }
        }
    }
    g_scene.post.isUnderwater = false;
    for (int i = 0; i < g_scene.numBrushes; ++i) {
        Brush* b = &g_scene.brushes[i];
        if (strcmp(b->classname, "func_water") != 0) continue;

        Vec3 min_aabb, max_aabb;
        Brush_GetWorldAABB(b, &min_aabb, &max_aabb);

        if (g_engine->camera.position.x >= min_aabb.x && g_engine->camera.position.x <= max_aabb.x &&
            g_engine->camera.position.y >= min_aabb.y && g_engine->camera.position.y <= max_aabb.y &&
            g_engine->camera.position.z >= min_aabb.z && g_engine->camera.position.z <= max_aabb.z)
        {
            g_scene.post.isUnderwater = true;
            g_scene.post.underwaterColor = Vec3{ 0.1f, 0.3f, 0.4f };
            break;
        }
    }
    if (g_current_mode == MODE_GAME) {
        for (int i = 0; i < g_scene.numObjects; ++i) {
            SceneObject* obj = &g_scene.objects[i];
            if (obj->physicsBody && obj->mass > 0.0f) {
                float phys_matrix_data[16];
                Physics_GetRigidBodyTransform(obj->physicsBody, phys_matrix_data);
                Mat4 physics_transform;
                memcpy(&physics_transform, phys_matrix_data, sizeof(Mat4));
                Mat4 scale_transform = mat4_scale(obj->scale);
                mat4_multiply(&obj->modelMatrix, &physics_transform, &scale_transform);
                mat4_decompose(&obj->modelMatrix, &obj->pos, &obj->rot, &obj->scale);
            }
        }
        for (int i = 0; i < g_scene.numBrushes; ++i) {
            Brush* b = &g_scene.brushes[i];
            if (b->physicsBody && b->mass > 0.0f) {
                float phys_matrix_data[16];
                Physics_GetRigidBodyTransform(b->physicsBody, phys_matrix_data);
                memcpy(&b->modelMatrix, phys_matrix_data, sizeof(Mat4));
            }
        }
    }
}

void cleanup() {
    Physics_DestroyWorld(g_engine->physicsWorld);
    for (int i = 0; i < g_scene.numParticleEmitters; i++) {
        ParticleEmitter_Free(&g_scene.particleEmitters[i]);
        ParticleSystem_Free(g_scene.particleEmitters[i].system);
    }
    for (int i = 0; i < g_scene.numParallaxRooms; ++i) {
        if (g_scene.parallaxRooms[i].cubemapTexture) glDeleteTextures(1, &g_scene.parallaxRooms[i].cubemapTexture);
    }
    for (int i = 0; i < g_scene.numActiveLights; i++) Light_DestroyShadowMap(&g_scene.lights[i]);
    for (int i = 0; i < g_scene.numBrushes; ++i) {
        if (strcmp(g_scene.brushes[i].classname, "env_reflectionprobe") == 0) {
            glDeleteTextures(1, &g_scene.brushes[i].cubemapTexture);
        }
        Brush_FreeData(&g_scene.brushes[i]);
    }
    if (g_scene.objects) {
        for (int i = 0; i < g_scene.numObjects; ++i) {
            if (g_scene.objects[i].model) Model_Free(g_scene.objects[i].model);
        }
        free(g_scene.objects);
        g_scene.objects = NULL;
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
        g_engine->cursor = NULL;
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

static int Engine_Initialize(int argc, char* argv[]) {
    GameConfig_ParseCommandLine(argc, argv);

#ifdef ENABLE_CHECKSUM
    char dllPath[1024];
#ifdef PLATFORM_WINDOWS
    HMODULE hModule = NULL;
    GetModuleHandleExA(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
        (LPCSTR)Engine_Main, &hModule);
    GetModuleFileNameA(hModule, dllPath, sizeof(dllPath));
#else
    Dl_info info;
    dladdr((void*)Engine_Main, &info);
    strncpy(dllPath, info.dli_fname, sizeof(dllPath) - 1);
    dllPath[sizeof(dllPath) - 1] = '\0';
#endif
    if (!Checksum_Verify(dllPath)) {
        SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, "Engine Protection Error", "Corrupted game files detected. Please attempt to reinstall.", NULL);
        return 0;
    }
#endif

#ifdef PLATFORM_WINDOWS
    if (!g_allow_multiple_instances) {
        const char* mutexName = "TectonicEngine_Instance_Mutex_9A4F";
        g_hMutex = CreateMutex(NULL, TRUE, mutexName);
        if (GetLastError() == ERROR_ALREADY_EXISTS) {
            SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, "Engine Already Running", "An instance of Tectonic Engine is already running.", NULL);
            if (g_hMutex) CloseHandle(g_hMutex);
            return 0;
        }
    }
#else
    if (!g_allow_multiple_instances) {
        const char* lockFilePath = "/tmp/TectonicEngine.lock";
        g_lockFileFd = open(lockFilePath, O_CREAT | O_RDWR, 0666);
        if (g_lockFileFd == -1) {
            SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, "Lock File Error", "Could not create or open the lock file.", NULL);
            return 0;
        }
        if (flock(g_lockFileFd, LOCK_EX | LOCK_NB) == -1 && errno == EWOULDBLOCK) {
            SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, "Engine Already Running", "An instance of Tectonic Engine is already running.", NULL);
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

    for (int i = 1; i < g_argc_stored; ++i) {
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
        return NULL;
    }
    if (!GL_ARB_shading_language_include) {
        SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, "GPU Feature Missing", "Your graphics card does not support glsl includes (GL_ARB_shading_language_include), which is required by this engine.", window);
        return NULL;
    }
    return context;
}

static void Engine_RenderGame() {
    char details_str[128];
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
        float shake_offset_x = rand_float_range(-1.0f, 1.0f) * g_engine->shake_amplitude * 0.015f;
        float shake_offset_y = rand_float_range(-1.0f, 1.0f) * g_engine->shake_amplitude * 0.015f;
        Mat4 shake_matrix = mat4_translate(Vec3{ shake_offset_x, shake_offset_y, 0.0f });
        mat4_multiply(&view, &shake_matrix, &view);
    }

    Vec3 velocity = Physics_GetLinearVelocity(g_engine->camera.physicsBody);
    float speed = sqrtf(velocity.x * velocity.x + velocity.z * velocity.z);
    if (speed > 0.1f) {
        float bob_cycle = g_engine->scaledTime * (Cvar_GetFloat("g_bobcycle") * 5.0f);
        float bob_amt = Cvar_GetFloat("g_bob");

        Mat4 bob_matrix;
        mat4_identity(&bob_matrix);
        bob_matrix.m[13] = -fabs(sin(bob_cycle)) * bob_amt;
        bob_matrix.m[12] = cos(bob_cycle * 2.0f) * bob_amt * 0.5f;

        mat4_multiply(&view, &view, &bob_matrix);
    }

    const Uint8* k_state = SDL_GetKeyboardState(NULL);
    float target_fov_offset = 0.0f;
    float base_fov = Cvar_GetFloat("fov_vertical");
    bool is_zoomed = k_state[SDL_SCANCODE_Z] && !Console_IsVisible();

    if (is_zoomed) {
        target_fov_offset = Cvar_GetFloat("g_zoom_fov") - base_fov;
    }
    else if (k_state[SDL_SCANCODE_LSHIFT] && !g_engine->camera.isCrouching && speed > 0.1f) {
        target_fov_offset = Cvar_GetFloat("g_sprint_fov");
    }

    float zoom_speed = Cvar_GetFloat("g_zoom_speed");
    g_engine->current_fov_offset += (target_fov_offset - g_engine->current_fov_offset) * g_engine->deltaTime * zoom_speed;

    float roll_max = Cvar_GetFloat("g_roll_angle");
    float roll_speed = Cvar_GetFloat("g_roll_speed");
    float target_roll = 0.0f;
    if (k_state[SDL_SCANCODE_A]) target_roll = roll_max;
    if (k_state[SDL_SCANCODE_D]) target_roll = -roll_max;

    g_engine->current_roll_angle += (target_roll - g_engine->current_roll_angle) * g_engine->deltaTime * roll_speed;
    Mat4 roll_mat = mat4_rotate_z(g_engine->current_roll_angle * (M_PI / 180.0f));
    mat4_multiply(&view, &roll_mat, &view);

    float fov_degrees = Cvar_GetFloat("fov_vertical");
    Mat4 projection = mat4_perspective((fov_degrees + g_engine->current_fov_offset) * (M_PI / 180.f),
        (float)g_engine->width / (float)g_engine->height, 0.1f, 1000.f);

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

    const int LOW_RES_WIDTH = g_engine->width / Cvar_GetFloat("r_geometry_downsample");
    const int LOW_RES_HEIGHT = g_engine->height / Cvar_GetFloat("r_geometry_downsample");
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

    bool debug_view_active = false;
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

    const char* texts[MAX_GAME_TEXT_MESSAGES];
    float positions_x[MAX_GAME_TEXT_MESSAGES];
    float positions_y[MAX_GAME_TEXT_MESSAGES];
    Vec4 colors[MAX_GAME_TEXT_MESSAGES];
    float alphas[MAX_GAME_TEXT_MESSAGES];
    int states[MAX_GAME_TEXT_MESSAGES];
    float scales[MAX_GAME_TEXT_MESSAGES];

    for (int i = 0; i < MAX_GAME_TEXT_MESSAGES; ++i) {
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

        float currentFrame = (float)SDL_GetTicks() / 1000.0f;
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
            g_fps_display = (float)g_fps_frame_count / ((float)(currentTicks - g_fps_last_update) / 1000.0f);
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

        if (UI_BeginPopupModal("Quit Confirmation", NULL, 1 << 3)) {
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

        int vsync_enabled = Cvar_GetInt("r_vsync");
        int fps_max = Cvar_GetInt("fps_max");
        if (vsync_enabled == 0 && fps_max > 0) {
            float targetFrameTimeMs = 1000.0f / (float)fps_max;
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

ENGINE_API int Engine_Main(int argc, char* argv[]) {
    g_argc_stored = argc;
    g_argv_stored = argv;

    if (!Engine_Initialize(g_argc_stored, g_argv_stored))
        return 1;

    SDL_Window* window = Engine_CreateWindow();
    if (!window) 
        return 1;

    SDL_GLContext context = Engine_CreateContext(window);
    if (!context) 
        return 1;

    init_engine(window, context);

    if (g_start_with_console) 
        Console_Toggle();

    if (g_dev_mode_requested) 
        Cvar_Set("developer", "1");

    Engine_RunLoop(window);

    Engine_Cleanup();

    return g_restart_requested ? 2 : 0;
}