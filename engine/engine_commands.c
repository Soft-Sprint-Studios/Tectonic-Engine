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

#include "engine.h"
#include "engine_commands.h"
#include "map.h"
#include "cvar.h"
#include "commands.h"
#include "gl_console.h"
#include "editor.h"
#include "main_menu.h"
#include "network.h"
#include "lightmapper.h"
#include "gl_render_misc.h"
#include <time.h>
#include <errno.h>

#ifdef PLATFORM_LINUX
#include <dirent.h>
#include <sys/stat.h>
#endif

extern Engine* g_engine;
extern Renderer g_renderer;
extern Scene g_scene;
extern EngineMode g_current_mode;
extern int g_last_water_cvar_state;
extern EngineModeTransition g_pending_mode_transition;
extern bool g_is_editor_mode;
extern bool g_screenshot_requested;
extern char g_screenshot_path[256];

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
    MiscRender_BuildCubemaps(&g_renderer, &g_scene, g_engine, resolution);
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

void RegisterEngineCommandsAndCvars(void) {
    init_cvars();
    init_commands();
}