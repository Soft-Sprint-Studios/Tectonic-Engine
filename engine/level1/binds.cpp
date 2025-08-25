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
#include "binds.h"
#include "gl_console.h"
#include <cstdio>
#include <cstring>
#include <cstdlib>

class Binds {
private:
    static KeyBind binds[MAX_BINDS];
    static int num_binds;

    static SDL_Keycode GetKeyFromName(const char* keyName) {
        return SDL_GetKeyFromName(keyName);
    }

    static const char* GetKeyName(SDL_Keycode key) {
        return SDL_GetKeyName(key);
    }

public:
    static void Init() {
        memset(binds, 0, sizeof(binds));
        num_binds = 0;
        Load("binds.txt");
        Console_Printf("Binds System Initialized.\n");
    }

    static void Shutdown() {
        Save("binds.txt");
        Console_Printf("Binds System Shutdown.\n");
    }

    static void Load(const char* filename) {
        FILE* file = fopen(filename, "r");
        if (!file) {
            Console_Printf("No binds.txt found. Creating new one on exit.");
            return;
        }

        num_binds = 0;
        char line[256];
        while (fgets(line, sizeof(line), file) && num_binds < MAX_BINDS) {
            char keyName[64];
            char command[MAX_COMMAND_LENGTH];
            if (sscanf(line, "bind \"%63[^\"]\" \"%127[^\"]\"", keyName, command) == 2) {
                Set(keyName, command);
            }
        }
        fclose(file);
        Console_Printf("Loaded %d keybinds from %s", num_binds, filename);
    }

    static void Save(const char* filename) {
        FILE* file = fopen(filename, "w");
        if (!file) {
            Console_Printf_Error("[error] Could not save binds to %s", filename);
            return;
        }

        for (int i = 0; i < num_binds; i++) {
            const char* keyName = GetKeyName(binds[i].key);
            if (keyName && strlen(keyName) > 0) {
                fprintf(file, "bind \"%s\" \"%s\"\n", keyName, binds[i].command);
            }
        }
        fclose(file);
        Console_Printf("Saved %d binds to %s", num_binds, filename);
    }

    static void Set(const char* keyName, const char* command) {
        SDL_Keycode key = GetKeyFromName(keyName);
        if (key == SDLK_UNKNOWN) {
            Console_Printf_Error("[error] Unknown key name: %s", keyName);
            return;
        }

        for (int i = 0; i < num_binds; i++) {
            if (binds[i].key == key) {
                strncpy(binds[i].command, command, MAX_COMMAND_LENGTH - 1);
                binds[i].command[MAX_COMMAND_LENGTH - 1] = '\0';
                Console_Printf("Re-bound '%s' to '%s'", keyName, command);
                return;
            }
        }

        if (num_binds < MAX_BINDS) {
            binds[num_binds].key = key;
            strncpy(binds[num_binds].command, command, MAX_COMMAND_LENGTH - 1);
            binds[num_binds].command[MAX_COMMAND_LENGTH - 1] = '\0';
            num_binds++;
            Console_Printf("Bound '%s' to '%s'", keyName, command);
        }
        else {
            Console_Printf_Error("[error] Maximum number of binds reached.");
        }
    }

    static void Unset(const char* keyName) {
        SDL_Keycode key = GetKeyFromName(keyName);
        if (key == SDLK_UNKNOWN) {
            Console_Printf_Error("[error] Unknown key name: %s", keyName);
            return;
        }

        for (int i = 0; i < num_binds; i++) {
            if (binds[i].key == key) {
                for (int j = i; j < num_binds - 1; j++) {
                    binds[j] = binds[j + 1];
                }
                num_binds--;
                Console_Printf("Unbound key '%s'", keyName);
                return;
            }
        }
        Console_Printf("Key '%s' is not bound.", keyName);
    }

    static void UnbindAll() {
        int old_num = num_binds;
        memset(binds, 0, sizeof(binds));
        num_binds = 0;
        Console_Printf("Unbound all %d keys.", old_num);
    }

    static const char* GetCommand(SDL_Keycode key) {
        for (int i = 0; i < num_binds; i++) {
            if (binds[i].key == key) {
                return binds[i].command;
            }
        }
        return nullptr;
    }
};

KeyBind Binds::binds[MAX_BINDS] = {};
int Binds::num_binds = 0;

void Binds_Init()
{
    Binds::Init();
}

void Binds_Shutdown()
{
    Binds::Shutdown();
}

void Binds_Load(const char* f)
{
    Binds::Load(f);
}

void Binds_Save(const char* f)
{
    Binds::Save(f);
}

void Binds_Set(const char* k, const char* c)
{
    Binds::Set(k, c);
}

void Binds_Unset(const char* k)
{
    Binds::Unset(k);
}

void Binds_UnbindAll()
{
    Binds::UnbindAll();
}

const char* Binds_GetCommand(SDL_Keycode k)
{
    return Binds::GetCommand(k);
}