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
#include "commands.h"
#include "gl_console.h"
#include "cvar.h"
#include <cstring>
#include <cstdlib>
#include <cstdio>

#define MAX_COMMANDS 256

class Commands {
private:
    static Command g_commands[MAX_COMMANDS];
    static int g_num_commands;

public:
    static void Init() {
        g_num_commands = 0;
        Console_Printf("Command System Initialized.");
    }

    static void Shutdown() {}

    static void Register(const char* name, command_func_t func, const char* description, int flags) {
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

    static void Execute(int argc, char** argv) {
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

    static int GetCount() {
        return g_num_commands;
    }

    static const Command* GetCommand(int index) {
        if (index >= 0 && index < g_num_commands) {
            return &g_commands[index];
        }
        return nullptr;
    }
};

Command Commands::g_commands[MAX_COMMANDS] = {};
int Commands::g_num_commands = 0;

void Commands_Init(void) {
    Commands::Init();
}

void Commands_Shutdown(void) {
    Commands::Shutdown();
}

void Commands_Register(const char* name, command_func_t func, const char* description, int flags) {
    Commands::Register(name, func, description, flags);
}

void Commands_Execute(int argc, char** argv) {
    Commands::Execute(argc, argv);
}

int Commands_GetCount() {
    return Commands::GetCount();
}

const Command* Commands_GetCommand(int index) {
    return Commands::GetCommand(index);
}
