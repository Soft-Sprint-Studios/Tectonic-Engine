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
#include "gameconfig.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <ctype.h>

static GameConfig g_GameConfig;
bool g_start_fullscreen = false;
bool g_start_windowed = false;
bool g_start_with_console = false;
bool g_dev_mode_requested = false;
int g_startup_width = 1920;
int g_startup_height = 1080;

void GameConfig_Init(void) {
    memset(&g_GameConfig, 0, sizeof(g_GameConfig));
    FILE* file = fopen("gameconf.txt", "r");
    if (!file) {
        Console_Printf("gameconf.txt not found.\n");
        return;
    }

    char line[256];
    while (fgets(line, sizeof(line), file)) {
        char* key = strtok(line, "=");
        char* value = strtok(NULL, "=");

        if (key && value) {
            key = trim(key);
            value = trim(value);

            if (_stricmp(key, "startmap") == 0) {
                strncpy(g_GameConfig.startmap, value, sizeof(g_GameConfig.startmap) - 1);
            }
            else if (_stricmp(key, "gamename") == 0) {
                strncpy(g_GameConfig.gamename, value, sizeof(g_GameConfig.gamename) - 1);
            }
        }
    }
    fclose(file);
}

const GameConfig* GameConfig_Get(void) {
    return &g_GameConfig;
}

void PreParse_GetResolution(int* width, int* height) {
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

void GameConfig_ParseCommandLine(int argc, char* argv[]) {
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