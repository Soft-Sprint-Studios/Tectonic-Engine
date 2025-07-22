/*
 * Copyright © 2025 Soft Sprint Studios
 * All rights reserved.
 *
 * This file is proprietary and confidential. Unauthorized reproduction,
 * modification, or distribution is strictly prohibited unless explicit
 * written permission is granted by Soft Sprint Studios.
 */
#include "gameconfig.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <ctype.h>

static GameConfig g_GameConfig;

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