/*
 * Copyright © 2025 Soft Sprint Studios
 * All rights reserved.
 *
 * This file is proprietary and confidential. Unauthorized reproduction,
 * modification, or distribution is strictly prohibited unless explicit
 * written permission is granted by Soft Sprint Studios.
 */
#include "commands.h"
#include "gl_console.h"
#include "cvar.h"
#include <string.h>
#include <stdlib.h>

#define MAX_COMMANDS 256

static Command g_commands[MAX_COMMANDS];
static int g_num_commands = 0;

void Commands_Init(void) {
    g_num_commands = 0;
    Console_Printf("Command System Initialized.");
}

void Commands_Shutdown(void) {
}

void Commands_Register(const char* name, command_func_t func, const char* description, int flags) {
    if (g_num_commands >= MAX_COMMANDS) {
        Console_Printf_Error("ERROR: Command registration failed, max commands reached.");
        return;
    }
    g_commands[g_num_commands].name = name;
    g_commands[g_num_commands].function = func;
    g_commands[g_num_commands].description = description;
    g_commands[g_num_commands].flags = flags;
    g_num_commands++;
}

void Commands_Execute(int argc, char** argv) {
    if (argc == 0) return;
    char* cmd_name = argv[0];

    for (int i = 0; i < g_num_commands; ++i) {
        if (_stricmp(cmd_name, g_commands[i].name) == 0) {
            if ((g_commands[i].flags & CMD_CHEAT) && Cvar_GetInt("g_cheats") == 0) {
                Console_Printf_Error("Command '%s' is cheat protected.", cmd_name);
                return;
            }
            g_commands[i].function(argc, argv);
            return;
        }
    }

    Cvar* c = Cvar_Find(cmd_name);
    if (c) {
        if (argc >= 2) {
            Cvar_Set(cmd_name, argv[1]);
        }
        else {
            Console_Printf("%s = %s // %s", c->name, c->stringValue, c->helpText);
        }
        return;
    }

    Console_Printf_Error("[error] Unknown command or cvar: %s", cmd_name);
}

int Commands_GetCount() {
    return g_num_commands;
}

const Command* Commands_GetCommand(int index) {
    if (index >= 0 && index < g_num_commands) {
        return &g_commands[index];
    }
    return NULL;
}