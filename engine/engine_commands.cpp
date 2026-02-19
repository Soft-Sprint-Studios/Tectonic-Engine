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
#include "engine_commands.h"
#include "map.h"
#include "cvar.h"
#include "binds.h"
#include "commands.h"
#include "gl_console.h"
#include "editor.h"
#include "main_menu.h"
#include "network.h"
#include "lightmapper.h"
#include "gl_render_misc.h"
#include "gl_loading_screen.h"
#include "io_system.h"
#include <time.h>
#include <errno.h>
#include "minispec.h"

extern Engine* g_engine;
extern Renderer g_renderer;
extern Scene g_scene;
extern EngineMode g_current_mode;
extern Int g_last_water_cvar_state;
extern EngineModeTransition g_pending_mode_transition;
extern Bool g_is_editor_mode;
extern Bool g_screenshot_requested;
extern Char g_screenshot_path[256];

void Cmd_Edit(Int argc, Char** argv) {
    if (g_current_mode == MODE_GAME) {
        g_last_water_cvar_state = Cvar_GetInt("r_water");
        Cvar_Set("r_water", "0");
        g_engine->flashlight_on = false;
        g_pending_mode_transition = TRANSITION_TO_EDITOR;
    }
    else if (g_current_mode == MODE_EDITOR) {
        if (g_last_water_cvar_state != -1) {
            Char val[2];
            sprintf(val, "%d", g_last_water_cvar_state);
            Cvar_Set("r_water", val);
        }
        g_pending_mode_transition = TRANSITION_TO_GAME;
    }
}

void Cmd_Quit(Int argc, Char** argv) {
    g_quit_requested = true;
}

void Cmd_Restart(Int argc, Char** argv) {
    g_restart_requested = true;
    Cvar_EngineSet("engine_running", "0");
}

void Cmd_SetPos(Int argc, Char** argv) {
    if (argc == 4) {
        Float x = atof(argv[1]);
        Float y = atof(argv[2]);
        Float z = atof(argv[3]);
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

void Cmd_Noclip(Int argc, Char** argv) {
    Cvar* c = Cvar_Find("noclip");
    if (c) {
        Bool currently_noclip = c->intValue;
        Cvar_Set("noclip", currently_noclip ? "0" : "1");
        if (currently_noclip) {
            Physics_Teleport(g_engine->camera.physicsBody, g_engine->camera.position);
        }
    }
}

void Cmd_Bind(Int argc, Char** argv) {
    if (argc == 3) {
        Binds_Set(argv[1], argv[2]);
    }
    else {
        Console_Printf("Usage: bind \"key\" \"command\"");
    }
}

void Cmd_Unbind(Int argc, Char** argv) {
    if (argc == 2) {
        Binds_Unset(argv[1]);
    }
    else {
        Console_Printf("Usage: unbind \"key\"");
    }
}

void Cmd_UnbindAll(Int argc, Char** argv) {
    Binds_UnbindAll();
}

void Cmd_Map(Int argc, Char** argv) {
    if (argc == 2) {
        g_current_mode = MODE_MAINMENU;
        SDL_SetRelativeMouseMode(SDL_FALSE);
        Char map_path[256];
        snprintf(map_path, sizeof(map_path), "%s.map", argv[1]);
        Console_Printf("Loading map: %s", map_path);

        LoadingScreen_Show(argv[1]);
        LoadingScreen_Render();
        SDL_GL_SwapWindow(g_engine->window);

        if (Scene_LoadMap(&g_scene, &g_renderer, map_path, g_engine)) {
            g_current_mode = MODE_GAME;
            SDL_SetRelativeMouseMode(SDL_TRUE);
        }
        else {
            Console_Printf_Error("Failed to load map: %s", map_path);
        }
    }
    else {
        Console_Printf("Usage: map <mapname>");
    }

    LoadingScreen_Hide();
}

void Cmd_Maps(Int argc, Char** argv) {
    Console_Printf("Available maps in root directory:");

    Int count = 0;
    const Char* exts[] = { ".map" };
    Char** files = IO_ScanDirectory("./", exts, 1, &count);

    if (count == 0) {
        Console_Printf("...No maps found.");
    }
    else {
        for (Int i = 0; i < count; ++i) {
            Console_Printf("  %s", files[i]);
        }

        Console_Printf("%d maps found total.", count);

        IO_FreeFileList(files, count);
    }
}

void Cmd_Disconnect(Int argc, Char** argv) {
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

void Cmd_Download(Int argc, Char** argv) {
    if (argc == 2 && strncmp(argv[1], "http", 4) == 0) {
        const Char* url = argv[1];
        const Char* filename_start = strrchr(url, '/');
        if (filename_start) {
            filename_start++;
        }
        else {
            filename_start = url;
        }
        _mkdir("downloads");
        Char output_path[256];
        snprintf(output_path, sizeof(output_path), "downloads/%s", filename_start);
        Console_Printf("Starting download for %s...", url);
        Network_DownloadFile(url, output_path);
    }
    else {
        Console_Printf("Usage: download http://... or https://...");
    }
}

void Cmd_Ping(Int argc, Char** argv) {
    if (argc == 2) {
        Console_Printf("Pinging %s...", argv[1]);
        Network_Ping(argv[1]);
    }
    else {
        Console_Printf("Usage: ping <hostname>");
    }
}

void Cmd_Screenshot(Int argc, Char** argv) {
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

void Cmd_Echo(Int argc, Char** argv) {
    if (argc < 2) {
        Console_Printf("Usage: echo <message>");
        return;
    }

    Char message[1024] = { 0 };
    for (Int i = 1; i < argc; i++) {
        strcat(message, argv[i]);
        if (i < argc - 1) {
            strcat(message, " ");
        }
    }
    Console_Printf("%s", message);
}

void Cmd_Clear(Int argc, Char** argv) {
    Console_ClearLog();
}

void Cmd_Help(Int argc, Char** argv) {
    Console_Printf("--- Command List ---");
    for (Int i = 0; i < Commands_GetCount(); ++i) {
        const Command* cmd = Commands_GetCommand(i);
        if (cmd) {
            Console_Printf("%s - %s", cmd->name, cmd->description);
        }
    }
    Console_Printf("--- CVAR List ---");
    Console_Printf("To set a cvar, type: <cvar_name> <value>");
    for (Int i = 0; i < Cvar_GetCount(); i++) {
        const Cvar* c = Cvar_GetCvar(i);
        if (c && !(c->flags & CVAR_HIDDEN)) {
            Console_Printf("%s - %s (current: \"%s\")", c->name, c->helpText, c->stringValue);
        }
    }
    Console_Printf("--------------------");
}

void Cmd_Exec(Int argc, Char** argv) {
    if (argc != 2) {
        Console_Printf("Usage: exec <filename>");
        return;
    }

    const Char* filename = argv[1];
    FILE* file = fopen(filename, "r");
    if (!file) {
        Console_Printf_Error("Could not open script file: %s", filename);
        return;
    }

    Console_Printf("Executing script: %s", filename);
    Char line[512];
    while (fgets(line, sizeof(line), file)) {
        Char* trimmed_line = trim(line);
        if (strlen(trimmed_line) == 0 || trimmed_line[0] == '/' || trimmed_line[0] == '#') {
            continue;
        }

        Char* cmd_copy = new Char[strlen(trimmed_line) + 1];
        strcpy(cmd_copy, trimmed_line);
        constexpr int MAX_ARGS = 32;
        Int exec_argc = 0;
        Char* exec_argv[MAX_ARGS];

        Char* p = strtok(cmd_copy, " ");
        while (p != nullptr && exec_argc < MAX_ARGS) {
            exec_argv[exec_argc++] = p;
            p = strtok(nullptr, " ");
        }

        if (exec_argc > 0) {
            Commands_Execute(exec_argc, exec_argv);
        }
        delete[] cmd_copy;
    }

    fclose(file);
    Console_Printf("Finished executing script: %s", filename);
}

void Cmd_Version(Int argc, Char** argv) {
    Console_Printf("Build: %d (%s, %s)", Compat_GetBuildNumber(), __DATE__, __TIME__);
}

void Cmd_SaveGame(Int argc, Char** argv) {
    if (g_current_mode != MODE_GAME && g_current_mode != MODE_EDITOR && g_current_mode != MODE_INGAMEMENU) {
        Console_Printf_Error("Can only save when a map is loaded.");
        return;
    }

    if (argc != 2) {
        Console_Printf("Usage: save <savename>");
        return;
    }

    Char savePath[256];
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

void Cmd_LoadGame(Int argc, Char** argv) {
    if (argc != 2) {
        Console_Printf("Usage: load <savename>");
        return;
    }

    Char savePath[256];
    snprintf(savePath, sizeof(savePath), "saves/%s.sav", argv[1]);

    Console_Printf("Loading game from %s...", savePath);

    LoadingScreen_Show(nullptr);
    LoadingScreen_Render();
    SDL_GL_SwapWindow(g_engine->window);

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

    LoadingScreen_Hide();
}

void Cmd_ScreenShake(Int argc, Char** argv) {
    if (argc < 4) {
        Console_Printf("Usage: screenshake <amplitude> <frequency> <duration>");
        return;
    }

    g_engine->shake_amplitude = atof(argv[1]);
    g_engine->shake_frequency = atof(argv[2]);
    g_engine->shake_duration_timer = atof(argv[3]);
}

void Cmd_Condump(Int argc, Char** argv) {
    Char filename[32];
    FILE* file = nullptr;

    for (Int i = 0; i < 1000; ++i) {
        snprintf(filename, sizeof(filename), "condump%03d.txt", i);
        file = fopen(filename, "r");
        if (file == nullptr) {
            break;
        }
        fclose(file);
        file = nullptr;
    }

    if (file != nullptr) {
        fclose(file);
    }

    file = fopen(filename, "w");
    if (!file) {
        Console_Printf_Error("Could not open %s for writing.", filename);
        return;
    }

    Int count = 0;
    const ConsoleItem* items = Console_GetLogItems(&count);

    for (Int i = 0; i < count; ++i) {
        fprintf(file, "%s\n", items[i].text);
    }

    fclose(file);
    Console_Printf("Console dumped to %s", filename);
}

void Cmd_PlayerPosition(Int argc, Char** argv) {
    if (g_current_mode == MODE_GAME) {
        Console_Printf("Current local player position: %f %f %f.\n", g_engine->camera.position.x, g_engine->camera.position.y, g_engine->camera.position.z);
    }
    else {
        Console_Printf("Not currently in a map.");
    }
}

void Cmd_HurtMe(Int argc, Char** argv) {
    if (argc < 2) {
        Console_Printf("Usage: hurtme <damage>");
        return;
    }

    if (Cvar_GetInt("god")) {
        Console_Printf("God Mode is enabled.");
        return;
    }

    Float damage = (Float)atof(argv[1]);
    g_engine->camera.health -= damage;
}

void Cmd_Kill(Int argc, Char** argv) {
    if (Cvar_GetInt("god")) {
        Console_Printf("God Mode is enabled.");
        return;
    }

    g_engine->camera.health = 0.0f;
}

void Cmd_Skyname(Int argc, Char** argv) {
    if (argc != 2) {
        Console_Printf("Usage: skyname <sky>");
        return;
    }

    const Char* new_skybox_name = argv[1];

    if (g_scene.skybox_cubemap) {
        glDeleteTextures(1, &g_scene.skybox_cubemap);
        g_scene.skybox_cubemap = 0;
    }

    strncpy(g_scene.skybox_path, new_skybox_name, sizeof(g_scene.skybox_path) - 1);
    g_scene.skybox_path[sizeof(g_scene.skybox_path) - 1] = '\0';

    const Char* suffixes[] = { "_px.png", "_nx.png", "_py.png", "_ny.png", "_pz.png", "_nz.png" };
    Char face_paths[6][256];
    const Char* face_pointers[6];
    Bool all_files_exist = true;

    for (Int i = 0; i < 6; ++i) {
        snprintf(face_paths[i], sizeof(face_paths[i]), "skybox/%s%s", g_scene.skybox_path, suffixes[i]);
        face_pointers[i] = face_paths[i];

        FILE* f = fopen(face_paths[i], "rb");
        if (f) {
            fclose(f);
        }
        else {
            all_files_exist = false;
            Console_Printf_Error("Skybox texture not found: %s", face_paths[i]);
        }
    }

    if (all_files_exist) {
        g_scene.skybox_cubemap = loadCubemap(face_pointers);
        g_scene.use_cubemap_skybox = true;
        Console_Printf("Skybox changed to '%s'", new_skybox_name);
    }
    else {
        Console_Printf_Error("Failed to load skybox '%s', one or more faces missing.", new_skybox_name);
        g_scene.use_cubemap_skybox = false;
        g_scene.skybox_path[0] = '\0';
    }
}

void Cmd_Reset(Int argc, Char** argv) {
    if (argc != 2) {
        Console_Printf("Usage: reset <cvar>\n");
        return;
    }

    const Char* cvar_name = argv[1];
    Cvar* c = Cvar_Find(cvar_name);

    if (c) {
        Cvar_Set(c->name, c->defaultValue);
    }
    else {
        Console_Printf_Error("Cvar '%s' not found.\n", cvar_name);
    }
}

void Cmd_Inc_f(Int argc, Char** argv) {
    if (argc < 2 || argc > 3) {
        Console_Printf("Usage: inc <cvar> [amount]\n");
        return;
    }

    const Char* cvar_name = argv[1];
    Cvar* c = Cvar_Find(cvar_name);
    if (!c) {
        Console_Printf_Error("Cvar '%s' not found.\n", cvar_name);
        return;
    }

    Float increment_amount = (argc == 3) ? (Float)atof(argv[2]) : 1.0f;
    Float new_value = Cvar_GetFloat(cvar_name) + increment_amount;

    Char new_value_str[128];
    snprintf(new_value_str, sizeof(new_value_str), "%g", new_value);
    Cvar_Set(cvar_name, new_value_str);
}

void Cmd_Dec_f(Int argc, Char** argv) {
    if (argc < 2 || argc > 3) {
        Console_Printf("Usage: dec <cvar> [amount]\n");
        return;
    }

    const Char* cvar_name = argv[1];
    Cvar* c = Cvar_Find(cvar_name);
    if (!c) {
        Console_Printf_Error("Cvar '%s' not found.\n", cvar_name);
        return;
    }

    Float decrement_amount = (argc == 3) ? (Float)atof(argv[2]) : 1.0f;
    Float new_value = Cvar_GetFloat(cvar_name) - decrement_amount;

    Char new_value_str[128];
    snprintf(new_value_str, sizeof(new_value_str), "%g", new_value);
    Cvar_Set(cvar_name, new_value_str);
}

void Cmd_Set(Int argc, Char** argv) {
    if (argc != 2 && argc != 3) {
        Console_Printf("Usage: set <variable> [value]\n");
        return;
    }

    const Char* cvar_name = argv[1];
    Cvar* c = Cvar_Find(cvar_name);
    if (!c) {
        Console_Printf_Error("Cvar '%s' not found.\n", cvar_name);
        return;
    }

    if (argc == 3) {
        Cvar_Set(cvar_name, argv[2]);
    }
    else {
        Console_Printf("\"%s\" is \"%s\" (default: \"%s\") // %s", c->name, c->stringValue, c->defaultValue, c->helpText);
    }
}

void init_cvars() {
    Cvar_Register("con_enable", "1", "Enables the ` key to toggle the console.", CVAR_HIDDEN);
    Cvar_Register("developer", "0", "Show developer console log on screen (0=off, 1=on)", CVAR_CHEAT);
    Cvar_Register("s_volume", "2.5", "Master volume for the game (0.0 to 4.0)", CVAR_NONE);
    Cvar_Register("s_mute", "0", "Global mute switch (0=off, 1=on)", CVAR_NONE);
    Cvar_Register("noclip", "0", "Enable noclip mode (0=off, 1=on)", CVAR_CHEAT);
    Cvar_Register("god", "0", "Enable god mode (player is invulnerable).", CVAR_CHEAT);
    Cvar_Register("gravity", "9.81", "World gravity value", CVAR_NONE);
    Cvar_Register("engine_running", "1", "Engine state (0=off, 1=on)", CVAR_HIDDEN);
    Cvar_Register("watermark", "1", "Show engine watermark (0=off, 1=on)", CVAR_NONE);
    Cvar_Register("r_width", "1920", "Screen width in pixels", CVAR_NONE);
    Cvar_Register("r_height", "1080", "Screen height in pixels", CVAR_NONE);
    Cvar_Register("r_monitor", "0", "Selects which monitor the game window appears on (0 = primary).", CVAR_NONE);
    Cvar_Register("r_gamma", "2.2", "Screen gamma correction value.", CVAR_NONE);
    Cvar_Register("r_geometry_downsample", "1.1", "Downsample factor for geometry pass, usefull for low res aesthetics/higher performance (e.g., 2 = 1/4 resolution)", CVAR_NONE);
    Cvar_Register("r_anisotropy", "16.0", "Anisotropic filtering level (0.0 to 16.0).", CVAR_NONE);
    Cvar_Register("r_mipmapping", "1", "Enable texture mipmapping (0=off, 1=on). Requires map reload.", CVAR_NONE);
    Cvar_Register("r_autoexposure", "1", "Enable auto-exposure (0=off, 1=on)", CVAR_NONE);
    Cvar_Register("r_autoexposure_speed", "0.5", "Auto-exposure adaptation speed", CVAR_NONE);
    Cvar_Register("r_autoexposure_key", "0.18", "Auto-exposure middle-grey value", CVAR_NONE);
    Cvar_Register("r_ssao", "1", "Enable SSAO (0=off, 1=on)", CVAR_NONE);
    Cvar_Register("r_ssao_downsample", "2", "Downsample factor for ssao (e.g., 2 = 1/4 resolution)", CVAR_NONE);
    Cvar_Register("r_bloom", "1", "Enable bloom (0=off, 1=on)", CVAR_NONE);
    Cvar_Register("r_bloom_downsample", "2", "Downsample factor for bloom (e.g., 2 = 1/4 resolution)", CVAR_NONE);
    Cvar_Register("r_volumetrics", "1", "Enable volumetric lighting (0=off, 1=on)", CVAR_NONE);
    Cvar_Register("r_volumetrics_downsample", "2", "Downsample factor for volumetrics (e.g., 2 = 1/4 resolution)", CVAR_NONE);
    Cvar_Register("r_volumetrics_steps", "1024", "Volumetric lighting number of steps", CVAR_NONE);
    Cvar_Register("r_faceculling", "1", "Enable back-face culling (0=off, 1=on)", CVAR_NONE);
    Cvar_Register("r_zprepass", "0", "Enable Z-prepass (0=off, 1=on)", CVAR_NONE);
    Cvar_Register("r_wireframe", "0", "Render in wireframe mode (0=off, 1=on)", CVAR_NONE);
    Cvar_Register("r_fullbright", "0", "Render scene with full brightness, ignoring lighting.", CVAR_NONE);
    Cvar_Register("r_shadows", "1", "Enable dynamic shadows (0=off, 1=on)", CVAR_NONE);
    Cvar_Register("r_shadow_distance_max", "100.0", "Max shadow casting distance", CVAR_NONE);
    Cvar_Register("r_shadow_map_size", "1024", "Shadow map resolution", CVAR_NONE);
    Cvar_Register("r_relief_mapping", "1", "Enable relief mapping (0=off, 1=on)", CVAR_NONE);
    Cvar_Register("r_relief_max_steps", "12.0", "Relief Mapping: Maximum initial steps (quality)", CVAR_NONE);
    Cvar_Register("r_relief_min_steps", "1.0", "Relief Mapping: Minimum initial steps (performance)", CVAR_NONE);
    Cvar_Register("r_relief_refine_steps", "6", "Relief Mapping: Refinement steps (accuracy)", CVAR_NONE);
    Cvar_Register("r_cubemaps", "1", "Enable environment mapping reflections (0=off, 1=on)", CVAR_NONE);
    Cvar_Register("r_colorcorrection", "1", "Enable color correction (0=off, 1=on)", CVAR_NONE);
    Cvar_Register("r_vignette", "1", "Enable vignette (0=off, 1=on)", CVAR_NONE);
    Cvar_Register("r_chromaticabberation", "1", "Enable chromatic aberration (0=off, 1=on)", CVAR_NONE);
    Cvar_Register("r_scanline", "1", "Enable scanline effect (0=off, 1=on)", CVAR_NONE);
    Cvar_Register("r_filmgrain", "1", "Enable film grain (0=off, 1=on)", CVAR_NONE);
    Cvar_Register("r_lensflare", "1", "Enable lens flare (0=off, 1=on)", CVAR_NONE);
    Cvar_Register("r_black_white", "1", "Enable black and white effect (0=off, 1=on)", CVAR_NONE);
    Cvar_Register("r_sharpening", "1", "Enable sharpening (0=off, 1=on)", CVAR_NONE);
    Cvar_Register("r_invert", "1", "Enable color invert (0=off, 1=on)", CVAR_NONE);
    Cvar_Register("r_vsync", "1", "Enable vertical sync (0=off, 1=on)", CVAR_NONE);
    Cvar_Register("r_fxaa", "1", "Enable depth-based anti-aliasing (0=off, 1=on)", CVAR_NONE);
    Cvar_Register("r_clear", "0", "Clear the screen every frame (0=off, 1=on)", CVAR_NONE);
    Cvar_Register("r_skybox", "1", "Enable skybox (0=off, 1=on)", CVAR_NONE);
    Cvar_Register("r_particles", "1", "Enable particles (0=off, 1=on)", CVAR_NONE);
    Cvar_Register("r_particles_cull_dist", "75.0", "Particle culling distance", CVAR_NONE);
    Cvar_Register("r_particles_soft", "1", "Enable soft particles (0=off, 1=on)", CVAR_NONE);
    Cvar_Register("r_sprites", "1", "Enable sprites (0=off, 1=on)", CVAR_NONE);
    Cvar_Register("r_water", "1", "Enable water rendering (0=off, 1=on)", CVAR_NONE);
    Cvar_Register("r_planar", "1", "Enable planar reflections for water and reflective glass (0=off, 1=on)", CVAR_NONE);
    Cvar_Register("r_planar_downsample", "2", "Downsample factor for planar reflections/refractions (e.g., 2 = 1/4 resolution)", CVAR_NONE);
    Cvar_Register("r_lightmaps_bicubic", "1", "Enable Bicubic lightmap filtering (0=off, 1=on)", CVAR_NONE);
    Cvar_Register("fps_max", "300", "Max FPS (0=unlimited)", CVAR_NONE);
    Cvar_Register("show_fps", "0", "Show FPS counter (0=off, 1=on)", CVAR_NONE);
    Cvar_Register("r_showgraph", "0", "Show framerate graph (0=off, 1=on)", CVAR_NONE);
    Cvar_Register("r_speeds", "0", "Show rendering statistics (0=off, 1=on)", CVAR_NONE);
    Cvar_Register("show_pos", "0", "Show player position (0=off, 1=on)", CVAR_NONE);
    Cvar_Register("show_health", "1", "Show player health (0=off, 1=on)", CVAR_NONE);
    Cvar_Register("r_debug_albedo", "0", "Show albedo buffer (0=off, 1=on)", CVAR_NONE);
    Cvar_Register("r_debug_normals", "0", "Show normals buffer (0=off, 1=on)", CVAR_NONE);
    Cvar_Register("r_debug_position", "0", "Show position buffer (0=off, 1=on)", CVAR_NONE);
    Cvar_Register("r_debug_metallic", "0", "Show metallic buffer (0=off, 1=on)", CVAR_NONE);
    Cvar_Register("r_debug_roughness", "0", "Show roughness buffer (0=off, 1=on)", CVAR_NONE);
    Cvar_Register("r_debug_ao", "0", "Show ambient occlusion buffer (0=off, 1=on)", CVAR_NONE);
    Cvar_Register("r_debug_volumetric", "0", "Show volumetric buffer (0=off, 1=on)", CVAR_NONE);
    Cvar_Register("r_debug_bloom", "0", "Show bloom mask (0=off, 1=on)", CVAR_NONE);
    Cvar_Register("r_debug_lightmaps", "0", "Show lightmap buffer (0=off, 1=on)", CVAR_NONE);
    Cvar_Register("r_debug_lightmaps_directional", "0", "Show directional lightmap buffer (0=off, 1=on)", CVAR_NONE);
    Cvar_Register("r_debug_vertex_light", "0", "Show baked vertex lighting buffer (0=off, 1=on)", CVAR_NONE);
    Cvar_Register("r_debug_vertex_light_directional", "0", "Show baked directional vertex lighting buffer (0=off, 1=on)", CVAR_NONE);
    Cvar_Register("r_debug_water_reflection", "0", "Forces water to show pure reflection texture (0=off, 1=on)", CVAR_NONE);
    Cvar_Register("r_sun_shadow_distance", "50.0", "Sun shadow frustum size", CVAR_NONE);
    Cvar_Register("r_texture_quality", "5", "Texture quality (1=very low to 5=very high)", CVAR_NONE);
    Cvar_Register("fov_vertical", "55", "Vertical field of view (degrees)", CVAR_NONE);
    Cvar_Register("g_speed", "6.0", "Player walking speed", CVAR_NONE);
    Cvar_Register("g_noclip_speed", "20.0", "Noclip speed", CVAR_NONE);
    Cvar_Register("g_sprint_speed", "8.0", "Player sprinting speed", CVAR_NONE);
    Cvar_Register("g_accel", "15.0", "Player acceleration", CVAR_NONE);
    Cvar_Register("g_friction", "2.0", "Player friction", CVAR_NONE);
    Cvar_Register("g_jump_force", "350.0", "Player jump force", CVAR_NONE);
    Cvar_Register("g_bob", "0.01", "The amount of view bobbing.", CVAR_NONE);
    Cvar_Register("g_bobcycle", "0.8", "The speed of the view bobbing.", CVAR_NONE);
    Cvar_Register("g_drawhud", "1", "Enable the hud (0=off, 1=on)", CVAR_NONE);
    Cvar_Register("g_roll_angle", "0.5", "Maximum angle to roll view when strafing", CVAR_NONE);
    Cvar_Register("g_roll_speed", "8.0", "Speed of view rolling interpolation", CVAR_NONE);
    Cvar_Register("g_crouch_height", "1.37", "Player height when crouching", CVAR_NONE);
    Cvar_Register("g_sprint_fov", "10.0", "Additional FOV added when sprinting", CVAR_NONE);
    Cvar_Register("g_sprint_fov_speed", "5.0", "Speed of FOV interpolation", CVAR_NONE);
    Cvar_Register("g_zoom_fov", "20.0", "FOV when zoomed in (degrees).", CVAR_NONE);
    Cvar_Register("g_zoom_speed", "10.0", "Speed of the zoom in/out transition.", CVAR_NONE);
    Cvar_Register("g_map_backup_path", "", "Directory to store timestamped map backups on save. e.g., 'D:/maps/backups'", CVAR_NONE);
#ifdef GAME_RELEASE
    Cvar_Register("g_cheats", "0", "Enable cheats (0=off, 1=on)", CVAR_NONE);
#else
    Cvar_Register("g_cheats", "1", "Enable cheats (0=off, 1=on)", CVAR_NONE);
#endif
    Cvar_Register("crosshair", "1", "Enable crosshair (0=off, 1=on)", CVAR_NONE);
    Cvar_Register("timescale", "1.0", "Game speed scale", CVAR_CHEAT);
    Cvar_Register("sensitivity", "1.0", "Mouse sensitivity.", CVAR_NONE);
    Cvar_Register("p_disable_deactivation", "0", "Disables physics objects sleeping. (0=off, 1=on).", CVAR_NONE);
    Cvar_Register("in_rawinput", "1", "Enable raw mouse input. (0=off, 1=on)", CVAR_NONE);
}

void init_commands() {
    Commands_Register("help", Cmd_Help, "Shows a list of all available commands and cvars.", CMD_NONE);
    Commands_Register("cmdlist", Cmd_Help, "Alias for the 'help' command.", CMD_NONE);
    Commands_Register("condump", Cmd_Condump, "Dump the contents of the console into a condump file", CMD_NONE);
    Commands_Register("skyname", Cmd_Skyname, "Changes the skybox cubemap. Usage: skyname <basename>", CMD_CHEAT);
    Commands_Register("edit", Cmd_Edit, "Toggles editor mode.", CMD_NONE);
    Commands_Register("screenshake", Cmd_ScreenShake, "Applies a screen shake effect. Usage: screenshake <amplitude> <frequency> <duration>", CMD_CHEAT);
    Commands_Register("quit", Cmd_Quit, "Exits the engine.", CMD_NONE);
    Commands_Register("restart", Cmd_Restart, "Restarts the engine.", CMD_NONE);
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
    Commands_Register("download", Cmd_Download, "Downloads a file from a URL.", CMD_NONE);
    Commands_Register("ping", Cmd_Ping, "Pings a network host to check connectivity.", CMD_NONE);
    Commands_Register("screenshot", Cmd_Screenshot, "Saves a screenshot to disk.", CMD_NONE);
    Commands_Register("exec", Cmd_Exec, "Executes a script file from the root directory.", CMD_NONE);
    Commands_Register("version", Cmd_Version, "Displays engine and map version information.", CMD_NONE);
    Commands_Register("echo", Cmd_Echo, "Prints a message to the console.", CMD_NONE);
    Commands_Register("clear", Cmd_Clear, "Clears the console text.", CMD_NONE);
    Commands_Register("reset", Cmd_Reset, "Resets a cvar to its default value.", CMD_NONE);
    Commands_Register("inc", Cmd_Inc_f, "Increments the value of a cvar.", CMD_NONE);
    Commands_Register("dec", Cmd_Dec_f, "Decrements the value of a cvar.", CMD_NONE);
    Commands_Register("set", Cmd_Set, "Sets or displays the value of a cvar.", CMD_NONE);
    Commands_Register("pos", Cmd_PlayerPosition, "Position of the player in the world.", CMD_NONE);
    Commands_Register("hurtme", Cmd_HurtMe, "Hurts the player. Usage: hurtme <damage>", CMD_CHEAT);
    Commands_Register("kill", Cmd_Kill, "Kills the player.", CMD_CHEAT);
}

void PrintSystemInfo() {
    const Char* vendor = minispec_cpu_vendor();
    const Char* brand = minispec_cpu_brand();
    LongLong ram_bytes = minispec_memory_bytes();

    Console_Printf("CPU Vendor: %s\n", vendor);
    Console_Printf("CPU Brand:  %s\n", brand);
    Console_Printf("RAM: %llu MB\n", ram_bytes / (1024 * 1024));
}

void RegisterEngineCommandsAndCvars(void) {
    init_cvars();
    init_commands();
}