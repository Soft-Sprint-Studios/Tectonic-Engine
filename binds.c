/*
 * Copyright © 2025 Soft Sprint Studios
 * All rights reserved.
 *
 * This file is proprietary and confidential. Unauthorized reproduction,
 * modification, or distribution is strictly prohibited unless explicit
 * written permission is granted by Soft Sprint Studios.
 */
#include "binds.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "gl_console.h"

static KeyBind g_binds[MAX_BINDS];
static int g_num_binds = 0;

void Binds_Init(void) {
    memset(g_binds, 0, sizeof(g_binds));
    g_num_binds = 0;
    Binds_Load("binds.txt");
    Console_Printf("Binds System Initialized.\n");
}

void Binds_Shutdown(void) {
    Binds_Save("binds.txt");
    Console_Printf("Binds System Shutdown.\n");
}

void Binds_Load(const char* filename) {
    FILE* file = fopen(filename, "r");
    if (!file) {
        Console_Printf("No binds.txt found. Creating new one on exit.");
        return;
    }

    g_num_binds = 0;
    char line[256];
    while (fgets(line, sizeof(line), file) && g_num_binds < MAX_BINDS) {
        char keyName[64];
        char command[MAX_COMMAND_LENGTH];

        if (sscanf(line, "bind \"%63[^\"]\" \"%127[^\"]\"", keyName, command) == 2) {
            Binds_Set(keyName, command);
        }
    }
    fclose(file);
    Console_Printf("Loaded %d keybinds from %s", g_num_binds, filename);
}

void Binds_Save(const char* filename) {
    FILE* file = fopen(filename, "w");
    if (!file) {
        Console_Printf("[error] Could not save binds to %s", filename);
        return;
    }

    for (int i = 0; i < g_num_binds; i++) {
        const char* keyName = SDL_GetKeyName(g_binds[i].key);
        if (keyName && strlen(keyName) > 0) {
            fprintf(file, "bind \"%s\" \"%s\"\n", keyName, g_binds[i].command);
        }
    }
    fclose(file);
    Console_Printf("Saved %d keybinds to %s", g_num_binds, filename);
}

void Binds_Set(const char* keyName, const char* command) {
    SDL_Keycode key = SDL_GetKeyFromName(keyName);
    if (key == SDLK_UNKNOWN) {
        Console_Printf("[error] Unknown key name: %s", keyName);
        return;
    }

    for (int i = 0; i < g_num_binds; i++) {
        if (g_binds[i].key == key) {
            strncpy(g_binds[i].command, command, MAX_COMMAND_LENGTH - 1);
            g_binds[i].command[MAX_COMMAND_LENGTH - 1] = '\0';
            Console_Printf("Re-bound '%s' to '%s'", keyName, command);
            return;
        }
    }

    if (g_num_binds < MAX_BINDS) {
        g_binds[g_num_binds].key = key;
        strncpy(g_binds[g_num_binds].command, command, MAX_COMMAND_LENGTH - 1);
        g_binds[g_num_binds].command[MAX_COMMAND_LENGTH - 1] = '\0';
        g_num_binds++;
        Console_Printf("Bound '%s' to '%s'", keyName, command);
    }
    else {
        Console_Printf("[error] Maximum number of binds reached.");
    }
}

void Binds_Unset(const char* keyName) {
    SDL_Keycode key = SDL_GetKeyFromName(keyName);
    if (key == SDLK_UNKNOWN) {
        Console_Printf("[error] Unknown key name: %s", keyName);
        return;
    }

    for (int i = 0; i < g_num_binds; i++) {
        if (g_binds[i].key == key) {
            for (int j = i; j < g_num_binds - 1; j++) {
                g_binds[j] = g_binds[j + 1];
            }
            g_num_binds--;
            Console_Printf("Unbound key '%s'", keyName);
            return;
        }
    }
    Console_Printf("Key '%s' is not bound.", keyName);
}

void Binds_UnbindAll(void) {
    int old_num_binds = g_num_binds;
    g_num_binds = 0;
    memset(g_binds, 0, sizeof(g_binds));
    Console_Printf("Unbound all %d keys.", old_num_binds);
}

const char* Binds_GetCommand(SDL_Keycode key) {
    for (int i = 0; i < g_num_binds; i++) {
        if (g_binds[i].key == key) {
            return g_binds[i].command;
        }
    }
    return NULL;
}