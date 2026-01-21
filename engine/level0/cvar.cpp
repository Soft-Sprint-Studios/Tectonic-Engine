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
#include "cvar.h"
#include "gl_console.h"
#include <cstdio>
#include <cstring>
#include <cstdlib>

class CvarManager {
private:
    static Cvar cvar_list[MAX_CVARS];
    static int num_cvars;

    static void UpdateValues(Cvar* c) {
        c->floatValue = atof(c->stringValue);
        c->intValue = atoi(c->stringValue);
    }

public:
    static void Init() {
        memset(cvar_list, 0, sizeof(cvar_list));
        num_cvars = 0;
    }

    static void Load(const char* filename) {
        FILE* file = fopen(filename, "r");
        if (!file) {
            Console_Printf("No %s found. Using default cvar values.", filename);
            return;
        }

        char line[256];
        char name[64];
        char value[128];
        int loaded_count = 0;

        while (fgets(line, sizeof(line), file)) {
            if (sscanf(line, "set \"%63[^\"]\" \"%127[^\"]\"", name, value) == 2) {
                EngineSet(name, value);
                loaded_count++;
            }
        }
        fclose(file);
    }

    static void Save(const char* filename) {
        FILE* file = fopen(filename, "w");
        if (!file) {
            Console_Printf_Error("Could not save cvars to %s", filename);
            return;
        }

        int saved_count = 0;
        for (int i = 0; i < num_cvars; i++) {
            if (!(cvar_list[i].flags & CVAR_HIDDEN)) {
                fprintf(file, "set \"%s\" \"%s\"\n", cvar_list[i].name, cvar_list[i].stringValue);
                saved_count++;
            }
        }
        fclose(file);
        Console_Printf("Saved %d cvars to %s", saved_count, filename);
    }

    static Cvar* Register(const char* name, const char* defaultValue, const char* helpText, int flags) {
        Cvar* c = Find(name);
        if (c) {
            strcpy(c->helpText, helpText);
            c->flags = flags;
            return c;
        }

        if (num_cvars >= MAX_CVARS) {
            Console_Printf_Error("Max CVars reached!\n");
            return nullptr;
        }

        c = &cvar_list[num_cvars++];
        strcpy(c->name, name);
        strcpy(c->stringValue, defaultValue);
        strcpy(c->defaultValue, defaultValue);
        strcpy(c->helpText, helpText);
        c->flags = flags;
        UpdateValues(c);

        return c;
    }

    static Cvar* Find(const char* name) {
        for (int i = 0; i < num_cvars; ++i) {
            if (_stricmp(cvar_list[i].name, name) == 0) {
                return &cvar_list[i];
            }
        }
        return nullptr;
    }

    static void Set(const char* name, const char* value) {
        Cvar* c = Find(name);
        if (c) {
            if ((c->flags & CVAR_CHEAT) && GetInt("g_cheats") == 0) {
                Console_Printf_Error("Cvar '%s' is cheat protected.", name);
                return;
            }
            strncpy(c->stringValue, value, MAX_COMMAND_LENGTH - 1);
            c->stringValue[MAX_COMMAND_LENGTH - 1] = '\0';
            UpdateValues(c);
            Console_Printf("Cvar '%s' set to '%s'", name, value);
        }
        else {
            Console_Printf_Error("Cvar '%s' not found.", name);
        }
    }

    static void EngineSet(const char* name, const char* value) {
        Cvar* c = Find(name);
        if (c) {
            strncpy(c->stringValue, value, MAX_COMMAND_LENGTH - 1);
            c->stringValue[MAX_COMMAND_LENGTH - 1] = '\0';
            UpdateValues(c);
        }
        else {
            Console_Printf("Cvar '%s' not found.\n", name);
        }
    }

    static float GetFloat(const char* name) {
        Cvar* c = Find(name);
        return c ? c->floatValue : 0.0f;
    }

    static int GetInt(const char* name) {
        Cvar* c = Find(name);
        return c ? c->intValue : 0;
    }

    static const char* GetString(const char* name) {
        Cvar* c = Find(name);
        return c ? c->stringValue : "";
    }

    static int GetCount() {
        return num_cvars;
    }

    static const Cvar* GetCvar(int index) {
        if (index >= 0 && index < num_cvars) {
            return &cvar_list[index];
        }
        return nullptr;
    }
};

Cvar CvarManager::cvar_list[MAX_CVARS] = {};
int CvarManager::num_cvars = 0;

void Cvar_Init() {
    CvarManager::Init();
}

void Cvar_Load(const char* filename) {
    CvarManager::Load(filename);
}

void Cvar_Save(const char* filename) {
    CvarManager::Save(filename);
}

Cvar* Cvar_Register(const char* name, const char* defaultValue, const char* helpText, int flags) {
    return CvarManager::Register(name, defaultValue, helpText, flags);
}

Cvar* Cvar_Find(const char* name) {
    return CvarManager::Find(name);
}

void Cvar_Set(const char* name, const char* value) {
    CvarManager::Set(name, value);
}

void Cvar_EngineSet(const char* name, const char* value) {
    CvarManager::EngineSet(name, value);
}

float Cvar_GetFloat(const char* name) {
    return CvarManager::GetFloat(name);
}

int Cvar_GetInt(const char* name) {
    return CvarManager::GetInt(name);
}

const char* Cvar_GetString(const char* name) {
    return CvarManager::GetString(name);
}

int Cvar_GetCount() {
    return CvarManager::GetCount();
}

const Cvar* Cvar_GetCvar(int index) {
    return CvarManager::GetCvar(index);
}
