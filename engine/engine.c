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
#include <SDL.h>
#include <SDL_image.h>
#include <GL/glew.h>
#include <SDL_opengl.h>
#include <stdio.h>
#include <stdbool.h>
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
#include "weapons.h"
#include "sentry_wrapper.h"
#include "checksum.h"
#include "water_manager.h"
#include "lightmapper.h"
#include "ipc_system.h"
#include "game_data.h"
#include "gl_shadows.h"
#include "engine_api.h"
#include "cgltf/cgltf.h"
#ifdef PLATFORM_LINUX
#include <dirent.h>
#include <sys/stat.h>
#include <dlfcn.h>
#endif

static const char* g_light_styles[] = {
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

static bool g_screenshot_requested = false;
static char g_screenshot_path[256] = { 0 };
static int g_last_deactivation_cvar_state = -1;

bool g_player_input_disabled = false;

static bool g_start_fullscreen = false;
static bool g_start_windowed = false;
static bool g_start_with_console = false;
static bool g_dev_mode_requested = false;

static int g_startup_width = 1920;
static int g_startup_height = 1080;

static void SaveFramebufferToPNG(GLuint fbo, int width, int height, const char* filepath);
void BuildCubemaps();

#ifdef PLATFORM_WINDOWS
static HANDLE g_hMutex = NULL;
#else
static int g_lockFileFd = -1;
#endif

static void init_scene(void);

static void Scene_UpdateAnimations(Scene* scene, float deltaTime);

typedef enum { MODE_GAME, MODE_EDITOR, MODE_MAINMENU, MODE_INGAMEMENU } EngineMode;

static Engine g_engine_instance;
static Engine* g_engine = &g_engine_instance;
static Renderer g_renderer;
static Scene g_scene;
static EngineMode g_current_mode = MODE_GAME;

typedef enum {
    TRANSITION_NONE,
    TRANSITION_TO_EDITOR,
    TRANSITION_TO_GAME
} EngineModeTransition;
static EngineModeTransition g_pending_mode_transition = TRANSITION_NONE;
static int g_last_water_cvar_state = -1;

static Uint32 g_fps_last_update = 0;
static int g_fps_frame_count = 0;
static float g_fps_display = 0.0f;

static unsigned int g_frame_counter = 0;

static unsigned int g_flashlight_sound_buffer = 0;
static unsigned int g_footstep_sound_buffer = 0;
static unsigned int g_jump_sound_buffer = 0;
#define FPS_GRAPH_SAMPLES 14400
static float g_fps_history[FPS_GRAPH_SAMPLES] = { 0.0f };
static int g_fps_history_index = 0;
static Vec3 g_last_player_pos = { 0.0f, 0.0f, 0.0f };
static float g_distance_walked = 0.0f;
const float FOOTSTEP_DISTANCE = 2.0f;
static int g_current_reverb_zone_index = -1;
static int g_last_vsync_cvar_state = -1;
static float g_current_friction_modifier = 1.0f;
static bool g_player_on_ladder = false;
static Vec3 g_ladder_normal;

float quadVertices[] = { -1.0f,1.0f,0.0f,1.0f,-1.0f,-1.0f,0.0f,0.0f,1.0f,-1.0f,1.0f,0.0f,-1.0f,1.0f,0.0f,1.0f,1.0f,-1.0f,1.0f,0.0f,1.0f,1.0f,1.0f,1.0f };
float parallaxRoomVertices[] = {
    -0.5f,  0.5f, 0.0f,  0.0f, 0.0f, 1.0f,   0.0f, 1.0f,   1.0f, 0.0f, 0.0f, 0.0f,
    -0.5f, -0.5f, 0.0f,  0.0f, 0.0f, 1.0f,   0.0f, 0.0f,   1.0f, 0.0f, 0.0f, 0.0f,
     0.5f, -0.5f, 0.0f,  0.0f, 0.0f, 1.0f,   1.0f, 0.0f,   1.0f, 0.0f, 0.0f, 0.0f,

    -0.5f,  0.5f, 0.0f,  0.0f, 0.0f, 1.0f,   0.0f, 1.0f,   1.0f, 0.0f, 0.0f, 0.0f,
     0.5f, -0.5f, 0.0f,  0.0f, 0.0f, 1.0f,   1.0f, 0.0f,   1.0f, 0.0f, 0.0f, 0.0f,
     0.5f,  0.5f, 0.0f,  0.0f, 0.0f, 1.0f,   1.0f, 1.0f,   1.0f, 0.0f, 0.0f, 0.0f
};

void handle_command(int argc, char** argv) {
    Commands_Execute(argc, argv);
}

void Cmd_Edit(int argc, char** argv) {
    if (g_current_mode == MODE_GAME) {
        g_last_water_cvar_state = Cvar_GetInt("r_water");
        Cvar_Set("r_water", "0");
        g_pending_mode_transition = TRANSITION_TO_EDITOR;
    }
    else if (g_current_mode == MODE_EDITOR) {
        if (g_last_water_cvar_state != -1) {
            char val[2];
            sprintf(val, "%d", g_last_water_cvar_state);
            Cvar_Set("r_water", val);
        }
        g_pending_mode_transition = TRANSITION_TO_GAME;
    }
}

void Cmd_Quit(int argc, char** argv) {
    Cvar_EngineSet("engine_running", "0");
}

void Cmd_SetPos(int argc, char** argv) {
    if (argc == 4) {
        float x = atof(argv[1]);
        float y = atof(argv[2]);
        float z = atof(argv[3]);
        Vec3 new_pos = { x, y, z };
        if (g_engine->camera.physicsBody) {
            Physics_Teleport(g_engine->camera.physicsBody, new_pos);
        }
        g_engine->camera.position = new_pos;
        Console_Printf("Teleported to %.2f, %.2f, %.2f", x, y, z);
    }
    else {
        Console_Printf("Usage: setpos <x> <y> <z>");
    }
}

void Cmd_Noclip(int argc, char** argv) {
    Cvar* c = Cvar_Find("noclip");
    if (c) {
        bool currently_noclip = c->intValue;
        Cvar_Set("noclip", currently_noclip ? "0" : "1");
        Console_Printf("noclip %s", Cvar_GetString("noclip"));
        if (currently_noclip) {
            Physics_Teleport(g_engine->camera.physicsBody, g_engine->camera.position);
        }
    }
}

void Cmd_Bind(int argc, char** argv) {
    if (argc == 3) {
        Binds_Set(argv[1], argv[2]);
    }
    else {
        Console_Printf("Usage: bind \"key\" \"command\"");
    }
}

void Cmd_Unbind(int argc, char** argv) {
    if (argc == 2) {
        Binds_Unset(argv[1]);
    }
    else {
        Console_Printf("Usage: unbind \"key\"");
    }
}

void Cmd_UnbindAll(int argc, char** argv) {
    Binds_UnbindAll();
}

void Cmd_Map(int argc, char** argv) {
    if (argc == 2) {
        g_current_mode = MODE_MAINMENU;
        SDL_SetRelativeMouseMode(SDL_FALSE);
        char map_path[256];
        snprintf(map_path, sizeof(map_path), "%s.map", argv[1]);
        Console_Printf("Loading map: %s", map_path);
        if (Scene_LoadMap(&g_scene, &g_renderer, map_path, g_engine)) {
            g_current_mode = MODE_GAME;
            SDL_SetRelativeMouseMode(SDL_TRUE);
        }
        else {
            Console_Printf_Error("[error] Failed to load map: %s", map_path);
        }
    }
    else {
        Console_Printf("Usage: map <mapname>");
    }
}

void Cmd_Maps(int argc, char** argv) {
    const char* dir_path = "./";
    Console_Printf("Available maps in root directory:");
#ifdef PLATFORM_WINDOWS
    char search_path[256];
    sprintf(search_path, "%s*.map", dir_path);
    WIN32_FIND_DATAA find_data;
    HANDLE h_find = FindFirstFileA(search_path, &find_data);
    if (h_find == INVALID_HANDLE_VALUE) {
        Console_Printf("...No maps found.");
    }
    else {
        do {
            if (!(find_data.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)) {
                Console_Printf("  %s", find_data.cFileName);
            }
        } while (FindNextFileA(h_find, &find_data) != 0);
        FindClose(h_find);
    }
#else
    DIR* d = opendir(dir_path);
    if (!d) {
        Console_Printf("...Could not open directory.");
    }
    else {
        struct dirent* dir;
        int count = 0;
        while ((dir = readdir(d)) != NULL) {
            const char* ext = strrchr(dir->d_name, '.');
            if (ext && (_stricmp(ext, ".map") == 0)) {
                Console_Printf("  %s", dir->d_name);
                count++;
            }
        }
        if (count == 0) {
            Console_Printf("...No maps found.");
        }
        closedir(d);
    }
#endif
}

void Cmd_Disconnect(int argc, char** argv) {
    if (g_current_mode == MODE_GAME || g_current_mode == MODE_EDITOR) {
        Console_Printf("Disconnecting from map...");
        g_current_mode = MODE_MAINMENU;
        SDL_SetRelativeMouseMode(SDL_FALSE);
        if (g_is_editor_mode) {
            Editor_Shutdown();
        }
        Scene_Clear(&g_scene, g_engine);
        MainMenu_SetInGameMenuMode(false, false);
    }
    else {
        Console_Printf("Not currently in a map.");
    }
}

void Cmd_Download(int argc, char** argv) {
    if (argc == 2 && strncmp(argv[1], "http", 4) == 0) {
        const char* url = argv[1];
        const char* filename_start = strrchr(url, '/');
        if (filename_start) {
            filename_start++;
        }
        else {
            filename_start = url;
        }
        _mkdir("downloads");
        char output_path[256];
        snprintf(output_path, sizeof(output_path), "downloads/%s", filename_start);
        Console_Printf("Starting download for %s...", url);
        Network_DownloadFile(url, output_path);
    }
    else {
        Console_Printf("Usage: download http://... or https://...");
    }
}

void Cmd_Ping(int argc, char** argv) {
    if (argc == 2) {
        Console_Printf("Pinging %s...", argv[1]);
        Network_Ping(argv[1]);
    }
    else {
        Console_Printf("Usage: ping <hostname>");
    }
}

void Cmd_BuildCubemaps(int argc, char** argv) {
    int resolution = 256;
    if (argc > 1) {
        int res_arg = atoi(argv[1]);
        if (res_arg > 0 && (res_arg & (res_arg - 1)) == 0) {
            resolution = res_arg;
        }
        else {
            Console_Printf_Warning("[WARNING] Invalid cubemap resolution '%s'. Must be a power of two. Using default 256.", argv[1]);
        }
    }
    BuildCubemaps(resolution);
}

void Cmd_Screenshot(int argc, char** argv) {
    if (g_screenshot_requested) {
        Console_Printf("Screenshot already queued.");
        return;
    }
    _mkdir("screenshots");

    time_t rawtime;
    struct tm* timeinfo;
    time(&rawtime);
    timeinfo = localtime(&rawtime);

    strftime(g_screenshot_path, sizeof(g_screenshot_path), "screenshots/screenshot_%Y-%m-%d_%H-%M-%S.png", timeinfo);
    g_screenshot_requested = true;
}

void Cmd_Echo(int argc, char** argv) {
    if (argc < 2) {
        Console_Printf("Usage: echo <message>");
        return;
    }

    char message[1024] = { 0 };
    for (int i = 1; i < argc; i++) {
        strcat(message, argv[i]);
        if (i < argc - 1) {
            strcat(message, " ");
        }
    }
    Console_Printf("%s", message);
}

void Cmd_Clear(int argc, char** argv) {
    Console_ClearLog();
}

void Cmd_Help(int argc, char** argv) {
    Console_Printf("--- Command List ---");
    for (int i = 0; i < Commands_GetCount(); ++i) {
        const Command* cmd = Commands_GetCommand(i);
        if (cmd) {
            Console_Printf("%s - %s", cmd->name, cmd->description);
        }
    }
    Console_Printf("--- CVAR List ---");
    Console_Printf("To set a cvar, type: <cvar_name> <value>");
    for (int i = 0; i < Cvar_GetCount(); i++) {
        const Cvar* c = Cvar_GetCvar(i);
        if (c && !(c->flags & CVAR_HIDDEN)) {
            Console_Printf("%s - %s (current: \"%s\")", c->name, c->helpText, c->stringValue);
        }
    }
    Console_Printf("--------------------");
}

void Cmd_Exec(int argc, char** argv) {
    if (argc != 2) {
        Console_Printf("Usage: exec <filename>");
        return;
    }

    const char* filename = argv[1];
    FILE* file = fopen(filename, "r");
    if (!file) {
        Console_Printf_Error("[error] Could not open script file: %s", filename);
        return;
    }

    Console_Printf("Executing script: %s", filename);
    char line[512];
    while (fgets(line, sizeof(line), file)) {
        char* trimmed_line = trim(line);
        if (strlen(trimmed_line) == 0 || trimmed_line[0] == '/' || trimmed_line[0] == '#') {
            continue;
        }

        char* cmd_copy = _strdup(trimmed_line);
#define MAX_ARGS 32
        int exec_argc = 0;
        char* exec_argv[MAX_ARGS];

        char* p = strtok(cmd_copy, " ");
        while (p != NULL && exec_argc < MAX_ARGS) {
            exec_argv[exec_argc++] = p;
            p = strtok(NULL, " ");
        }

        if (exec_argc > 0) {
            Commands_Execute(exec_argc, exec_argv);
        }
        free(cmd_copy);
    }

    fclose(file);
    Console_Printf("Finished executing script: %s", filename);
}

void Cmd_SaveGame(int argc, char** argv) {
    if (g_current_mode != MODE_GAME && g_current_mode != MODE_EDITOR && g_current_mode != MODE_INGAMEMENU) {
        Console_Printf_Error("Can only save when a map is loaded.");
        return;
    }

    if (argc != 2) {
        Console_Printf("Usage: save <savename>");
        return;
    }

    char savePath[256];
    snprintf(savePath, sizeof(savePath), "saves/%s.sav", argv[1]);

    if (_mkdir("saves") != 0 && errno != EEXIST) {
        Console_Printf_Error("Could not create saves directory.");
        return;
    }

    if (Scene_SaveMap(&g_scene, g_engine, savePath)) {
        Console_Printf("Game saved to %s", savePath);
    }
    else {
        Console_Printf_Error("Failed to save game to %s", savePath);
    }
}

void Cmd_LoadGame(int argc, char** argv) {
    if (argc != 2) {
        Console_Printf("Usage: load <savename>");
        return;
    }

    char savePath[256];
    snprintf(savePath, sizeof(savePath), "saves/%s.sav", argv[1]);

    Console_Printf("Loading game from %s...", savePath);

    if (g_is_editor_mode) {
        Editor_Shutdown();
    }
    g_current_mode = MODE_GAME;
    SDL_SetRelativeMouseMode(SDL_TRUE);

    if (Scene_LoadMap(&g_scene, &g_renderer, savePath, g_engine)) {
        Console_Printf("Game loaded successfully.");
    }
    else {
        Console_Printf_Error("Failed to load save file: %s", savePath);
        g_current_mode = MODE_MAINMENU;
        SDL_SetRelativeMouseMode(SDL_FALSE);
        MainMenu_SetInGameMenuMode(false, false);
    }
}

void Cmd_BuildLighting(int argc, char** argv) {
    int resolution = 128;
    int bounces = 1;

    if (argc > 1) {
        int res_arg = atoi(argv[1]);
        if (res_arg > 0 && (res_arg & (res_arg - 1)) == 0) {
            resolution = res_arg;
        }
        else {
            Console_Printf_Warning("[WARNING] Invalid lightmap resolution '%s'. Must be a power of two. Using default 128.", argv[1]);
        }
    }

    if (argc > 2) {
        int bounce_arg = atoi(argv[2]);
        if (bounce_arg >= 0) {
            bounces = bounce_arg;
        }
        else {
            Console_Printf_Warning("[WARNING] Invalid bounce count '%s'. Must be >= 0. Using default 1.", argv[2]);
        }
    }

    Lightmapper_Generate(&g_scene, g_engine, resolution, bounces);
}

void Cmd_ScreenShake(int argc, char** argv) {
    if (argc < 4) {
        Console_Printf("Usage: screenshake <amplitude> <frequency> <duration>");
        return;
    }

    g_engine->shake_amplitude = atof(argv[1]);
    g_engine->shake_frequency = atof(argv[2]);
    g_engine->shake_duration_timer = atof(argv[3]);
}

void init_cvars() {
    Cvar_Register("developer", "0", "Show developer console log on screen (0=off, 1=on)", CVAR_CHEAT);
    Cvar_Register("volume", "2.5", "Master volume for the game (0.0 to 4.0)", CVAR_NONE);
    Cvar_Register("noclip", "0", "Enable noclip mode (0=off, 1=on)", CVAR_NONE);
    Cvar_Register("god", "0", "Enable god mode (player is invulnerable).", CVAR_CHEAT);
    Cvar_Register("gravity", "9.81", "World gravity value", CVAR_NONE);
    Cvar_Register("engine_running", "1", "Engine state (0=off, 1=on)", CVAR_HIDDEN);
    Cvar_Register("r_width", "1920", "Screen width in pixels", CVAR_NONE);
    Cvar_Register("r_height", "1080", "Screen height in pixels", CVAR_NONE);
    Cvar_Register("r_autoexposure", "1", "Enable auto-exposure (0=off, 1=on)", CVAR_NONE);
    Cvar_Register("r_autoexposure_speed", "0.5", "Auto-exposure adaptation speed", CVAR_NONE);
    Cvar_Register("r_autoexposure_key", "0.18", "Auto-exposure middle-grey value", CVAR_NONE);
    Cvar_Register("r_ssao", "1", "Enable SSAO (0=off, 1=on)", CVAR_NONE);
    Cvar_Register("r_ssr", "0", "Enable Screen Space Reflections (0=off, 1=on)", CVAR_NONE);
    Cvar_Register("r_bloom", "1", "Enable bloom (0=off, 1=on)", CVAR_NONE);
    Cvar_Register("r_volumetrics", "1", "Enable volumetric lighting (0=off, 1=on)", CVAR_NONE);
    Cvar_Register("r_faceculling", "1", "Enable back-face culling (0=off, 1=on)", CVAR_NONE);
    Cvar_Register("r_zprepass", "1", "Enable Z-prepass (0=off, 1=on)", CVAR_NONE);
    Cvar_Register("r_physics_shadows", "1", "Enable Basic realtime shadows for physics props (0=off, 1=on)", CVAR_NONE);
    Cvar_Register("r_wireframe", "0", "Render in wireframe mode (0=off, 1=on)", CVAR_NONE);
    Cvar_Register("r_shadows", "1", "Enable dynamic shadows (0=off, 1=on)", CVAR_NONE);
    Cvar_Register("r_shadow_distance_max", "100.0", "Max shadow casting distance", CVAR_NONE);
    Cvar_Register("r_shadow_map_size", "1024", "Shadow map resolution", CVAR_NONE);
    Cvar_Register("r_relief_mapping", "1", "Enable relief mapping (0=off, 1=on)", CVAR_NONE);
    Cvar_Register("r_cubemaps", "1", "Enable environment mapping reflections (0=off, 1=on)", CVAR_NONE);
    Cvar_Register("r_colorcorrection", "1", "Enable color correction (0=off, 1=on)", CVAR_NONE);
    Cvar_Register("r_vignette", "1", "Enable vignette (0=off, 1=on)", CVAR_NONE);
    Cvar_Register("r_chromaticabberation", "1", "Enable chromatic aberration (0=off, 1=on)", CVAR_NONE);
    Cvar_Register("r_dof", "1", "Enable depth of field (0=off, 1=on)", CVAR_NONE);
    Cvar_Register("r_scanline", "1", "Enable scanline effect (0=off, 1=on)", CVAR_NONE);
    Cvar_Register("r_filmgrain", "1", "Enable film grain (0=off, 1=on)", CVAR_NONE);
    Cvar_Register("r_lensflare", "1", "Enable lens flare (0=off, 1=on)", CVAR_NONE);
    Cvar_Register("r_black_white", "1", "Enable black and white effect (0=off, 1=on)", CVAR_NONE);
    Cvar_Register("r_sharpening", "1", "Enable sharpening (0=off, 1=on)", CVAR_NONE);
    Cvar_Register("r_invert", "1", "Enable color invert (0=off, 1=on)", CVAR_NONE);
    Cvar_Register("r_vsync", "1", "Enable vertical sync (0=off, 1=on)", CVAR_NONE);
    Cvar_Register("r_motionblur", "0", "Enable motion blur (0=off, 1=on)", CVAR_NONE);
    Cvar_Register("r_fxaa", "1", "Enable depth-based anti-aliasing (0=off, 1=on)", CVAR_NONE);
    Cvar_Register("r_clear", "0", "Clear the screen every frame (0=off, 1=on)", CVAR_NONE);
    Cvar_Register("r_skybox", "1", "Enable skybox (0=off, 1=on)", CVAR_NONE);
    Cvar_Register("r_particles", "1", "Enable particles (0=off, 1=on)", CVAR_NONE);
    Cvar_Register("r_particles_cull_dist", "75.0", "Particle culling distance", CVAR_NONE);
    Cvar_Register("r_sprites", "1", "Enable sprites (0=off, 1=on)", CVAR_NONE);
    Cvar_Register("r_water", "1", "Enable water rendering (0=off, 1=on)", CVAR_NONE);
    Cvar_Register("r_planar", "1", "Enable planar reflections for water and reflective glass (0=off, 1=on)", CVAR_NONE);
    Cvar_Register("r_planar_downsample", "2", "Downsample factor for planar reflections/refractions (e.g., 2 = 1/4 resolution)", CVAR_NONE);
    Cvar_Register("r_lightmaps_bicubic", "0", "Enable Bicubic lightmap filtering (0=off, 1=on)", CVAR_NONE);
    Cvar_Register("fps_max", "300", "Max FPS (0=unlimited)", CVAR_NONE);
    Cvar_Register("show_fps", "0", "Show FPS counter (0=off, 1=on)", CVAR_NONE);
    Cvar_Register("r_showgraph", "0", "Show framerate graph (0=off, 1=on)", CVAR_NONE);
    Cvar_Register("show_pos", "0", "Show player position (0=off, 1=on)", CVAR_NONE);
    Cvar_Register("r_debug_albedo", "0", "Show albedo buffer (0=off, 1=on)", CVAR_NONE);
    Cvar_Register("r_debug_normals", "0", "Show normals buffer (0=off, 1=on)", CVAR_NONE);
    Cvar_Register("r_debug_position", "0", "Show position buffer (0=off, 1=on)", CVAR_NONE);
    Cvar_Register("r_debug_metallic", "0", "Show metallic buffer (0=off, 1=on)", CVAR_NONE);
    Cvar_Register("r_debug_roughness", "0", "Show roughness buffer (0=off, 1=on)", CVAR_NONE);
    Cvar_Register("r_debug_ao", "0", "Show ambient occlusion buffer (0=off, 1=on)", CVAR_NONE);
    Cvar_Register("r_debug_velocity", "0", "Show velocity buffer (0=off, 1=on)", CVAR_NONE);
    Cvar_Register("r_debug_volumetric", "0", "Show volumetric buffer (0=off, 1=on)", CVAR_NONE);
    Cvar_Register("r_debug_bloom", "0", "Show bloom mask (0=off, 1=on)", CVAR_NONE);
    Cvar_Register("r_debug_lightmaps", "0", "Show lightmap buffer (0=off, 1=on)", CVAR_NONE);
    Cvar_Register("r_debug_lightmaps_directional", "0", "Show directional lightmap buffer (0=off, 1=on)", CVAR_NONE);
    Cvar_Register("r_debug_vertex_light", "0", "Show baked vertex lighting buffer (0=off, 1=on)", CVAR_NONE);
    Cvar_Register("r_debug_vertex_light_directional", "0", "Show baked directional vertex lighting buffer (0=off, 1=on)", CVAR_NONE);
    Cvar_Register("r_debug_water_reflection", "0", "Forces water to show pure reflection texture (0=off, 1=on)", CVAR_CHEAT);
    Cvar_Register("r_sun_shadow_distance", "50.0", "Sun shadow frustum size", CVAR_NONE);
    Cvar_Register("r_texture_quality", "5", "Texture quality (1=very low to 5=very high)", CVAR_NONE);
    Cvar_Register("fov_vertical", "55", "Vertical field of view (degrees)", CVAR_NONE);
    Cvar_Register("g_speed", "6.0", "Player walking speed", CVAR_NONE);
    Cvar_Register("g_sprint_speed", "8.0", "Player sprinting speed", CVAR_NONE);
    Cvar_Register("g_accel", "15.0", "Player acceleration", CVAR_NONE);
    Cvar_Register("g_friction", "2.0", "Player friction", CVAR_NONE);
    Cvar_Register("g_jump_force", "350.0", "Player jump force", CVAR_NONE);
    Cvar_Register("g_bob", "0.01", "The amount of view bobbing.", CVAR_NONE);
    Cvar_Register("g_bobcycle", "0.8", "The speed of the view bobbing.", CVAR_NONE);
#ifdef GAME_RELEASE
    Cvar_Register("g_cheats", "0", "Enable cheats (0=off, 1=on)", CVAR_NONE);
#else
    Cvar_Register("g_cheats", "1", "Enable cheats (0=off, 1=on)", CVAR_NONE);
#endif
    Cvar_Register("crosshair", "1", "Enable crosshair (0=off, 1=on)", CVAR_NONE);
    Cvar_Register("timescale", "1.0", "Game speed scale", CVAR_CHEAT);
    Cvar_Register("sensitivity", "1.0", "Mouse sensitivity.", CVAR_NONE);
    Cvar_Register("p_disable_deactivation", "0", "Disables physics objects sleeping (0=off, 1=on).", CVAR_NONE);
}

void init_commands() {
    Commands_Register("help", Cmd_Help, "Shows a list of all available commands and cvars.", CMD_NONE);
    Commands_Register("cmdlist", Cmd_Help, "Alias for the 'help' command.", CMD_NONE);
    Commands_Register("edit", Cmd_Edit, "Toggles editor mode.", CMD_NONE);
    Commands_Register("screenshake", Cmd_ScreenShake, "Applies a screen shake effect. Usage: screenshake <amplitude> <frequency> <duration>", CMD_CHEAT);
    Commands_Register("quit", Cmd_Quit, "Exits the engine.", CMD_NONE);
    Commands_Register("exit", Cmd_Quit, "Alias for the 'quit' command.", CMD_NONE);
    Commands_Register("setpos", Cmd_SetPos, "Teleports the player to a specified XYZ coordinate.", CMD_CHEAT);
    Commands_Register("noclip", Cmd_Noclip, "Toggles player collision and gravity.", CMD_CHEAT);
    Commands_Register("bind", Cmd_Bind, "Binds a key to a command.", CMD_NONE);
    Commands_Register("unbind", Cmd_Unbind, "Removes a key binding.", CMD_NONE);
    Commands_Register("unbindall", Cmd_UnbindAll, "Removes all key bindings.", CMD_NONE);
    Commands_Register("map", Cmd_Map, "Loads the specified map.", CMD_NONE);
    Commands_Register("maps", Cmd_Maps, "Lists all available .map files in the root directory.", CMD_NONE);
    Commands_Register("disconnect", Cmd_Disconnect, "Disconnects from the current map and returns to the main menu.", CMD_NONE);
    Commands_Register("save", Cmd_SaveGame, "Saves the current game state.", CMD_NONE);
    Commands_Register("load", Cmd_LoadGame, "Loads a saved game state.", CMD_NONE);
    Commands_Register("build_lighting", Cmd_BuildLighting, "Builds static lighting for the scene. Usage: build_lighting [resolution] [bounces]", CMD_NONE);
    Commands_Register("download", Cmd_Download, "Downloads a file from a URL.", CMD_NONE);
    Commands_Register("ping", Cmd_Ping, "Pings a network host to check connectivity.", CMD_NONE);
    Commands_Register("build_cubemaps", Cmd_BuildCubemaps, "Builds cubemaps for all reflection probes. Usage: build_cubemaps [resolution]", CMD_NONE);
    Commands_Register("screenshot", Cmd_Screenshot, "Saves a screenshot to disk.", CMD_NONE);
    Commands_Register("exec", Cmd_Exec, "Executes a script file from the root directory.", CMD_NONE);
    Commands_Register("echo", Cmd_Echo, "Prints a message to the console.", CMD_NONE);
    Commands_Register("clear", Cmd_Clear, "Clears the console text.", CMD_NONE);

    Console_Printf("Engine commands registered.");
}

void PrintSystemInfo() {
    char vendor[13];
    char brand[49];

    GetCPUType(vendor);
    GetCPUName(brand);

    Console_Printf("CPU Vendor: %s\n", vendor);
    Console_Printf("CPU Brand:  %s\n", brand);
    Console_Printf("CPU count: %d\n", SDL_GetCPUCount());
    Console_Printf("Cache line size: %d bytes\n", SDL_GetCPUCacheLineSize());
    Console_Printf("RDTSC support: %s\n", SDL_HasRDTSC() ? "Yes" : "No");
    Console_Printf("AltiVec support: %s\n", SDL_HasAltiVec() ? "Yes" : "No");
    Console_Printf("MMX support: %s\n", SDL_HasMMX() ? "Yes" : "No");
    Console_Printf("3DNow support: %s\n", SDL_Has3DNow() ? "Yes" : "No");
    Console_Printf("SSE support: %s\n", SDL_HasSSE() ? "Yes" : "No");
    Console_Printf("SSE2 support: %s\n", SDL_HasSSE2() ? "Yes" : "No");
    Console_Printf("SSE3 support: %s\n", SDL_HasSSE3() ? "Yes" : "No");
    Console_Printf("SSE4.1 support: %s\n", SDL_HasSSE41() ? "Yes" : "No");
    Console_Printf("SSE4.2 support: %s\n", SDL_HasSSE42() ? "Yes" : "No");
    Console_Printf("AVX support: %s\n", SDL_HasAVX() ? "Yes" : "No");
    Console_Printf("AVX2 support: %s\n", SDL_HasAVX2() ? "Yes" : "No");
    Console_Printf("NEON support: %s\n", SDL_HasNEON() ? "Yes" : "No");
    Console_Printf("RAM: %d MB\n", SDL_GetSystemRAM());
}

void init_engine(SDL_Window* window, SDL_GLContext context) {
    g_engine->width = g_startup_width;
    g_engine->height = g_startup_height;
    g_engine->window = window; g_engine->context = context; g_engine->running = true; g_engine->deltaTime = 0.0f; g_engine->lastFrame = 0.0f;
    g_engine->unscaledDeltaTime = 0.0f; g_engine->scaledTime = 0.0f;
    IPC_Init();
    g_engine->camera = (Camera){ {0,1,5}, 0,0, false, PLAYER_HEIGHT_NORMAL, NULL, 100.0f };  g_engine->flashlight_on = false;
    g_engine->active_camera_brush_index = -1;
    g_player_input_disabled = false;
    g_engine->prev_health = g_engine->camera.health;
    g_engine->red_flash_intensity = 0.0f;
    g_engine->prev_player_y_velocity = 0.0f;
    GameConfig_Init();
    UI_Init(window, context);
    SoundSystem_Init();
    Cvar_Init();
    Log_Init("logs.txt");
    init_cvars();
    Cvar_Load("cvars.txt");
    IO_Init();
    Binds_Init();
    Commands_Init();
    init_commands();
    GameData_Init("tectonic.tgd");
    Sentry_Init();
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
        Console_Printf_Error("[ERROR] Failed to initialize Main Menu.");
        g_engine->running = false;
    }
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
    g_last_player_pos = g_scene.playerStart.position;
}

void process_input() {
    SDL_Event event;
    while (SDL_PollEvent(&event)) {
        if (event.type == SDL_QUIT) {
            Cvar_EngineSet("engine_running", "0");
            return;
        }
        UI_ProcessEvent(&event);
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
                Weapons_TryFire(g_engine, &g_scene);
            }
        }
        if (g_current_mode == MODE_MAINMENU || g_current_mode == MODE_INGAMEMENU) {
            MainMenuAction action = MainMenu_HandleEvent(&event);
            if (action == MAINMENU_ACTION_START_GAME) {
                g_current_mode = MODE_GAME;
                SDL_SetRelativeMouseMode(SDL_TRUE);
                Console_Printf("Starting game...");
                MainMenu_SetInGameMenuMode(false, true);
            }
            else if (action == MAINMENU_ACTION_CONTINUE_GAME) {
                g_current_mode = MODE_GAME;
                SDL_SetRelativeMouseMode(SDL_TRUE);
            }
            else if (action == MAINMENU_ACTION_QUIT) {
                Cvar_EngineSet("engine_running", "0");
            }
        }
        else if (g_current_mode == MODE_EDITOR) {
            Editor_ProcessEvent(&event, &g_scene, g_engine);
        }

        if (event.type == SDL_KEYDOWN && event.key.repeat == 0) {
            if (event.key.keysym.sym == SDLK_e && g_current_mode == MODE_GAME && !Console_IsVisible()) {
                Vec3 forward = { cosf(g_engine->camera.pitch) * sinf(g_engine->camera.yaw), sinf(g_engine->camera.pitch), -cosf(g_engine->camera.pitch) * cosf(g_engine->camera.yaw) };
                vec3_normalize(&forward);

                Vec3 ray_end = vec3_add(g_engine->camera.position, vec3_muls(forward, 3.0f));

                for (int i = 0; i < g_scene.numBrushes; ++i) {
                    Brush* brush = &g_scene.brushes[i];
                    if (strcmp(brush->classname, "func_button") == 0) {
                        Vec3 brush_local_min = { FLT_MAX, FLT_MAX, FLT_MAX };
                        Vec3 brush_local_max = { -FLT_MAX, -FLT_MAX, -FLT_MAX };
                        if (brush->numVertices > 0) {
                            for (int v_idx = 0; v_idx < brush->numVertices; ++v_idx) {
                                brush_local_min.x = fminf(brush_local_min.x, brush->vertices[v_idx].pos.x);
                                brush_local_min.y = fminf(brush_local_min.y, brush->vertices[v_idx].pos.y);
                                brush_local_min.z = fminf(brush_local_min.z, brush->vertices[v_idx].pos.z);
                                brush_local_max.x = fmaxf(brush_local_max.x, brush->vertices[v_idx].pos.x);
                                brush_local_max.y = fmaxf(brush_local_max.y, brush->vertices[v_idx].pos.y);
                                brush_local_max.z = fmaxf(brush_local_max.z, brush->vertices[v_idx].pos.z);
                            }
                        }
                        else {
                            brush_local_min = (Vec3){ -0.1f, -0.1f, -0.1f };
                            brush_local_max = (Vec3){ 0.1f,  0.1f,  0.1f };
                        }

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
                            Vec3 brush_local_min = { FLT_MAX, FLT_MAX, FLT_MAX };
                            Vec3 brush_local_max = { -FLT_MAX, -FLT_MAX, -FLT_MAX };
                            if (brush->numVertices > 0) {
                                for (int v_idx = 0; v_idx < brush->numVertices; ++v_idx) {
                                    brush_local_min.x = fminf(brush_local_min.x, brush->vertices[v_idx].pos.x);
                                    brush_local_min.y = fminf(brush_local_min.y, brush->vertices[v_idx].pos.y);
                                    brush_local_min.z = fminf(brush_local_min.z, brush->vertices[v_idx].pos.z);
                                    brush_local_max.x = fmaxf(brush_local_max.x, brush->vertices[v_idx].pos.x);
                                    brush_local_max.y = fmaxf(brush_local_max.y, brush->vertices[v_idx].pos.y);
                                    brush_local_max.z = fmaxf(brush_local_max.z, brush->vertices[v_idx].pos.z);
                                }
                            }
                            else {
                                brush_local_min = (Vec3){ -0.1f, -0.1f, -0.1f };
                                brush_local_max = (Vec3){ 0.1f,  0.1f,  0.1f };
                            }
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
                        Vec3 brush_local_min = { FLT_MAX, FLT_MAX, FLT_MAX };
                        Vec3 brush_local_max = { -FLT_MAX, -FLT_MAX, -FLT_MAX };
                        if (brush->numVertices > 0) {
                            for (int v_idx = 0; v_idx < brush->numVertices; ++v_idx) {
                                brush_local_min.x = fminf(brush_local_min.x, brush->vertices[v_idx].pos.x);
                                brush_local_min.y = fminf(brush_local_min.y, brush->vertices[v_idx].pos.y);
                                brush_local_min.z = fminf(brush_local_min.z, brush->vertices[v_idx].pos.z);
                                brush_local_max.x = fmaxf(brush_local_max.x, brush->vertices[v_idx].pos.x);
                                brush_local_max.y = fmaxf(brush_local_max.y, brush->vertices[v_idx].pos.y);
                                brush_local_max.z = fmaxf(brush_local_max.z, brush->vertices[v_idx].pos.z);
                            }
                        }
                        else {
                            brush_local_min = (Vec3){ -0.5f, -0.5f, -0.5f };
                            brush_local_max = (Vec3){ 0.5f, 0.5f, 0.5f };
                        }

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
            }
            if (event.key.keysym.sym == SDLK_ESCAPE) {
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
                Console_Toggle();
                if (g_current_mode == MODE_GAME || g_current_mode == MODE_INGAMEMENU) {
                    SDL_SetRelativeMouseMode(Console_IsVisible() ? SDL_FALSE : SDL_TRUE);
                }
            }
#ifndef GAME_RELEASE
            else if (event.key.keysym.sym == SDLK_F5) {
                if (g_current_mode != MODE_MAINMENU) {
                    handle_command(1, (char* []) { "edit" });
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
                            handle_command(argc, argv);
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
        float speed = (noclip ? 10.0f : 5.0f) * (g_engine->camera.isCrouching ? 0.5f : 1.0f);

        if (g_player_input_disabled) {
            if (!noclip) Physics_SetLinearVelocity(g_engine->camera.physicsBody, (Vec3) { 0, 0, 0 });
            return;
        }

        if (noclip) {
            Vec3 forward = { cosf(g_engine->camera.pitch) * sinf(g_engine->camera.yaw), sinf(g_engine->camera.pitch), -cosf(g_engine->camera.pitch) * cosf(g_engine->camera.yaw) };
            vec3_normalize(&forward);
            Vec3 right = vec3_cross(forward, (Vec3) { 0, 1, 0 });
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
                    current_vel_flat = (Vec3){ 0,0,0 };
                }
            }

            Physics_SetLinearVelocity(g_engine->camera.physicsBody, (Vec3) { current_vel_flat.x, current_vel.y, current_vel_flat.z });
            Physics_Activate(g_engine->camera.physicsBody);

            if (state[SDL_SCANCODE_SPACE]) {
                Vec3 current_vel = Physics_GetLinearVelocity(g_engine->camera.physicsBody);
                if (current_vel.y <= 0.1f && Physics_CheckGroundContact(g_engine->physicsWorld, g_engine->camera.physicsBody, 0.1f)) {
                    Physics_ApplyCentralImpulse(g_engine->camera.physicsBody, (Vec3) { 0, Cvar_GetFloat("g_jump_force"), 0 });
                    SoundSystem_PlaySound(g_jump_sound_buffer, g_engine->camera.position, 1.0f, 1.0f, 50.0f, false);
                }
            }
        }
        g_engine->camera.isCrouching = state[SDL_SCANCODE_LCTRL];
    }
}

void update_state() {
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
    SoundSystem_SetMasterVolume(Cvar_GetFloat("volume"));
    IO_ProcessPendingEvents(g_engine->lastFrame, &g_scene, g_engine);
    LogicSystem_Update(&g_scene, g_engine->deltaTime);
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
            Mat4 rot_mat = create_trs_matrix((Vec3) { 0, 0, 0 }, light->rot, (Vec3) { 1, 1, 1 });
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
        if (b->numVertices == 0) continue;

        Vec3 min_aabb = { FLT_MAX, FLT_MAX, FLT_MAX };
        Vec3 max_aabb = { -FLT_MAX, -FLT_MAX, -FLT_MAX };
        for (int v = 0; v < b->numVertices; ++v) {
            Vec3 world_v = mat4_mul_vec3(&b->modelMatrix, b->vertices[v].pos);
            min_aabb.x = fminf(min_aabb.x, world_v.x);
            min_aabb.y = fminf(min_aabb.y, world_v.y);
            min_aabb.z = fminf(min_aabb.z, world_v.z);
            max_aabb.x = fmaxf(max_aabb.x, world_v.x);
            max_aabb.y = fmaxf(max_aabb.y, world_v.y);
            max_aabb.z = fmaxf(max_aabb.z, world_v.z);
        }

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

        if (b->numVertices == 0) continue;

        Vec3 min_aabb = { FLT_MAX, FLT_MAX, FLT_MAX };
        Vec3 max_aabb = { -FLT_MAX, -FLT_MAX, -FLT_MAX };
        for (int v = 0; v < b->numVertices; ++v) {
            Vec3 world_v = mat4_mul_vec3(&b->modelMatrix, b->vertices[v].pos);
            min_aabb.x = fminf(min_aabb.x, world_v.x);
            min_aabb.y = fminf(min_aabb.y, world_v.y);
            min_aabb.z = fminf(min_aabb.z, world_v.z);
            max_aabb.x = fmaxf(max_aabb.x, world_v.x);
            max_aabb.y = fmaxf(max_aabb.y, world_v.y);
            max_aabb.z = fmaxf(max_aabb.z, world_v.z);
        }

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
    vec3_normalize(&forward); SoundSystem_UpdateListener(g_engine->camera.position, forward, (Vec3) { 0, 1, 0 });
    bool noclip = Cvar_GetInt("noclip");
    if (!noclip) {
        Vec3 vel = Physics_GetLinearVelocity(g_engine->camera.physicsBody);
        bool on_ground = fabs(vel.y) < 0.1f;

        if (on_ground) {
            float dx = g_engine->camera.position.x - g_last_player_pos.x;
            float dz = g_engine->camera.position.z - g_last_player_pos.z;
            g_distance_walked += sqrtf(dx * dx + dz * dz);

            if (g_distance_walked >= FOOTSTEP_DISTANCE) {
                SoundSystem_PlaySound(g_footstep_sound_buffer, g_engine->camera.position, 0.7f, 1.0f, 50.0f, false);
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
            if (strcmp(b->classname, "func_water") == 0 && b->numVertices > 0) {
                Physics_ApplyBuoyancyInVolume(g_engine->physicsWorld, (const float*)b->vertices, b->numVertices, &b->modelMatrix);
            }
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
            Physics_SetLinearVelocity(g_engine->camera.physicsBody, (Vec3) { conveyor_vel.x, player_vel.y, conveyor_vel.z });
            Physics_Activate(g_engine->camera.physicsBody);
        }

        Vec3 conveyor_min, conveyor_max;
        if (b->numVertices > 0) {
            conveyor_min = (Vec3){ FLT_MAX, FLT_MAX, FLT_MAX };
            conveyor_max = (Vec3){ -FLT_MAX, -FLT_MAX, -FLT_MAX };
            for (int v_idx = 0; v_idx < b->numVertices; ++v_idx) {
                Vec3 world_v = mat4_mul_vec3(&b->modelMatrix, b->vertices[v_idx].pos);
                conveyor_min.x = fminf(conveyor_min.x, world_v.x); conveyor_min.y = fminf(conveyor_min.y, world_v.y); conveyor_min.z = fminf(conveyor_min.z, world_v.z);
                conveyor_max.x = fmaxf(conveyor_max.x, world_v.x); conveyor_max.y = fmaxf(conveyor_max.y, world_v.y); conveyor_max.z = fmaxf(conveyor_max.z, world_v.z);
            }

            for (int obj_idx = 0; obj_idx < g_scene.numObjects; ++obj_idx) {
                SceneObject* obj = &g_scene.objects[obj_idx];
                if (obj->mass > 0.0f) {
                    Vec3 obj_pos = obj->pos;
                    if (obj_pos.x > conveyor_min.x && obj_pos.x < conveyor_max.x &&
                        obj_pos.y > conveyor_min.y && obj_pos.y < conveyor_max.y + 1.0f &&
                        obj_pos.z > conveyor_min.z && obj_pos.z < conveyor_max.z)
                    {
                        Vec3 obj_vel = Physics_GetLinearVelocity(obj->physicsBody);
                        Physics_SetLinearVelocity(obj->physicsBody, (Vec3) { conveyor_vel.x, obj_vel.y, conveyor_vel.z });
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
            Physics_SetGravity(g_engine->physicsWorld, (Vec3) { 0, -gravity_val, 0 });
            in_gravity_zone = true;
            break;
        }
    }

    if (!in_gravity_zone) {
        Physics_SetGravity(g_engine->physicsWorld, (Vec3) { 0, -Cvar_GetFloat("gravity"), 0 });
    }
    if (g_engine->camera.health <= 0.0f) {
        g_engine->camera.health = 100.0f;
        g_engine->prev_health = g_engine->camera.health;
        Physics_Teleport(g_engine->camera.physicsBody, g_scene.playerStart.position);
        Console_Printf("Player died and respawned.");
    }
    Physics_SetGravityEnabled(g_engine->camera.physicsBody, !noclip);
    if (noclip) Physics_SetLinearVelocity(g_engine->camera.physicsBody, (Vec3) { 0, 0, 0 });
    if (g_engine->physicsWorld) Physics_StepSimulation(g_engine->physicsWorld, g_engine->deltaTime);
    if (!noclip && !g_player_input_disabled) { Vec3 p; Physics_GetPosition(g_engine->camera.physicsBody, &p); g_engine->camera.position.x = p.x; g_engine->camera.position.z = p.z; float h = g_engine->camera.isCrouching ? PLAYER_HEIGHT_CROUCH : PLAYER_HEIGHT_NORMAL; float eyeHeightOffsetFromCenter = (g_engine->camera.currentHeight / 2.0f) * 0.85f;
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

                Mat4 rot_mat = create_trs_matrix((Vec3) { 0, 0, 0 }, move_angles, (Vec3) { 1, 1, 1 });
                b->door_move_dir = mat4_mul_vec3_dir(&rot_mat, (Vec3) { 1, 0, 0 });
                vec3_normalize(&b->door_move_dir);

                float move_dist = atof(Brush_GetProperty(b, "distance", "0"));

                if (move_dist <= 0) {
                    Vec3 min_aabb_local = { FLT_MAX, FLT_MAX, FLT_MAX };
                    Vec3 max_aabb_local = { -FLT_MAX, -FLT_MAX, -FLT_MAX };
                    for (int v = 0; v < b->numVertices; ++v) {
                        min_aabb_local.x = fminf(min_aabb_local.x, b->vertices[v].pos.x);
                        min_aabb_local.y = fminf(min_aabb_local.y, b->vertices[v].pos.y);
                        min_aabb_local.z = fminf(min_aabb_local.z, b->vertices[v].pos.z);
                        max_aabb_local.x = fmaxf(max_aabb_local.x, b->vertices[v].pos.x);
                        max_aabb_local.y = fmaxf(max_aabb_local.y, b->vertices[v].pos.y);
                        max_aabb_local.z = fmaxf(max_aabb_local.z, b->vertices[v].pos.z);
                    }
                    Vec3 size = vec3_sub(max_aabb_local, min_aabb_local);
                    Vec3 extent_x = vec3_muls((Vec3) { 1, 0, 0 }, size.x);
                    Vec3 extent_y = vec3_muls((Vec3) { 0, 1, 0 }, size.y);
                    Vec3 extent_z = vec3_muls((Vec3) { 0, 0, 1 }, size.z);
                    move_dist = fabsf(vec3_dot(extent_x, b->door_move_dir)) + fabsf(vec3_dot(extent_y, b->door_move_dir)) + fabsf(vec3_dot(extent_z, b->door_move_dir));
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
                b->current_angular_velocity = vec3_lerp((Vec3) { b->current_angular_velocity, 0, 0 }, (Vec3) { b->target_angular_velocity, 0, 0 }, g_engine->deltaTime* lerp_speed).x;
            }
            else {
                b->current_angular_velocity = b->target_angular_velocity;
            }

            if (fabsf(b->current_angular_velocity) > 0.001f) {
                Vec3 rotation_axis = { 0, 0, 1 };
                if (atoi(Brush_GetProperty(b, "XAxis", "0")) != 0) rotation_axis = (Vec3){ 1, 0, 0 };
                if (atoi(Brush_GetProperty(b, "YAxis", "0")) != 0) rotation_axis = (Vec3){ 0, 1, 0 };

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

                Mat4 rot_mat = create_trs_matrix((Vec3) { 0, 0, 0 }, swing_angles, (Vec3) { 1, 1, 1 });
                b->pendulum_swing_dir = mat4_mul_vec3_dir(&rot_mat, (Vec3) { 1, 0, 0 });
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

        Vec3 min_aabb = { FLT_MAX, FLT_MAX, FLT_MAX };
        Vec3 max_aabb = { -FLT_MAX, -FLT_MAX, -FLT_MAX };
        for (int v = 0; v < b->numVertices; ++v) {
            Vec3 world_v = mat4_mul_vec3(&b->modelMatrix, b->vertices[v].pos);
            min_aabb.x = fminf(min_aabb.x, world_v.x);
            min_aabb.y = fminf(min_aabb.y, world_v.y);
            min_aabb.z = fminf(min_aabb.z, world_v.z);
            max_aabb.x = fmaxf(max_aabb.x, world_v.x);
            max_aabb.y = fmaxf(max_aabb.y, world_v.y);
            max_aabb.z = fmaxf(max_aabb.z, world_v.z);
        }

        if (g_engine->camera.position.x >= min_aabb.x && g_engine->camera.position.x <= max_aabb.x &&
            g_engine->camera.position.y >= min_aabb.y && g_engine->camera.position.y <= max_aabb.y &&
            g_engine->camera.position.z >= min_aabb.z && g_engine->camera.position.z <= max_aabb.z)
        {
            g_scene.post.isUnderwater = true;
            g_scene.post.underwaterColor = (Vec3){ 0.1f, 0.3f, 0.4f };
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

void render_parallax_rooms(Mat4* view, Mat4* projection) {
    glUseProgram(g_renderer.parallaxInteriorShader);
    glUniformMatrix4fv(glGetUniformLocation(g_renderer.parallaxInteriorShader, "view"), 1, GL_FALSE, view->m);
    glUniformMatrix4fv(glGetUniformLocation(g_renderer.parallaxInteriorShader, "projection"), 1, GL_FALSE, projection->m);
    glUniform3fv(glGetUniformLocation(g_renderer.parallaxInteriorShader, "viewPos"), 1, &g_engine->camera.position.x);

    for (int i = 0; i < g_scene.numParallaxRooms; ++i) {
        ParallaxRoom* p = &g_scene.parallaxRooms[i];
        if (p->cubemapTexture == 0) continue;

        glUniformMatrix4fv(glGetUniformLocation(g_renderer.parallaxInteriorShader, "model"), 1, GL_FALSE, p->modelMatrix.m);
        glUniform1f(glGetUniformLocation(g_renderer.parallaxInteriorShader, "roomDepth"), p->roomDepth);

        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_CUBE_MAP, p->cubemapTexture);
        glUniform1i(glGetUniformLocation(g_renderer.parallaxInteriorShader, "roomCubemap"), 0);

        glBindVertexArray(g_renderer.parallaxRoomVAO);
        glDrawArrays(GL_TRIANGLES, 0, 6);
    }
    glBindVertexArray(0);
}

void render_refractive_glass(Mat4* view, Mat4* projection) {
    glUseProgram(g_renderer.glassShader);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glDepthMask(GL_FALSE);

    glUniformMatrix4fv(glGetUniformLocation(g_renderer.glassShader, "view"), 1, GL_FALSE, view->m);
    glUniformMatrix4fv(glGetUniformLocation(g_renderer.glassShader, "projection"), 1, GL_FALSE, projection->m);

    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, g_renderer.finalRenderTexture);
    glUniform1i(glGetUniformLocation(g_renderer.glassShader, "sceneTexture"), 0);

    glActiveTexture(GL_TEXTURE1);
    glUniform1i(glGetUniformLocation(g_renderer.glassShader, "normalMap"), 1);

    for (int i = 0; i < g_scene.numBrushes; i++) {
        Brush* b = &g_scene.brushes[i];
        if (strcmp(b->classname, "env_glass") != 0) continue;

        const char* normal_map_name = Brush_GetProperty(b, "normal_map", "NULL");
        Material* normal_mat = TextureManager_FindMaterial(normal_map_name);
        if (normal_mat && normal_mat != &g_MissingMaterial) {
            glBindTexture(GL_TEXTURE_2D, normal_mat->normalMap);
        }
        else {
            glBindTexture(GL_TEXTURE_2D, defaultNormalMapID);
        }

        glUniform1f(glGetUniformLocation(g_renderer.glassShader, "refractionStrength"), atof(Brush_GetProperty(b, "refraction_strength", "0.01")));
        glUniformMatrix4fv(glGetUniformLocation(g_renderer.glassShader, "model"), 1, GL_FALSE, b->modelMatrix.m);
        glBindVertexArray(b->vao);
        glDrawArrays(GL_TRIANGLES, 0, b->totalRenderVertexCount);
    }

    glDepthMask(GL_TRUE);
    glDisable(GL_BLEND);
    glBindVertexArray(0);
}

void render_reflective_glass(Mat4* view, Mat4* projection) {
    if (!Cvar_GetInt("r_planar")) return;

    glUseProgram(g_renderer.reflectiveGlassShader);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glDepthMask(GL_FALSE);

    glUniformMatrix4fv(glGetUniformLocation(g_renderer.reflectiveGlassShader, "view"), 1, GL_FALSE, view->m);
    glUniformMatrix4fv(glGetUniformLocation(g_renderer.reflectiveGlassShader, "projection"), 1, GL_FALSE, projection->m);
    glUniform3fv(glGetUniformLocation(g_renderer.reflectiveGlassShader, "viewPos"), 1, &g_engine->camera.position.x);

    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, g_renderer.reflectionTexture);
    glUniform1i(glGetUniformLocation(g_renderer.reflectiveGlassShader, "reflectionTexture"), 0);

    glActiveTexture(GL_TEXTURE1);
    glBindTexture(GL_TEXTURE_2D, g_renderer.refractionTexture);
    glUniform1i(glGetUniformLocation(g_renderer.reflectiveGlassShader, "refractionTexture"), 1);

    glActiveTexture(GL_TEXTURE2);
    glUniform1i(glGetUniformLocation(g_renderer.reflectiveGlassShader, "normalMap"), 2);

    for (int i = 0; i < g_scene.numBrushes; i++) {
        Brush* b = &g_scene.brushes[i];
        if (strcmp(b->classname, "func_reflective_glass") != 0) continue;

        const char* normal_map_name = Brush_GetProperty(b, "normal_map", "NULL");
        Material* normal_mat = TextureManager_FindMaterial(normal_map_name);
        glBindTexture(GL_TEXTURE_2D, (normal_mat && normal_mat != &g_MissingMaterial) ? normal_mat->normalMap : defaultNormalMapID);

        glUniform1f(glGetUniformLocation(g_renderer.reflectiveGlassShader, "refractionStrength"), atof(Brush_GetProperty(b, "refraction_strength", "0.01")));

        glUniformMatrix4fv(glGetUniformLocation(g_renderer.reflectiveGlassShader, "model"), 1, GL_FALSE, b->modelMatrix.m);
        glBindVertexArray(b->vao);
        glDrawArrays(GL_TRIANGLES, 0, b->totalRenderVertexCount);
    }

    glDepthMask(GL_TRUE);
    glDisable(GL_BLEND);
    glBindVertexArray(0);
}

void render_planar_reflections(Mat4* view, Mat4* projection, const Mat4* sunLightSpaceMatrix, Camera* camera) {
    if (!Cvar_GetInt("r_planar")) return;

    float reflection_plane_height = 0.0;
    bool reflection_plane_found = false;
    for (int i = 0; i < g_scene.numBrushes; ++i) {
        Brush* b = &g_scene.brushes[i];
        if (strcmp(b->classname, "func_water") == 0 || strcmp(b->classname, "func_reflective_glass") == 0) {
            float max_y = -FLT_MAX;
            for (int v = 0; v < b->numVertices; ++v) {
                Vec3 world_v = mat4_mul_vec3(&b->modelMatrix, b->vertices[v].pos);
                if (world_v.y > max_y) {
                    max_y = world_v.y;
                }
            }
            reflection_plane_height = max_y;
            reflection_plane_found = true;
            break;
        }
    }

    if (!reflection_plane_found) {
        return;
    }

    int downsample = Cvar_GetInt("r_planar_downsample");
    if (downsample < 1) downsample = 1;
    int reflection_width = g_engine->width / downsample;
    int reflection_height = g_engine->height / downsample;

    glEnable(GL_CLIP_DISTANCE0);
    glEnable(GL_FRAMEBUFFER_SRGB);

    Camera reflection_camera = *camera;

    float distance = 2 * (reflection_camera.position.y - reflection_plane_height);
    reflection_camera.position.y -= distance;
    reflection_camera.pitch = -reflection_camera.pitch;

    Vec3 f_refl = { cosf(reflection_camera.pitch) * sinf(reflection_camera.yaw), sinf(reflection_camera.pitch), -cosf(reflection_camera.pitch) * cosf(reflection_camera.yaw) };
    vec3_normalize(&f_refl);
    Vec3 t_refl = vec3_add(reflection_camera.position, f_refl);
    Mat4 reflection_view = mat4_lookAt(reflection_camera.position, t_refl, (Vec3) { 0, 1, 0 });

    glUseProgram(g_renderer.mainShader);
    glUniform4f(glGetUniformLocation(g_renderer.mainShader, "clipPlane"), 0, 1, 0, -reflection_plane_height + 0.1f);

    glViewport(0, 0, reflection_width, reflection_height);

    Geometry_RenderPass(&g_renderer, &g_scene, g_engine, &reflection_view, projection, sunLightSpaceMatrix, reflection_camera.position, false);

    glBindFramebuffer(GL_READ_FRAMEBUFFER, g_renderer.gBufferFBO);
    glBindFramebuffer(GL_DRAW_FRAMEBUFFER, g_renderer.reflectionFBO);
    glBlitFramebuffer(0, 0, g_engine->width / GEOMETRY_PASS_DOWNSAMPLE_FACTOR, g_engine->height / GEOMETRY_PASS_DOWNSAMPLE_FACTOR, 0, 0, reflection_width, reflection_height, GL_COLOR_BUFFER_BIT, GL_LINEAR);
    glBlitFramebuffer(0, 0, g_engine->width / GEOMETRY_PASS_DOWNSAMPLE_FACTOR, g_engine->height / GEOMETRY_PASS_DOWNSAMPLE_FACTOR, 0, 0, reflection_width, reflection_height, GL_DEPTH_BUFFER_BIT, GL_NEAREST);

    glBindFramebuffer(GL_FRAMEBUFFER, g_renderer.reflectionFBO);
    glViewport(0, 0, reflection_width, reflection_height);

    Skybox_Render(&g_renderer, &g_scene, g_engine, &reflection_view, projection);

    glUseProgram(g_renderer.mainShader);
    glUniform4f(glGetUniformLocation(g_renderer.mainShader, "clipPlane"), 0, -1, 0, reflection_plane_height);
    glViewport(0, 0, reflection_width, reflection_height);

    Geometry_RenderPass(&g_renderer, &g_scene, g_engine, view, projection, sunLightSpaceMatrix, camera->position, false);

    glBindFramebuffer(GL_READ_FRAMEBUFFER, g_renderer.gBufferFBO);
    glBindFramebuffer(GL_DRAW_FRAMEBUFFER, g_renderer.refractionFBO);
    glBlitFramebuffer(0, 0, g_engine->width / GEOMETRY_PASS_DOWNSAMPLE_FACTOR, g_engine->height / GEOMETRY_PASS_DOWNSAMPLE_FACTOR, 0, 0, reflection_width, reflection_height, GL_COLOR_BUFFER_BIT, GL_LINEAR);
    glBlitFramebuffer(0, 0, g_engine->width / GEOMETRY_PASS_DOWNSAMPLE_FACTOR, g_engine->height / GEOMETRY_PASS_DOWNSAMPLE_FACTOR, 0, 0, reflection_width, reflection_height, GL_DEPTH_BUFFER_BIT, GL_NEAREST);

    glBindFramebuffer(GL_FRAMEBUFFER, g_renderer.refractionFBO);
    glViewport(0, 0, reflection_width, reflection_height);

    Skybox_Render(&g_renderer, &g_scene, g_engine, view, projection);

    glDisable(GL_CLIP_DISTANCE0);
    glDisable(GL_FRAMEBUFFER_SRGB);
    glUseProgram(g_renderer.mainShader);
    glUniform4f(glGetUniformLocation(g_renderer.mainShader, "clipPlane"), 0, 0, 0, 0);
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    glViewport(0, 0, g_engine->width, g_engine->height);
}

static void render_water(Mat4* view, Mat4* projection, const Mat4* sunLightSpaceMatrix) {
    glUseProgram(g_renderer.waterShader);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    glUniformMatrix4fv(glGetUniformLocation(g_renderer.waterShader, "view"), 1, GL_FALSE, view->m);
    glUniformMatrix4fv(glGetUniformLocation(g_renderer.waterShader, "projection"), 1, GL_FALSE, projection->m);
    glUniform3fv(glGetUniformLocation(g_renderer.waterShader, "viewPos"), 1, &g_engine->camera.position.x);
    glUniform1i(glGetUniformLocation(g_renderer.waterShader, "u_debug_reflection"), Cvar_GetInt("r_debug_water_reflection"));

    glUniform1i(glGetUniformLocation(g_renderer.waterShader, "sun.enabled"), g_scene.sun.enabled);
    glUniform3fv(glGetUniformLocation(g_renderer.waterShader, "sun.direction"), 1, &g_scene.sun.direction.x);
    glUniform3fv(glGetUniformLocation(g_renderer.waterShader, "sun.color"), 1, &g_scene.sun.color.x);
    glUniform1f(glGetUniformLocation(g_renderer.waterShader, "sun.intensity"), g_scene.sun.intensity);

    glUniformMatrix4fv(glGetUniformLocation(g_renderer.waterShader, "sunLightSpaceMatrix"), 1, GL_FALSE, sunLightSpaceMatrix->m);
    glUniform1i(glGetUniformLocation(g_renderer.waterShader, "numActiveLights"), g_scene.numActiveLights);
    glUniform1i(glGetUniformLocation(g_renderer.waterShader, "r_lightmaps_bicubic"), Cvar_GetInt("r_lightmaps_bicubic"));
    glUniform1i(glGetUniformLocation(g_renderer.waterShader, "r_debug_lightmaps"), Cvar_GetInt("r_debug_lightmaps"));
    glUniform1i(glGetUniformLocation(g_renderer.waterShader, "r_debug_lightmaps_directional"), Cvar_GetInt("r_debug_lightmaps_directional"));

    Mat4 lightSpaceMatrices[MAX_LIGHTS];
    for (int i = 0; i < g_scene.numActiveLights; ++i) {
        Light* light = &g_scene.lights[i];
        if (light->type == LIGHT_SPOT) {
            float angle_rad = acosf(fmaxf(-1.0f, fminf(1.0f, light->cutOff))); if (angle_rad < 0.01f) angle_rad = 0.01f;
            Mat4 lightProjection = mat4_perspective(angle_rad * 2.0f, 1.0f, 1.0f, light->shadowFarPlane);
            Vec3 up_vector = (Vec3){ 0, 1, 0 }; if (fabs(vec3_dot(light->direction, up_vector)) > 0.99f) { up_vector = (Vec3){ 1, 0, 0 }; }
            Mat4 lightView = mat4_lookAt(light->position, vec3_add(light->position, light->direction), up_vector);
            mat4_multiply(&lightSpaceMatrices[i], &lightProjection, &lightView);
        }
        else {
            mat4_identity(&lightSpaceMatrices[i]);
        }
    }

    glUniform1i(glGetUniformLocation(g_renderer.waterShader, "flashlight.enabled"), g_engine->flashlight_on);
    if (g_engine->flashlight_on) {
        Vec3 forward = { cosf(g_engine->camera.pitch) * sinf(g_engine->camera.yaw), sinf(g_engine->camera.pitch), -cosf(g_engine->camera.pitch) * cosf(g_engine->camera.yaw) };
        vec3_normalize(&forward);
        glUniform3fv(glGetUniformLocation(g_renderer.waterShader, "flashlight.position"), 1, &g_engine->camera.position.x);
        glUniform3fv(glGetUniformLocation(g_renderer.waterShader, "flashlight.direction"), 1, &forward.x);
    }

    glUniform3fv(glGetUniformLocation(g_renderer.waterShader, "cameraPosition"), 1, &g_engine->camera.position.x);
    glUniform1f(glGetUniformLocation(g_renderer.waterShader, "time"), g_engine->scaledTime);

    glActiveTexture(GL_TEXTURE11);
    glBindTexture(GL_TEXTURE_2D, g_renderer.sunShadowMap);
    glUniform1i(glGetUniformLocation(g_renderer.waterShader, "sunShadowMap"), 11);

    glActiveTexture(GL_TEXTURE2);
    glBindTexture(GL_TEXTURE_2D, g_renderer.reflectionTexture);
    glUniform1i(glGetUniformLocation(g_renderer.waterShader, "reflectionTexture"), 2);

    glActiveTexture(GL_TEXTURE4);
    glBindTexture(GL_TEXTURE_2D, g_renderer.refractionTexture);
    glUniform1i(glGetUniformLocation(g_renderer.waterShader, "refractionTexture"), 4);

    glActiveTexture(GL_TEXTURE8);
    glBindTexture(GL_TEXTURE_2D, g_renderer.refractionDepthTexture);
    glUniform1i(glGetUniformLocation(g_renderer.waterShader, "refractionDepthTexture"), 8);

    for (int i = 0; i < g_scene.numBrushes; ++i) {
        Brush* b = &g_scene.brushes[i];
        if (strcmp(b->classname, "func_water") != 0) continue;
        const char* water_def_name = Brush_GetProperty(b, "water_def", "default_water");
        WaterDef* water_def = WaterManager_FindWaterDef(water_def_name);
        if (!water_def) continue;

        Vec3 world_min = { FLT_MAX, FLT_MAX, FLT_MAX };
        Vec3 world_max = { -FLT_MAX, -FLT_MAX, -FLT_MAX };
        if (b->numVertices > 0) {
            for (int v_idx = 0; v_idx < b->numVertices; ++v_idx) {
                Vec3 world_v = mat4_mul_vec3(&b->modelMatrix, b->vertices[v_idx].pos);
                world_min.x = fminf(world_min.x, world_v.x);
                world_min.y = fminf(world_min.y, world_v.y);
                world_min.z = fminf(world_min.z, world_v.z);
                world_max.x = fmaxf(world_max.x, world_v.x);
                world_max.y = fmaxf(world_max.y, world_v.y);
                world_max.z = fmaxf(world_max.z, world_v.z);
            }
            glUniform3fv(glGetUniformLocation(g_renderer.waterShader, "u_waterAabbMin"), 1, &world_min.x);
            glUniform3fv(glGetUniformLocation(g_renderer.waterShader, "u_waterAabbMax"), 1, &world_max.x);
        }

        glActiveTexture(GL_TEXTURE0); glBindTexture(GL_TEXTURE_2D, water_def->dudvMap);
        glActiveTexture(GL_TEXTURE1); glBindTexture(GL_TEXTURE_2D, water_def->normalMap);

        if (b->lightmapAtlas != 0) {
            glUniform1i(glGetUniformLocation(g_renderer.waterShader, "useLightmap"), 1);
            glActiveTexture(GL_TEXTURE12);
            glBindTexture(GL_TEXTURE_2D, b->lightmapAtlas);
            glUniform1i(glGetUniformLocation(g_renderer.waterShader, "lightmap"), 12);
        }
        else {
            glUniform1i(glGetUniformLocation(g_renderer.waterShader, "useLightmap"), 0);
        }

        if (b->directionalLightmapAtlas != 0) {
            glUniform1i(glGetUniformLocation(g_renderer.waterShader, "useDirectionalLightmap"), 1);
            glActiveTexture(GL_TEXTURE13);
            glBindTexture(GL_TEXTURE_2D, b->directionalLightmapAtlas);
            glUniform1i(glGetUniformLocation(g_renderer.waterShader, "directionalLightmap"), 13);
        }
        else {
            glUniform1i(glGetUniformLocation(g_renderer.waterShader, "useDirectionalLightmap"), 0);
        }

        if (water_def->flowMap != 0) {
            glActiveTexture(GL_TEXTURE3);
            glBindTexture(GL_TEXTURE_2D, water_def->flowMap);
            glUniform1i(glGetUniformLocation(g_renderer.waterShader, "flowMap"), 3);
            glUniform1f(glGetUniformLocation(g_renderer.waterShader, "flowSpeed"), water_def->flowSpeed);
            glUniform1i(glGetUniformLocation(g_renderer.waterShader, "useFlowMap"), 1);
        }
        else {
            glUniform1i(glGetUniformLocation(g_renderer.waterShader, "useFlowMap"), 0);
        }

        glUniformMatrix4fv(glGetUniformLocation(g_renderer.waterShader, "model"), 1, GL_FALSE, b->modelMatrix.m);
        glBindVertexArray(b->vao);
        glDrawArrays(GL_TRIANGLES, 0, b->totalRenderVertexCount);
    }
    glBindVertexArray(0);
}

void render_bloom_pass() {
    glUseProgram(g_renderer.bloomShader); glBindFramebuffer(GL_FRAMEBUFFER, g_renderer.bloomFBO);
    glViewport(0, 0, g_engine->width / BLOOM_DOWNSAMPLE, g_engine->height / BLOOM_DOWNSAMPLE);
    glActiveTexture(GL_TEXTURE0); glBindTexture(GL_TEXTURE_2D, g_renderer.gLitColor);
    glBindVertexArray(g_renderer.quadVAO); glDrawArrays(GL_TRIANGLES, 0, 6);
    bool horizontal = true, first_iteration = true; unsigned int amount = 10; glUseProgram(g_renderer.bloomBlurShader);
    for (unsigned int i = 0; i < amount; i++) {
        glBindFramebuffer(GL_FRAMEBUFFER, g_renderer.pingpongFBO[horizontal]); glUniform1i(glGetUniformLocation(g_renderer.bloomBlurShader, "horizontal"), horizontal);
        glActiveTexture(GL_TEXTURE0); glBindTexture(GL_TEXTURE_2D, first_iteration ? g_renderer.bloomBrightnessTexture : g_renderer.pingpongColorbuffers[!horizontal]);
        glBindVertexArray(g_renderer.quadVAO); glDrawArrays(GL_TRIANGLES, 0, 6);
        horizontal = !horizontal; if (first_iteration) first_iteration = false;
    }
    glViewport(0, 0, g_engine->width, g_engine->height);
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

void render_volumetric_pass(Mat4* view, Mat4* projection, const Mat4* sunLightSpaceMatrix) {
    bool should_render_volumetrics = false;
    if (g_scene.sun.enabled && g_scene.sun.volumetricIntensity > 0.001f) {
        should_render_volumetrics = true;
    }
    if (!should_render_volumetrics) {
        for (int i = 0; i < g_scene.numActiveLights; ++i) {
            if (g_scene.lights[i].intensity > 0.001f && g_scene.lights[i].volumetricIntensity > 0.001f) {
                should_render_volumetrics = true;
                break;
            }
        }
    }

    if (!should_render_volumetrics) {
        glBindFramebuffer(GL_FRAMEBUFFER, g_renderer.volumetricFBO);
        glClear(GL_COLOR_BUFFER_BIT);
        glBindFramebuffer(GL_FRAMEBUFFER, g_renderer.volPingpongFBO[0]);
        glClear(GL_COLOR_BUFFER_BIT);
        glBindFramebuffer(GL_FRAMEBUFFER, 0);
        return;
    }
    glBindFramebuffer(GL_FRAMEBUFFER, g_renderer.volumetricFBO);
    glViewport(0, 0, g_engine->width / VOLUMETRIC_DOWNSAMPLE, g_engine->height / VOLUMETRIC_DOWNSAMPLE);
    glClear(GL_COLOR_BUFFER_BIT);

    glUseProgram(g_renderer.volumetricShader);
    glUniform3fv(glGetUniformLocation(g_renderer.volumetricShader, "viewPos"), 1, &g_engine->camera.position.x);

    Mat4 invView, invProj;
    mat4_inverse(view, &invView);
    mat4_inverse(projection, &invProj);
    glUniformMatrix4fv(glGetUniformLocation(g_renderer.volumetricShader, "invView"), 1, GL_FALSE, invView.m);
    glUniformMatrix4fv(glGetUniformLocation(g_renderer.volumetricShader, "invProjection"), 1, GL_FALSE, invProj.m);
    glUniformMatrix4fv(glGetUniformLocation(g_renderer.volumetricShader, "projection"), 1, GL_FALSE, projection->m);
    glUniformMatrix4fv(glGetUniformLocation(g_renderer.volumetricShader, "view"), 1, GL_FALSE, view->m);

    glUniform1i(glGetUniformLocation(g_renderer.volumetricShader, "numActiveLights"), g_scene.numActiveLights);

    glUniform1i(glGetUniformLocation(g_renderer.volumetricShader, "sun.enabled"), g_scene.sun.enabled);
    if (g_scene.sun.enabled) {
        glActiveTexture(GL_TEXTURE15);
        glBindTexture(GL_TEXTURE_2D, g_renderer.sunShadowMap);
        glUniform1i(glGetUniformLocation(g_renderer.volumetricShader, "sunShadowMap"), 15);
        glUniformMatrix4fv(glGetUniformLocation(g_renderer.volumetricShader, "sunLightSpaceMatrix"), 1, GL_FALSE, sunLightSpaceMatrix->m);
        glUniform3fv(glGetUniformLocation(g_renderer.volumetricShader, "sun.direction"), 1, &g_scene.sun.direction.x);
        glUniform3fv(glGetUniformLocation(g_renderer.volumetricShader, "sun.color"), 1, &g_scene.sun.color.x);
        glUniform1f(glGetUniformLocation(g_renderer.volumetricShader, "sun.intensity"), g_scene.sun.intensity);
        glUniform1f(glGetUniformLocation(g_renderer.volumetricShader, "sun.volumetricIntensity"), g_scene.sun.volumetricIntensity / 100.0f);
    }

    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, g_renderer.gPosition);

    glBindVertexArray(g_renderer.quadVAO);
    glDrawArrays(GL_TRIANGLES, 0, 6);

    bool horizontal = true, first_iteration = true;
    unsigned int amount = 4;
    glUseProgram(g_renderer.volumetricBlurShader);
    for (unsigned int i = 0; i < amount; i++) {
        glBindFramebuffer(GL_FRAMEBUFFER, g_renderer.volPingpongFBO[horizontal]);
        glUniform1i(glGetUniformLocation(g_renderer.volumetricBlurShader, "horizontal"), horizontal);
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, first_iteration ? g_renderer.volumetricTexture : g_renderer.volPingpongTextures[!horizontal]);
        glBindVertexArray(g_renderer.quadVAO);
        glDrawArrays(GL_TRIANGLES, 0, 6);
        horizontal = !horizontal;
        if (first_iteration) first_iteration = false;
    }

    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    glViewport(0, 0, g_engine->width, g_engine->height);
}

void render_ssao_pass(Mat4* projection) {
    const int ssao_width = g_engine->width / SSAO_DOWNSAMPLE;
    const int ssao_height = g_engine->height / SSAO_DOWNSAMPLE;
    glViewport(0, 0, ssao_width, ssao_height);
    glBindFramebuffer(GL_FRAMEBUFFER, g_renderer.ssaoFBO);
    glClear(GL_COLOR_BUFFER_BIT);
    glUseProgram(g_renderer.ssaoShader);
    glUniformMatrix4fv(glGetUniformLocation(g_renderer.ssaoShader, "projection"), 1, GL_FALSE, projection->m);
    glUniform2f(glGetUniformLocation(g_renderer.ssaoShader, "screenSize"), (float)ssao_width, (float)ssao_height);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, g_renderer.gPosition);
    glActiveTexture(GL_TEXTURE1);
    glBindTexture(GL_TEXTURE_2D, g_renderer.gGeometryNormal);
    glBindVertexArray(g_renderer.quadVAO);
    glDrawArrays(GL_TRIANGLES, 0, 6);
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    glBindFramebuffer(GL_FRAMEBUFFER, g_renderer.ssaoBlurFBO);
    glClear(GL_COLOR_BUFFER_BIT);
    glUseProgram(g_renderer.ssaoBlurShader);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, g_renderer.ssaoColorBuffer);
    glBindVertexArray(g_renderer.quadVAO);
    glDrawArrays(GL_TRIANGLES, 0, 6);
    glViewport(0, 0, g_engine->width, g_engine->height);
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

void render_ssr_pass(GLuint sourceTexture, GLuint destFBO, Mat4* view, Mat4* projection) {
    if (!Cvar_GetInt("r_ssr")) {
        glBindFramebuffer(GL_READ_FRAMEBUFFER, g_renderer.finalRenderFBO);
        glBindFramebuffer(GL_DRAW_FRAMEBUFFER, destFBO);
        glBlitFramebuffer(0, 0, g_engine->width, g_engine->height, 0, 0, g_engine->width, g_engine->height, GL_COLOR_BUFFER_BIT, GL_NEAREST);
        glBindFramebuffer(GL_FRAMEBUFFER, 0);
        return;
    }

    glBindFramebuffer(GL_FRAMEBUFFER, destFBO);
    glViewport(0, 0, g_engine->width, g_engine->height);
    glClear(GL_COLOR_BUFFER_BIT);
    glDisable(GL_DEPTH_TEST);

    glUseProgram(g_renderer.ssrShader);

    glUniformMatrix4fv(glGetUniformLocation(g_renderer.ssrShader, "projection"), 1, GL_FALSE, projection->m);
    glUniformMatrix4fv(glGetUniformLocation(g_renderer.ssrShader, "view"), 1, GL_FALSE, view->m);

    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, sourceTexture);
    glUniform1i(glGetUniformLocation(g_renderer.ssrShader, "colorBuffer"), 0);

    glActiveTexture(GL_TEXTURE1);
    glBindTexture(GL_TEXTURE_2D, g_renderer.gNormal);
    glUniform1i(glGetUniformLocation(g_renderer.ssrShader, "gNormal"), 1);

    glActiveTexture(GL_TEXTURE2);
    glBindTexture(GL_TEXTURE_2D, g_renderer.gPBRParams);
    glUniform1i(glGetUniformLocation(g_renderer.ssrShader, "ssrValuesMap"), 2);

    glActiveTexture(GL_TEXTURE3);
    glBindTexture(GL_TEXTURE_2D, g_renderer.gPosition);
    glUniform1i(glGetUniformLocation(g_renderer.ssrShader, "gPosition"), 3);

    glBindVertexArray(g_renderer.quadVAO);
    glDrawArrays(GL_TRIANGLES, 0, 6);

    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    glEnable(GL_DEPTH_TEST);
}

void render_lighting_composite_pass(Mat4* view, Mat4* projection) {
    glBindFramebuffer(GL_FRAMEBUFFER, g_renderer.finalRenderFBO);
    glViewport(0, 0, g_engine->width, g_engine->height);
    if (Cvar_GetInt("r_clear")) {
        glClear(GL_COLOR_BUFFER_BIT);
    }
    glUseProgram(g_renderer.postProcessShader);
    glUniform2f(glGetUniformLocation(g_renderer.postProcessShader, "resolution"), g_engine->width, g_engine->height);
    glUniform1f(glGetUniformLocation(g_renderer.postProcessShader, "time"), g_engine->scaledTime);
    glUniform1f(glGetUniformLocation(g_renderer.postProcessShader, "u_exposure"), g_renderer.currentExposure);
    glUniform1f(glGetUniformLocation(g_renderer.postProcessShader, "u_red_flash_intensity"), g_engine->red_flash_intensity);
    LogicEntity* fog_ent = FindActiveEntityByClass(&g_scene, "env_fog");
    if (fog_ent) {
        glUniform1i(glGetUniformLocation(g_renderer.postProcessShader, "u_fogEnabled"), 1);
        Vec3 fog_color;
        sscanf(LogicEntity_GetProperty(fog_ent, "color", "0.5 0.6 0.7"), "%f %f %f", &fog_color.x, &fog_color.y, &fog_color.z);
        glUniform3fv(glGetUniformLocation(g_renderer.postProcessShader, "u_fogColor"), 1, &fog_color.x);
        glUniform1f(glGetUniformLocation(g_renderer.postProcessShader, "u_fogStart"), atof(LogicEntity_GetProperty(fog_ent, "start", "50.0")));
        glUniform1f(glGetUniformLocation(g_renderer.postProcessShader, "u_fogEnd"), atof(LogicEntity_GetProperty(fog_ent, "end", "200.0")));
    }
    else {
        glUniform1i(glGetUniformLocation(g_renderer.postProcessShader, "u_fogEnabled"), 0);
    }
    glUniform1i(glGetUniformLocation(g_renderer.postProcessShader, "u_postEnabled"), g_scene.post.enabled);
    glUniform1f(glGetUniformLocation(g_renderer.postProcessShader, "u_crtCurvature"), g_scene.post.crtCurvature);

    float vignette_strength = Cvar_GetInt("r_vignette") ? g_scene.post.vignetteStrength : 0.0f;
    float scanline_strength = Cvar_GetInt("r_scanline") ? g_scene.post.scanlineStrength : 0.0f;
    float grain_intensity = Cvar_GetInt("r_filmgrain") ? g_scene.post.grainIntensity : 0.0f;
    bool lensflare_enabled = Cvar_GetInt("r_lensflare") && g_scene.post.lensFlareEnabled;
    bool ca_enabled = Cvar_GetInt("r_chromaticabberation") && g_scene.post.chromaticAberrationEnabled;
    bool bw_enabled = Cvar_GetInt("r_black_white") && g_scene.post.bwEnabled;
    bool sharpen_enabled = Cvar_GetInt("r_sharpening") && g_scene.post.sharpenEnabled;
    bool invert_enabled = Cvar_GetInt("r_invert") && g_scene.post.invertEnabled;

    glUniform1f(glGetUniformLocation(g_renderer.postProcessShader, "u_vignetteStrength"), vignette_strength);
    glUniform1f(glGetUniformLocation(g_renderer.postProcessShader, "u_vignetteRadius"), g_scene.post.vignetteRadius);
    glUniform1i(glGetUniformLocation(g_renderer.postProcessShader, "u_lensFlareEnabled"), lensflare_enabled);
    glUniform1f(glGetUniformLocation(g_renderer.postProcessShader, "u_lensFlareStrength"), g_scene.post.lensFlareStrength);
    glUniform1f(glGetUniformLocation(g_renderer.postProcessShader, "u_scanlineStrength"), scanline_strength);
    glUniform1f(glGetUniformLocation(g_renderer.postProcessShader, "u_grainIntensity"), grain_intensity);
    glUniform1i(glGetUniformLocation(g_renderer.postProcessShader, "u_chromaticAberrationEnabled"), ca_enabled);
    glUniform1f(glGetUniformLocation(g_renderer.postProcessShader, "u_chromaticAberrationStrength"), g_scene.post.chromaticAberrationStrength);
    glUniform1i(glGetUniformLocation(g_renderer.postProcessShader, "u_sharpenEnabled"), sharpen_enabled);
    glUniform1f(glGetUniformLocation(g_renderer.postProcessShader, "u_sharpenAmount"), g_scene.post.sharpenAmount);
    glUniform1i(glGetUniformLocation(g_renderer.postProcessShader, "u_bwEnabled"), bw_enabled);
    glUniform1f(glGetUniformLocation(g_renderer.postProcessShader, "u_bwStrength"), g_scene.post.bwStrength);
    glUniform1i(glGetUniformLocation(g_renderer.postProcessShader, "u_invertEnabled"), invert_enabled);
    glUniform1f(glGetUniformLocation(g_renderer.postProcessShader, "u_invertStrength"), g_scene.post.invertStrength);
    glUniform1i(glGetUniformLocation(g_renderer.postProcessShader, "u_isUnderwater"), g_scene.post.isUnderwater);
    glUniform3fv(glGetUniformLocation(g_renderer.postProcessShader, "u_underwaterColor"), 1, &g_scene.post.underwaterColor.x);
    glUniform1i(glGetUniformLocation(g_renderer.postProcessShader, "u_bloomEnabled"), Cvar_GetInt("r_bloom"));
    glUniform1i(glGetUniformLocation(g_renderer.postProcessShader, "u_volumetricsEnabled"), Cvar_GetInt("r_volumetrics"));
    glUniform1i(glGetUniformLocation(g_renderer.postProcessShader, "u_fadeActive"), g_scene.post.fade_active);
    glUniform1f(glGetUniformLocation(g_renderer.postProcessShader, "u_fadeAlpha"), g_scene.post.fade_alpha);
    glUniform3fv(glGetUniformLocation(g_renderer.postProcessShader, "u_fadeColor"), 1, &g_scene.post.fade_color.x);
    bool cc_enabled = Cvar_GetInt("r_colorcorrection") && g_scene.colorCorrection.enabled && g_scene.colorCorrection.lutTexture != 0;
    glUniform1i(glGetUniformLocation(g_renderer.postProcessShader, "u_colorCorrectionEnabled"), cc_enabled);
    if (cc_enabled) {
        glActiveTexture(GL_TEXTURE6);
        glBindTexture(GL_TEXTURE_2D, g_scene.colorCorrection.lutTexture);
        glUniform1i(glGetUniformLocation(g_renderer.postProcessShader, "colorCorrectionLUT"), 6);
    }
    Vec2 light_pos_on_screen = { -2.0, -2.0 }; float flare_intensity = 0.0;
    if (g_scene.numActiveLights > 0) {
        Vec3 light_world_pos = g_scene.lights[0].position; Mat4 view_proj; mat4_multiply(&view_proj, projection, view); float clip_space_pos[4]; float w = 1.0f;
        clip_space_pos[0] = view_proj.m[0] * light_world_pos.x + view_proj.m[4] * light_world_pos.y + view_proj.m[8] * light_world_pos.z + view_proj.m[12] * w;
        clip_space_pos[1] = view_proj.m[1] * light_world_pos.x + view_proj.m[5] * light_world_pos.y + view_proj.m[9] * light_world_pos.z + view_proj.m[13] * w;
        clip_space_pos[2] = view_proj.m[2] * light_world_pos.x + view_proj.m[6] * light_world_pos.y + view_proj.m[10] * light_world_pos.z + view_proj.m[14] * w;
        clip_space_pos[3] = view_proj.m[3] * light_world_pos.x + view_proj.m[7] * light_world_pos.y + view_proj.m[11] * light_world_pos.z + view_proj.m[15] * w;
        float clip_w = clip_space_pos[3];
        if (clip_w > 0) {
            float ndc_x = clip_space_pos[0] / clip_w; float ndc_y = clip_space_pos[1] / clip_w;
            if (ndc_x > -1.0 && ndc_x < 1.0 && ndc_y > -1.0 && ndc_y < 1.0) { light_pos_on_screen.x = ndc_x * 0.5 + 0.5; light_pos_on_screen.y = ndc_y * 0.5 + 0.5; flare_intensity = 1.0; }
            glUniform3fv(glGetUniformLocation(g_renderer.postProcessShader, "u_flareLightWorldPos"), 1, &light_world_pos.x);
            glUniformMatrix4fv(glGetUniformLocation(g_renderer.postProcessShader, "u_view"), 1, GL_FALSE, view->m);
        }
    }
    glUniform2fv(glGetUniformLocation(g_renderer.postProcessShader, "lightPosOnScreen"), 1, &light_pos_on_screen.x);
    glUniform1f(glGetUniformLocation(g_renderer.postProcessShader, "flareIntensity"), flare_intensity);
    glActiveTexture(GL_TEXTURE0); glBindTexture(GL_TEXTURE_2D, g_renderer.gLitColor);
    glActiveTexture(GL_TEXTURE1); glBindTexture(GL_TEXTURE_2D, g_renderer.pingpongColorbuffers[0]);
    glActiveTexture(GL_TEXTURE2); glBindTexture(GL_TEXTURE_2D, g_renderer.gPosition);
    glActiveTexture(GL_TEXTURE3); glBindTexture(GL_TEXTURE_2D, g_renderer.volPingpongTextures[0]);
    if (Cvar_GetInt("r_ssao")) {
        glActiveTexture(GL_TEXTURE4);
        glBindTexture(GL_TEXTURE_2D, g_renderer.ssaoBlurColorBuffer);
    }
    glUniform1i(glGetUniformLocation(g_renderer.postProcessShader, "u_fxaa_enabled"), Cvar_GetInt("r_fxaa"));
    glUniform1i(glGetUniformLocation(g_renderer.postProcessShader, "sceneTexture"), 0);
    glUniform1i(glGetUniformLocation(g_renderer.postProcessShader, "bloomBlur"), 1);
    glUniform1i(glGetUniformLocation(g_renderer.postProcessShader, "gPosition"), 2);
    glUniform1i(glGetUniformLocation(g_renderer.postProcessShader, "volumetricTexture"), 3);
    glUniform1i(glGetUniformLocation(g_renderer.postProcessShader, "ssao"), 4);
    glUniform1i(glGetUniformLocation(g_renderer.postProcessShader, "u_ssaoEnabled"), Cvar_GetInt("r_ssao"));
    glBindVertexArray(g_renderer.quadVAO);
    glDrawArrays(GL_TRIANGLES, 0, 6);
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
    SoundSystem_DeleteBuffer(g_flashlight_sound_buffer);
    SoundSystem_DeleteBuffer(g_footstep_sound_buffer);
    SoundSystem_DeleteBuffer(g_jump_sound_buffer);
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
#ifdef PLATFORM_WINDOWS
    if (g_hMutex) {
        ReleaseMutex(g_hMutex);
        CloseHandle(g_hMutex);
    }
#else
    if (g_lockFileFd != -1) {
        flock(g_lockFileFd, LOCK_UN);
        close(g_lockFileFd);
    }
#endif
    SDL_GL_DeleteContext(g_engine->context);
    SDL_DestroyWindow(g_engine->window);
    IMG_Quit();
    SDL_Quit();
}

void render_autoexposure_pass() {
    bool auto_exposure_enabled = Cvar_GetInt("r_autoexposure");

    glBindBuffer(GL_SHADER_STORAGE_BUFFER, g_renderer.histogramSSBO);
    GLuint zero = 0;
    glClearBufferData(GL_SHADER_STORAGE_BUFFER, GL_R32UI, GL_RED_INTEGER, GL_UNSIGNED_INT, &zero);

    glUseProgram(g_renderer.histogramShader);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, g_renderer.gLitColor);
    glUniform1i(glGetUniformLocation(g_renderer.histogramShader, "u_inputTexture"), 0);
    glDispatchCompute((GLuint)(g_engine->width / 16), (GLuint)(g_engine->height / 16), 1);

    glUseProgram(g_renderer.exposureShader);
    glUniform1f(glGetUniformLocation(g_renderer.exposureShader, "u_autoexposure_key"), Cvar_GetFloat("r_autoexposure_key"));
    glUniform1f(glGetUniformLocation(g_renderer.exposureShader, "u_autoexposure_speed"), Cvar_GetFloat("r_autoexposure_speed"));
    glUniform1f(glGetUniformLocation(g_renderer.exposureShader, "u_deltaTime"), g_engine->deltaTime);
    glUniform1i(glGetUniformLocation(g_renderer.exposureShader, "u_autoexposure_enabled"), auto_exposure_enabled);

    glDispatchCompute(1, 1, 1);

    glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT);

    glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0);
}

void render_dof_pass(GLuint sourceTexture, GLuint sourceDepthTexture, GLuint destFBO) {
    glBindFramebuffer(GL_FRAMEBUFFER, destFBO);
    glDisable(GL_DEPTH_TEST);
    glClear(GL_COLOR_BUFFER_BIT);
    glUseProgram(g_renderer.dofShader);
    glUniform1f(glGetUniformLocation(g_renderer.dofShader, "u_focusDistance"), g_scene.post.dofFocusDistance);
    glUniform1f(glGetUniformLocation(g_renderer.dofShader, "u_aperture"), g_scene.post.dofAperture);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, sourceTexture);
    glUniform1i(glGetUniformLocation(g_renderer.dofShader, "screenTexture"), 0);
    glActiveTexture(GL_TEXTURE1);
    glBindTexture(GL_TEXTURE_2D, sourceDepthTexture);
    glUniform1i(glGetUniformLocation(g_renderer.dofShader, "depthTexture"), 1);
    glBindVertexArray(g_renderer.quadVAO);
    glDrawArrays(GL_TRIANGLES, 0, 6);
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

void render_motion_blur_pass(GLuint sourceTexture, GLuint destFBO) {
    glBindFramebuffer(GL_FRAMEBUFFER, destFBO);
    glDisable(GL_DEPTH_TEST);
    glClear(GL_COLOR_BUFFER_BIT);

    glUseProgram(g_renderer.motionBlurShader);

    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, sourceTexture);
    glUniform1i(glGetUniformLocation(g_renderer.motionBlurShader, "sceneTexture"), 0);

    glActiveTexture(GL_TEXTURE1);
    glBindTexture(GL_TEXTURE_2D, g_renderer.gVelocity);
    glUniform1i(glGetUniformLocation(g_renderer.motionBlurShader, "velocityTexture"), 1);

    glBindVertexArray(g_renderer.quadVAO);
    glDrawArrays(GL_TRIANGLES, 0, 6);
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

void SaveFramebufferToPNG(GLuint fbo, int width, int height, const char* filepath) {
    glBindFramebuffer(GL_FRAMEBUFFER, fbo);
    unsigned char* pixels = (unsigned char*)malloc(width * height * 4);
    if (!pixels) {
        Console_Printf_Error("[ERROR] Failed to allocate memory for screenshot pixels.");
        glBindFramebuffer(GL_FRAMEBUFFER, 0);
        return;
    }

    glReadPixels(0, 0, width, height, GL_RGBA, GL_UNSIGNED_BYTE, pixels);

    SDL_Surface* surface = SDL_CreateRGBSurfaceFrom(pixels, width, height, 32, width * 4,
        0x000000FF, 0x0000FF00, 0x00FF0000, 0xFF000000);

    if (!surface) {
        Console_Printf_Error("[ERROR] Failed to create SDL surface for screenshot.");
    }
    else {
        if (IMG_SavePNG(surface, filepath) != 0) {
            Console_Printf_Error("[ERROR] Failed to save screenshot to %s: %s", filepath, IMG_GetError());
        }
        else {
            Console_Printf("Saved cubemap face to %s", filepath);
        }
        SDL_FreeSurface(surface);
    }

    free(pixels);
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
}
static void SaveScreenshotToPNG(const char* filepath) {
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    unsigned char* pixels = (unsigned char*)malloc(g_engine->width * g_engine->height * 4);
    if (!pixels) {
        Console_Printf_Error("[ERROR] Failed to allocate memory for screenshot pixels.");
        return;
    }

    glReadPixels(0, 0, g_engine->width, g_engine->height, GL_RGBA, GL_UNSIGNED_BYTE, pixels);

    int row_size = g_engine->width * 4;
    unsigned char* temp_row = (unsigned char*)malloc(row_size);
    if (!temp_row) {
        Console_Printf_Error("[ERROR] Failed to allocate memory for screenshot row buffer.");
        free(pixels);
        return;
    }

    for (int y = 0; y < g_engine->width / 2; ++y) {
        unsigned char* top = pixels + y * row_size;
        unsigned char* bottom = pixels + (g_engine->width - 1 - y) * row_size;
        memcpy(temp_row, top, row_size);
        memcpy(top, bottom, row_size);
        memcpy(bottom, temp_row, row_size);
    }
    free(temp_row);

    SDL_Surface* surface = SDL_CreateRGBSurfaceFrom(pixels, g_engine->width, g_engine->height, 32, row_size, 0x000000FF, 0x0000FF00, 0x00FF0000, 0xFF000000);

    if (!surface) {
        Console_Printf_Error("[ERROR] Failed to create SDL surface for screenshot.");
    }
    else {
        if (IMG_SavePNG(surface, filepath) != 0) {
            Console_Printf_Error("[ERROR] Failed to save screenshot to %s: %s", filepath, IMG_GetError());
        }
        else {
            Console_Printf("Screenshot saved to %s", filepath);
        }
        SDL_FreeSurface(surface);
    }
    free(pixels);
}
void BuildCubemaps(int resolution) {
    Console_Printf("Starting cubemap build with %dx%d resolution...", resolution, resolution);
    glFinish();

    Camera original_camera = g_engine->camera;

    Vec3 targets[] = { {1,0,0}, {-1,0,0}, {0,1,0}, {0,-1,0}, {0,0,1}, {0,0,-1} };
    Vec3 ups[] = { {0,-1,0}, {0,-1,0}, {0,0,1}, {0,0,-1}, {0,-1,0}, {0,-1,0} };
    const char* suffixes[] = { "px", "nx", "py", "ny", "pz", "nz" };

    GLuint cubemap_fbo, cubemap_texture, cubemap_rbo;
    glGenFramebuffers(1, &cubemap_fbo);
    glBindFramebuffer(GL_FRAMEBUFFER, cubemap_fbo);
    glGenTextures(1, &cubemap_texture);
    glBindTexture(GL_TEXTURE_2D, cubemap_texture);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_SRGB, resolution, resolution, 0, GL_RGBA, GL_UNSIGNED_BYTE, NULL);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, cubemap_texture, 0);
    glGenRenderbuffers(1, &cubemap_rbo);
    glBindRenderbuffer(GL_RENDERBUFFER, cubemap_rbo);
    glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH24_STENCIL8, resolution, resolution);
    glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_STENCIL_ATTACHMENT, GL_RENDERBUFFER, cubemap_rbo);
    if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE) {
        Console_Printf_Error("[ERROR] Cubemap face FBO not complete!");
        glDeleteFramebuffers(1, &cubemap_fbo);
        glDeleteTextures(1, &cubemap_texture);
        glDeleteRenderbuffers(1, &cubemap_rbo);
        glBindFramebuffer(GL_FRAMEBUFFER, 0);
        return;
    }
    glBindFramebuffer(GL_FRAMEBUFFER, 0);

    for (int i = 0; i < g_scene.numBrushes; ++i) {
        Brush* b = &g_scene.brushes[i];
        if (!strcmp(b->classname, "env_reflectionprobe") == 0) {
            continue;
        }
        if (strlen(b->name) == 0) {
            Console_Printf_Warning("[WARNING] Skipping unnamed reflection probe at index %d.", i);
            continue;
        }

        Console_Printf("Building cubemap for probe '%s'...", b->name);

        for (int face_idx = 0; face_idx < 6; ++face_idx) {
            g_engine->camera.position = b->pos;
            Vec3 target_pos = vec3_add(g_engine->camera.position, targets[face_idx]);
            Mat4 view = mat4_lookAt(g_engine->camera.position, target_pos, ups[face_idx]);
            Mat4 projection = mat4_perspective(90.0f * (M_PI / 180.f), 1.0f, 0.1f, 1000.f);

            Shadows_RenderPointAndSpot(&g_renderer, &g_scene, g_engine);
            Mat4 sunLightSpaceMatrix;
            mat4_identity(&sunLightSpaceMatrix);
            if (g_scene.sun.enabled) {
                Calculate_Sun_Light_Space_Matrix(&sunLightSpaceMatrix, &g_scene.sun, g_engine->camera.position);
                Shadows_RenderSun(&g_renderer, &g_scene, &sunLightSpaceMatrix);
            }

            Geometry_RenderPass(&g_renderer, &g_scene, g_engine, &view, &projection, &sunLightSpaceMatrix, g_engine->camera.position, false);

            glBindFramebuffer(GL_FRAMEBUFFER, cubemap_fbo);
            glViewport(0, 0, resolution, resolution);
            if (Cvar_GetInt("r_clear")) {
                glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
            }

            glEnable(GL_FRAMEBUFFER_SRGB);

            const int LOW_RES_WIDTH = g_engine->width / GEOMETRY_PASS_DOWNSAMPLE_FACTOR;
            const int LOW_RES_HEIGHT = g_engine->height / GEOMETRY_PASS_DOWNSAMPLE_FACTOR;

            glBindFramebuffer(GL_READ_FRAMEBUFFER, g_renderer.gBufferFBO);
            glBindFramebuffer(GL_DRAW_FRAMEBUFFER, cubemap_fbo);
            glBlitFramebuffer(0, 0, LOW_RES_WIDTH, LOW_RES_HEIGHT, 0, 0, resolution, resolution, GL_COLOR_BUFFER_BIT, GL_LINEAR);
            glBlitFramebuffer(0, 0, LOW_RES_WIDTH, LOW_RES_HEIGHT, 0, 0, resolution, resolution, GL_DEPTH_BUFFER_BIT, GL_NEAREST);

            glBindFramebuffer(GL_FRAMEBUFFER, cubemap_fbo);
            Skybox_Render(&g_renderer, &g_scene, g_engine, &view, &projection);

            glDisable(GL_FRAMEBUFFER_SRGB);

            char filepath[256];
            sprintf(filepath, "cubemaps/%s_%s.png", b->name, suffixes[face_idx]);
            SaveFramebufferToPNG(cubemap_fbo, resolution, resolution, filepath);
        }

        const char* face_paths[6];
        char paths_storage[6][256];
        for (int k = 0; k < 6; ++k) {
            sprintf(paths_storage[k], "cubemaps/%s_%s.png", b->name, suffixes[k]);
            face_paths[k] = paths_storage[k];
        }
        b->cubemapTexture = TextureManager_ReloadCubemap(face_paths, b->cubemapTexture);
    }

    glDeleteFramebuffers(1, &cubemap_fbo);
    glDeleteTextures(1, &cubemap_texture);
    glDeleteRenderbuffers(1, &cubemap_rbo);
    glBindFramebuffer(GL_FRAMEBUFFER, 0);

    g_engine->camera = original_camera;
    glViewport(0, 0, g_engine->width, g_engine->height);

    Console_Printf("Cubemap build finished.");
}

void evaluate_animation(SceneObject* obj, float time) {
    if (!obj->model || obj->model->num_animations == 0 || obj->current_animation < 0) return;

    AnimationClip* clip = &obj->model->animations[obj->current_animation];
    cgltf_node* nodes = (cgltf_node*)obj->model->nodes;
    cgltf_size num_nodes = obj->model->num_nodes;
    Skin* skin = (obj->model->num_skins > 0) ? &obj->model->skins[0] : NULL;

    if (!skin) return;

    if (!obj->bone_matrices) {
        obj->bone_matrices = malloc(sizeof(Mat4) * skin->num_joints);
        if (!obj->bone_matrices) return;
    }

    Mat4* local_transforms = (Mat4*)malloc(sizeof(Mat4) * num_nodes);
    if (!local_transforms) return;

    for (size_t i = 0; i < num_nodes; ++i) {
        cgltf_node* node = &nodes[i];
        Vec3 t = { node->translation[0], node->translation[1], node->translation[2] };
        Vec4 r = { node->rotation[0], node->rotation[1], node->rotation[2], node->rotation[3] };
        Vec3 s = { node->scale[0], node->scale[1], node->scale[2] };
        mat4_compose(&local_transforms[i], t, r, s);
    }

    for (int i = 0; i < clip->num_channels; ++i) {
        AnimationChannel* channel = &clip->channels[i];
        AnimationSampler* sampler = &channel->sampler;
        int joint_index = channel->target_joint;

        if (joint_index < 0 || joint_index >= num_nodes) continue;

        size_t frame_idx = 0;
        for (size_t k = 0; k < sampler->num_keyframes - 1; ++k) {
            if (time >= sampler->timestamps[k] && time <= sampler->timestamps[k + 1]) {
                frame_idx = k;
                break;
            }
        }
        if (frame_idx >= sampler->num_keyframes - 1) {
            frame_idx = sampler->num_keyframes > 1 ? sampler->num_keyframes - 2 : 0;
        }

        float t0 = sampler->timestamps[frame_idx];
        float t1 = sampler->timestamps[frame_idx + 1];
        float factor = (t1 > t0) ? (time - t0) / (t1 - t0) : 0.0f;

        cgltf_node* node = &nodes[joint_index];
        Vec3 final_t = { node->translation[0], node->translation[1], node->translation[2] };
        Vec4 final_r = { node->rotation[0], node->rotation[1], node->rotation[2], node->rotation[3] };
        Vec3 final_s = { node->scale[0], node->scale[1], node->scale[2] };

        if (sampler->translations) final_t = vec3_lerp(sampler->translations[frame_idx], sampler->translations[frame_idx + 1], factor);
        if (sampler->rotations) final_r = quat_slerp(sampler->rotations[frame_idx], sampler->rotations[frame_idx + 1], factor);
        if (sampler->scales) final_s = vec3_lerp(sampler->scales[frame_idx], sampler->scales[frame_idx + 1], factor);

        mat4_compose(&local_transforms[joint_index], final_t, final_r, final_s);
    }

    Mat4* global_transforms = (Mat4*)malloc(sizeof(Mat4) * num_nodes);
    if (!global_transforms) {
        free(local_transforms);
        return;
    }

    for (size_t i = 0; i < num_nodes; ++i) {
        cgltf_node* node = &nodes[i];
        if (node->parent) {
            int parent_idx = node->parent - nodes;
            mat4_multiply(&global_transforms[i], &global_transforms[parent_idx], &local_transforms[i]);
        }
        else {
            global_transforms[i] = local_transforms[i];
        }
    }

    for (int i = 0; i < skin->num_joints; ++i) {
        int joint_node_idx = skin->joints[i].joint_index;
        if (joint_node_idx >= 0 && joint_node_idx < num_nodes) {
            Mat4 inv_bind = skin->joints[i].inverse_bind_matrix;
            mat4_multiply(&obj->bone_matrices[i], &global_transforms[joint_node_idx], &inv_bind);
        }
    }

    free(local_transforms);
    free(global_transforms);
}

static void Scene_UpdateAnimations(Scene* scene, float deltaTime) {
    for (int i = 0; i < scene->numObjects; ++i) {
        SceneObject* obj = &scene->objects[i];

        if (!obj->model || obj->model->num_animations == 0) {
            mat4_identity(&obj->animated_local_transform);
            continue;
        }

        if (obj->current_animation == -1) {
            obj->animation_playing = false;
            obj->animation_looping = true;
            obj->animation_time = 0.0f;
            mat4_identity(&obj->animated_local_transform);
            obj->current_animation = 0;
        }

        mat4_identity(&obj->animated_local_transform);

        if (obj->animation_playing) {
            AnimationClip* clip = &obj->model->animations[obj->current_animation];

            if (clip->duration <= 0.0f) {
                continue;
            }

            obj->animation_time += deltaTime;
            if (obj->animation_time > clip->duration) {
                if (obj->animation_looping) {
                    obj->animation_time = fmod(obj->animation_time, clip->duration);
                }
                else {
                    obj->animation_time = clip->duration;
                    obj->animation_playing = false;
                }
            }

            if (obj->model->num_skins > 0) {
                evaluate_animation(obj, obj->animation_time);
            }
            else {
                cgltf_node* target_node = &((cgltf_node*)obj->model->nodes)[0];

                Vec3 anim_t = { target_node->translation[0], target_node->translation[1], target_node->translation[2] };
                Vec4 anim_r = { target_node->rotation[0], target_node->rotation[1], target_node->rotation[2], target_node->rotation[3] };
                Vec3 anim_s = { target_node->scale[0], target_node->scale[1], target_node->scale[2] };

                for (int c = 0; c < clip->num_channels; ++c) {
                    AnimationChannel* channel = &clip->channels[c];
                    AnimationSampler* sampler = &channel->sampler;

                    size_t frame_idx = 0;
                    for (size_t k = 0; k < sampler->num_keyframes - 1; ++k) {
                        if (obj->animation_time >= sampler->timestamps[k] && obj->animation_time <= sampler->timestamps[k + 1]) {
                            frame_idx = k;
                            break;
                        }
                    }
                    if (frame_idx >= sampler->num_keyframes - 1) frame_idx = sampler->num_keyframes > 1 ? sampler->num_keyframes - 2 : 0;

                    float t0 = sampler->timestamps[frame_idx];
                    float t1 = sampler->timestamps[frame_idx + 1];
                    float factor = (t1 > t0) ? (obj->animation_time - t0) / (t1 - t0) : 0.0f;

                    if (sampler->translations) anim_t = vec3_lerp(sampler->translations[frame_idx], sampler->translations[frame_idx + 1], factor);
                    if (sampler->rotations) anim_r = quat_slerp(sampler->rotations[frame_idx], sampler->rotations[frame_idx + 1], factor);
                    if (sampler->scales) anim_s = vec3_lerp(sampler->scales[frame_idx], sampler->scales[frame_idx + 1], factor);
                }

                Mat4 trans_mat = mat4_translate(anim_t);
                Mat4 rot_mat = quat_to_mat4(anim_r);
                Mat4 scale_mat = mat4_scale(anim_s);

                mat4_multiply(&obj->animated_local_transform, &trans_mat, &rot_mat);
                mat4_multiply(&obj->animated_local_transform, &obj->animated_local_transform, &scale_mat);
            }
        }
        else if (obj->model->num_skins > 0) {
            if (!obj->bone_matrices) {
                obj->bone_matrices = malloc(sizeof(Mat4) * obj->model->skins[0].num_joints);
            }
            if (obj->bone_matrices) {
                for (int j = 0; j < obj->model->skins[0].num_joints; ++j) {
                    mat4_identity(&obj->bone_matrices[j]);
                }
            }
        }
    }
}

void ParseCommandLineArgs(int argc, char* argv[]) {
    for (int i = 1; i < argc; ++i) {
        if (_stricmp(argv[i], "-fullscreen") == 0) {
            g_start_fullscreen = true;
            g_start_windowed = false;
        }
        else if (_stricmp(argv[i], "-window") == 0) {
            g_start_windowed = true;
            g_start_fullscreen = false;
        }
        else if (_stricmp(argv[i], "-console") == 0) {
            g_start_with_console = true;
        }
        else if (_stricmp(argv[i], "-dev") == 0) {
            g_dev_mode_requested = true;
        }
        else if (_stricmp(argv[i], "-w") == 0) {
            if (i + 1 < argc) {
                g_startup_width = atoi(argv[++i]);
            }
        }
        else if (_stricmp(argv[i], "-h") == 0) {
            if (i + 1 < argc) {
                g_startup_height = atoi(argv[++i]);
            }
        }
    }
}

static void PreParse_GetResolution(int* width, int* height) {
    FILE* file = fopen("cvars.txt", "r");
    if (!file) {
        return;
    }

    char line[256];
    char name[64];
    char value_str[128];
    while (fgets(line, sizeof(line), file)) {
        if (sscanf(line, "set \"%63[^\"]\" \"%127[^\"]\"", name, value_str) == 2) {
            if (_stricmp(name, "r_width") == 0) {
                *width = atoi(value_str);
            }
            else if (_stricmp(name, "r_height") == 0) {
                *height = atoi(value_str);
            }
        }
    }
    fclose(file);
}

ENGINE_API int Engine_Main(int argc, char* argv[]) {
    ParseCommandLineArgs(argc, argv);
#ifdef ENABLE_CHECKSUM
    char dllPath[1024];
#ifdef PLATFORM_WINDOWS
    HMODULE hModule = NULL;
    GetModuleHandleExA(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT, (LPCSTR)Engine_Main, &hModule);
    GetModuleFileNameA(hModule, dllPath, sizeof(dllPath));
#else
    Dl_info info;
    dladdr((void*)Engine_Main, &info);
    strncpy(dllPath, info.dli_fname, sizeof(dllPath) - 1);
    dllPath[sizeof(dllPath) - 1] = '\0';
#endif
    if (!Checksum_Verify(dllPath)) {
        SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, "Engine Protection Error", "Corrupted game files detected. Please attempt to reinstall.", NULL);
        return 1;
    }
#endif
#ifdef DISABLE_DEBUGGER
    if (CheckForDebugger()) {
        SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, "Security Alert", "Debugger detected! The program will close.", NULL);
        return 1;
    }
#endif
#ifdef PLATFORM_WINDOWS
    const char* mutexName = "TectonicEngine_Instance_Mutex_9A4F";
    g_hMutex = CreateMutex(NULL, TRUE, mutexName);
    if (GetLastError() == ERROR_ALREADY_EXISTS) {
        SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, "Engine Already Running", "An instance of Tectonic Engine is already running.", NULL);
        if (g_hMutex) CloseHandle(g_hMutex);
        return 1;
    }
#else
    const char* lockFilePath = "/tmp/TectonicEngine.lock";
    g_lockFileFd = open(lockFilePath, O_CREAT | O_RDWR, 0666);
    if (g_lockFileFd == -1) {
        SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, "Lock File Error", "Could not create or open the lock file.", NULL);
        return 1;
    }
    if (flock(g_lockFileFd, LOCK_EX | LOCK_NB) == -1) {
        if (errno == EWOULDBLOCK) {
            SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, "Engine Already Running", "An instance of Tectonic Engine is already running.", NULL);
            close(g_lockFileFd);
            return 1;
        }
    }
#endif
    SDL_Init(SDL_INIT_VIDEO); IMG_Init(IMG_INIT_PNG | IMG_INIT_JPG);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 4); SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 5);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);
#ifndef GAME_RELEASE
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_FLAGS, SDL_GL_CONTEXT_DEBUG_FLAG);
#endif
    Uint32 window_flags = SDL_WINDOW_OPENGL;
    if (g_start_fullscreen && !g_start_windowed) {
        window_flags |= SDL_WINDOW_FULLSCREEN_DESKTOP;
    }
    PreParse_GetResolution(&g_startup_width, &g_startup_height);
    for (int i = 1; i < argc; ++i) {
        if (_stricmp(argv[i], "-w") == 0 && i + 1 < argc) g_startup_width = atoi(argv[++i]);
        if (_stricmp(argv[i], "-h") == 0 && i + 1 < argc) g_startup_height = atoi(argv[++i]);
    }
    SDL_Window* window = SDL_CreateWindow("Tectonic Engine", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, g_startup_width, g_startup_height, window_flags);
    SDL_GLContext context = SDL_GL_CreateContext(window);
    glewExperimental = GL_TRUE; glewInit();
    GL_InitDebugOutput();
    init_engine(window, context);

    if (g_start_with_console) {
        Console_Toggle();
    }
    if (g_dev_mode_requested) {
        Cvar_Set("developer", "1");
    }
    if (Cvar_GetInt("r_vsync")) {
        SDL_GL_SetSwapInterval(1);
    }
    else {
        SDL_GL_SetSwapInterval(0);
    }
    if (!GLEW_ARB_bindless_texture) {
        fprintf(stderr, "FATAL ERROR: Your GPU or driver does not support GL_ARB_bindless_texture, which is required.\n");
        SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, "GPU Feature Missing", "Your graphics card does not support bindless textures (GL_ARB_bindless_texture), which is required by this engine.", window);
        return -1;
    }
    SDL_SetRelativeMouseMode(SDL_FALSE);
    MainMenu_SetInGameMenuMode(false, false);

    g_fps_last_update = SDL_GetTicks();
    while (g_engine->running) {
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
        Uint32 frameStartTicks = SDL_GetTicks();
        g_scene.post.fade_active = false;
        g_scene.post.fade_alpha = 0.0f;
        int current_vsync_cvar = Cvar_GetInt("r_vsync");
        if (current_vsync_cvar != g_last_vsync_cvar_state) {
            if (SDL_GL_SetSwapInterval(current_vsync_cvar) == 0) {
                Console_Printf("V-Sync set to %s.", current_vsync_cvar ? "ON" : "OFF");
            }
            else {
                Console_Printf_Warning("[warning] Could not set V-Sync: %s", SDL_GetError());
            }
            g_last_vsync_cvar_state = current_vsync_cvar;
        }
        float currentFrame = (float)SDL_GetTicks() / 1000.0f;
        g_engine->unscaledDeltaTime = currentFrame - g_engine->lastFrame;
        g_engine->lastFrame = currentFrame;

        if (g_engine->unscaledDeltaTime > 0.0f) {
            g_fps_history[g_fps_history_index] = 1.0f / g_engine->unscaledDeltaTime;
            g_fps_history_index = (g_fps_history_index + 1) % FPS_GRAPH_SAMPLES;
        }

        float time_scale_val = Cvar_GetFloat("timescale");
        if (time_scale_val < 0.0f) {
            time_scale_val = 0.0f;
        }
        g_engine->deltaTime = g_engine->unscaledDeltaTime * time_scale_val;
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
        process_input(); update_state();
        if (g_current_mode == MODE_MAINMENU || g_current_mode == MODE_INGAMEMENU) {
            const GameConfig* config = GameConfig_Get();
            if (g_current_mode == MODE_MAINMENU) {
                Discord_Update(config->gamename, "In Main Menu");
            }
            else {
                Discord_Update(config->gamename, "Paused");
            }
            MainMenu_Update(g_engine->unscaledDeltaTime);
            MainMenu_Render();
        }
        else if (g_current_mode == MODE_GAME) {
            char details_str[128];
            sprintf(details_str, "Map: %s", g_scene.mapPath);
            Discord_Update("Playing", details_str);
            Vec3 f = { cosf(g_engine->camera.pitch) * sinf(g_engine->camera.yaw),sinf(g_engine->camera.pitch),-cosf(g_engine->camera.pitch) * cosf(g_engine->camera.yaw) }; vec3_normalize(&f);
            Vec3 t = vec3_add(g_engine->camera.position, f);
            Mat4 view = mat4_lookAt(g_engine->camera.position, t, (Vec3) { 0, 1, 0 });
            if (g_engine->shake_amplitude > 0.0f) {
                float time = g_engine->scaledTime * g_engine->shake_frequency;
                float shake_offset_x = (rand_float_range(-1.0f, 1.0f)) * g_engine->shake_amplitude * 0.015f;
                float shake_offset_y = (rand_float_range(-1.0f, 1.0f)) * g_engine->shake_amplitude * 0.015f;

                Mat4 shake_matrix = mat4_translate((Vec3) { shake_offset_x, shake_offset_y, 0.0f });
                mat4_multiply(&view, &shake_matrix, &view);
            }
            Vec3 vel = Physics_GetLinearVelocity(g_engine->camera.physicsBody);
            float speed = sqrtf(vel.x * vel.x + vel.z * vel.z);
            if (speed > 0.1f) {
                float bob_cycle = g_engine->scaledTime * (Cvar_GetFloat("g_bobcycle") * 5.0f);
                float bob_amt = Cvar_GetFloat("g_bob");

                Mat4 bob_matrix;
                mat4_identity(&bob_matrix);
                bob_matrix.m[13] = -fabs(sin(bob_cycle)) * bob_amt;
                bob_matrix.m[12] = cos(bob_cycle * 2.0f) * bob_amt * 0.5f;

                mat4_multiply(&view, &view, &bob_matrix);
            }
            float fov_degrees = Cvar_GetFloat("fov_vertical");
            Mat4 projection = mat4_perspective(fov_degrees * (M_PI / 180.f), (float)g_engine->width / (float)g_engine->height, 0.1f, 1000.f);
            Mat4 sunLightSpaceMatrix;
            mat4_identity(&sunLightSpaceMatrix);

            if (Cvar_GetInt("r_shadows")) {
                if ((g_frame_counter % 2) == 0) {
                    Shadows_RenderPointAndSpot(&g_renderer, &g_scene, g_engine);
                }

                if (g_scene.sun.enabled) {
                    Calculate_Sun_Light_Space_Matrix(&sunLightSpaceMatrix, &g_scene.sun, g_engine->camera.position);
                    if ((g_frame_counter % 2) == 0) {
                        Shadows_RenderSun(&g_renderer, &g_scene, &sunLightSpaceMatrix);
                    }
                }
            }
            if (Cvar_GetInt("r_planar")) {
                render_planar_reflections(&view, &projection, &sunLightSpaceMatrix, &g_engine->camera);
            }
            Geometry_RenderPass(&g_renderer, &g_scene, g_engine, &view, &projection, &sunLightSpaceMatrix, g_engine->camera.position, false);
            if (Cvar_GetInt("r_ssao")) {
                render_ssao_pass(&projection);
            }
            if (Cvar_GetInt("r_volumetrics")) {
                render_volumetric_pass(&view, &projection, &sunLightSpaceMatrix);
            }
            if (Cvar_GetInt("r_bloom")) {
                render_bloom_pass();
            }
            render_autoexposure_pass();
            render_lighting_composite_pass(&view, &projection);
            glBindFramebuffer(GL_READ_FRAMEBUFFER, g_renderer.gBufferFBO);
            glBindFramebuffer(GL_DRAW_FRAMEBUFFER, g_renderer.finalRenderFBO);
            const int LOW_RES_WIDTH = g_engine->width / GEOMETRY_PASS_DOWNSAMPLE_FACTOR;
            const int LOW_RES_HEIGHT = g_engine->height / GEOMETRY_PASS_DOWNSAMPLE_FACTOR;
            glBlitFramebuffer(0, 0, LOW_RES_WIDTH, LOW_RES_HEIGHT, 0, 0, g_engine->width, g_engine->height, GL_DEPTH_BUFFER_BIT, GL_NEAREST);
            if(Cvar_GetInt("r_skybox")) {
                Skybox_Render(&g_renderer, &g_scene, g_engine, &view, &projection);
            }
            Blackhole_Render(&g_renderer, &g_scene, g_engine, &view, &projection);
            glBindFramebuffer(GL_FRAMEBUFFER, g_renderer.finalRenderFBO);
            render_refractive_glass(&view, &projection);
            render_reflective_glass(&view, &projection);
            for (int i = 0; i < g_scene.numVideoPlayers; ++i) {
                VideoPlayer_Render(&g_scene.videoPlayers[i], &view, &projection);
            }
            glEnable(GL_BLEND);
            glDepthMask(GL_FALSE);
            if (Cvar_GetInt("r_water")) {
                render_water(&view, &projection, &sunLightSpaceMatrix);
            }
            if (Cvar_GetInt("r_particles")) {
                for (int i = 0; i < g_scene.numParticleEmitters; ++i) {
                    ParticleEmitter_Render(&g_scene.particleEmitters[i], view, projection);
                }
            }
            if (Cvar_GetInt("r_sprites")) {
                Sprites_Render(&g_renderer, &g_scene, &view, &projection);
            }
            glDepthMask(GL_TRUE);
            glDisable(GL_BLEND);
            GLuint source_fbo = g_renderer.finalRenderFBO;
            GLuint source_tex = g_renderer.finalRenderTexture;
            if (Cvar_GetInt("r_ssr")) {
                render_ssr_pass(source_tex, g_renderer.postProcessFBO, &view, &projection);
                source_fbo = g_renderer.postProcessFBO;
                source_tex = g_renderer.postProcessTexture;
            }
            if (g_scene.post.dofEnabled && Cvar_GetInt("r_dof")) {
                render_dof_pass(source_tex, g_renderer.finalDepthTexture, g_renderer.postProcessFBO);
                source_fbo = g_renderer.postProcessFBO;
                source_tex = g_renderer.postProcessTexture;
            }

            if (Cvar_GetInt("r_motionblur")) {
                GLuint target_fbo = (source_fbo == g_renderer.finalRenderFBO) ? g_renderer.postProcessFBO : g_renderer.finalRenderFBO;
                render_motion_blur_pass(source_tex, target_fbo);
                source_fbo = target_fbo;
                source_tex = (source_fbo == g_renderer.finalRenderFBO) ? g_renderer.finalRenderTexture : g_renderer.postProcessTexture;
            }

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

            if (!debug_view_active) {
                Renderer_Present(source_fbo, g_engine);
            }
            Overlay_Render(&g_scene, g_engine);
            Mat4 currentViewProjection;
            mat4_multiply(&currentViewProjection, &projection, &view);
            g_renderer.prevViewProjection = currentViewProjection;
        }
        else {
            char details_str[128];
            sprintf(details_str, "Map: %s", g_scene.mapPath);
            Discord_Update("In the Editor", details_str);
            Editor_RenderAllViewports(g_engine, &g_renderer, &g_scene);
            glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
        }
        if (g_current_mode == MODE_MAINMENU || g_current_mode == MODE_INGAMEMENU) {
        }
        else if (g_current_mode == MODE_EDITOR) { Editor_RenderUI(g_engine, &g_scene, &g_renderer); }
        else {
            UI_RenderGameHUD(g_fps_display, g_engine->camera.position.x, g_engine->camera.position.y, g_engine->camera.position.z, g_engine->camera.health, g_fps_history, FPS_GRAPH_SAMPLES);
            UI_RenderDeveloperOverlay();
        }
        Console_Draw(); 
        if (g_screenshot_requested) {
            SaveScreenshotToPNG(g_screenshot_path);
            g_screenshot_requested = false;
        }
        int vsync_enabled = Cvar_GetInt("r_vsync");
        int fps_max = Cvar_GetInt("fps_max");

        if (vsync_enabled == 0 && fps_max > 0) {
            float targetFrameTimeMs = 1000.0f / (float)fps_max;
            Uint32 frameTicks = SDL_GetTicks() - frameStartTicks;
            if (frameTicks < targetFrameTimeMs) {
                SDL_Delay((Uint32)(targetFrameTimeMs - frameTicks));
            }
        }
        g_frame_counter++;
        UI_EndFrame(window);
    }
    cleanup(); return 0;
}
