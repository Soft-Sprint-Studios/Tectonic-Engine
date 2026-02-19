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
#include "gameconfig.h"
#include "gl_console.h"
#include <cstdio>
#include <cstring>
#include <cstdlib>
#include <cctype>

static GameConfig g_GameConfig;
Bool g_start_fullscreen = false;
Bool g_start_windowed = false;
Bool g_start_with_console = false;
Bool g_dev_mode_requested = false;
Bool g_allow_multiple_instances = false;
Int g_startup_width = 1920;
Int g_startup_height = 1080;

class GameConfigClass {
private:
    static GameConfig config;

public:
    static void Init() {
        memset(&config, 0, sizeof(config));

        FILE* file = fopen("gameconf.txt", "r");
        if (!file) {
            Console_Printf("gameconf.txt not found.\n");
            return;
        }

        Char line[256];
        while (fgets(line, sizeof(line), file)) {
            Char* key = strtok(line, "=");
            Char* value = strtok(nullptr, "=");

            if (key && value) {
                key = Common::trim(key);
                value = Common::trim(value);

                if (_stricmp(key, "startmap") == 0) {
                    strncpy(config.startmap, value, sizeof(config.startmap) - 1);
                }
                else if (_stricmp(key, "gamename") == 0) {
                    strncpy(config.gamename, value, sizeof(config.gamename) - 1);
                }
            }
        }

        fclose(file);
    }

    static const GameConfig* Get() {
        return &config;
    }

    static void PreParseResolution(Int* width, Int* height) {
        FILE* file = fopen("cvars.txt", "r");
        if (!file) return;

        Char line[256];
        Char name[64];
        Char value_str[128];

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

    static void ParseCommandLine(Int argc, Char* argv[]) {
        for (Int i = 1; i < argc; ++i) {
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
            else if (_stricmp(argv[i], "-w") == 0 && i + 1 < argc) {
                g_startup_width = atoi(argv[++i]);
            }
            else if (_stricmp(argv[i], "-h") == 0 && i + 1 < argc) {
                g_startup_height = atoi(argv[++i]);
            }
            else if (_stricmp(argv[i], "-allowmultiple") == 0) {
                g_allow_multiple_instances = true;
            }
            else if (_stricmp(argv[i], "-high") == 0) {
#ifdef PLATFORM_WINDOWS
                SetPriorityClass(GetCurrentProcess(), HIGH_PRIORITY_CLASS);
#endif
            }
            else if (_stricmp(argv[i], "-low") == 0) {
#ifdef PLATFORM_WINDOWS
                SetPriorityClass(GetCurrentProcess(), IDLE_PRIORITY_CLASS);
#endif
            }
            else {
                Console_Printf_Warning("Unknown command line argument: %s ignoring\n", argv[i]);
            }
        }
    }
};

GameConfig GameConfigClass::config = {};

void GameConfig_Init() {
    GameConfigClass::Init();
}

const GameConfig* GameConfig_Get() {
    return GameConfigClass::Get();
}

void PreParse_GetResolution(Int* width, Int* height) {
    GameConfigClass::PreParseResolution(width, height);
}

void GameConfig_ParseCommandLine(Int argc, Char* argv[]) {
    GameConfigClass::ParseCommandLine(argc, argv);
}