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

extern void Cmd_Edit(int argc, char** argv);
extern void Cmd_Quit(int argc, char** argv);
extern void Cmd_SetPos(int argc, char** argv);
extern void Cmd_Noclip(int argc, char** argv);
extern void Cmd_Bind(int argc, char** argv);
extern void Cmd_Unbind(int argc, char** argv);
extern void Cmd_Map(int argc, char** argv);
extern void Cmd_Maps(int argc, char** argv);
extern void Cmd_Disconnect(int argc, char** argv);
extern void Cmd_Download(int argc, char** argv);
extern void Cmd_Ping(int argc, char** argv);
extern void Cmd_BuildCubemaps(int argc, char** argv);
extern void Cmd_Help(int argc, char** argv);
extern void Cmd_UnbindAll(int argc, char** argv);
extern void Cmd_Screenshot(int argc, char** argv);
extern void Cmd_Exec(int argc, char** argv);
extern void Cmd_Echo(int argc, char** argv);

void Commands_Init(void) {
    g_num_commands = 0;

    Commands_Register("help", Cmd_Help, "Shows a list of all available commands and cvars.");
    Commands_Register("cmdlist", Cmd_Help, "Alias for the 'help' command.");
    Commands_Register("edit", Cmd_Edit, "Toggles editor mode.");
    Commands_Register("quit", Cmd_Quit, "Exits the engine.");
    Commands_Register("exit", Cmd_Quit, "Alias for the 'quit' command.");
    Commands_Register("setpos", Cmd_SetPos, "Teleports the player to a specified XYZ coordinate.");
    Commands_Register("noclip", Cmd_Noclip, "Toggles player collision and gravity.");
    Commands_Register("bind", Cmd_Bind, "Binds a key to a command.");
    Commands_Register("unbind", Cmd_Unbind, "Removes a key binding.");
    Commands_Register("unbindall", Cmd_UnbindAll, "Removes all key bindings.");
    Commands_Register("map", Cmd_Map, "Loads the specified map.");
    Commands_Register("maps", Cmd_Maps, "Lists all available .map files in the root directory.");
    Commands_Register("disconnect", Cmd_Disconnect, "Disconnects from the current map and returns to the main menu.");
    Commands_Register("download", Cmd_Download, "Downloads a file from a URL.");
    Commands_Register("ping", Cmd_Ping, "Pings a network host to check connectivity.");
    Commands_Register("build_cubemaps", Cmd_BuildCubemaps, "Builds cubemaps for all reflection probes. Usage: build_cubemaps [resolution]");
    Commands_Register("screenshot", Cmd_Screenshot, "Saves a screenshot to disk.");
    Commands_Register("exec", Cmd_Exec, "Executes a script file from the root directory.");
    Commands_Register("echo", Cmd_Echo, "Prints a message to the console.");

    Console_Printf("Command System Initialized. Registered %d commands.", g_num_commands);
}

void Commands_Shutdown(void) {
}

void Commands_Register(const char* name, command_func_t func, const char* description) {
    if (g_num_commands >= MAX_COMMANDS) {
        Console_Printf_Error("ERROR: Command registration failed, max commands reached.");
        return;
    }
    g_commands[g_num_commands].name = name;
    g_commands[g_num_commands].function = func;
    g_commands[g_num_commands].description = description;
    g_num_commands++;
}

void Commands_Execute(int argc, char** argv) {
    if (argc == 0) return;
    char* cmd_name = argv[0];

    for (int i = 0; i < g_num_commands; ++i) {
        if (_stricmp(cmd_name, g_commands[i].name) == 0) {
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

void Cmd_Help(int argc, char** argv) {
    Console_Printf("--- Command List ---");
    for (int i = 0; i < g_num_commands; ++i) {
        Console_Printf("%s - %s", g_commands[i].name, g_commands[i].description);
    }
    Console_Printf("--- CVAR List ---");
    Console_Printf("To set a cvar, type: <cvar_name> <value>");
    for (int i = 0; i < num_cvars; i++) {
        Cvar* c = &cvar_list[i];
        if (c->flags & CVAR_HIDDEN) {
            continue;
        }
        Console_Printf("%s - %s (current: \"%s\")", c->name, c->helpText, c->stringValue);
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