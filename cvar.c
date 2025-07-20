/*
 * Copyright © 2025 Soft Sprint Studios
 * All rights reserved.
 *
 * This file is proprietary and confidential. Unauthorized reproduction,
 * modification, or distribution is strictly prohibited unless explicit
 * written permission is granted by Soft Sprint Studios.
 */
#include "cvar.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include "compat.h"
#include "gl_console.h"

Cvar cvar_list[MAX_CVARS];
int num_cvars = 0;

void Cvar_Init() {
    memset(cvar_list, 0, sizeof(cvar_list));
    num_cvars = 0;
}

static void Cvar_UpdateValues(Cvar* c) {
    c->floatValue = atof(c->stringValue);
    c->intValue = atoi(c->stringValue);
}

void Cvar_Load(const char* filename) {
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
            Cvar_EngineSet(name, value);
            loaded_count++;
        }
    }
    fclose(file);
    Console_Printf("Loaded %d cvars from %s", loaded_count, filename);
}

void Cvar_Save(const char* filename) {
    FILE* file = fopen(filename, "w");
    if (!file) {
        Console_Printf_Error("[error] Could not save cvars to %s", filename);
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

Cvar* Cvar_Register(const char* name, const char* defaultValue, const char* helpText, int flags) {
    Cvar* c = Cvar_Find(name);
    if (c) {
        strcpy(c->helpText, helpText);
        c->flags = flags;
        return c;
    }

    if (num_cvars >= MAX_CVARS) {
        Console_Printf_Error("ERROR: Max CVars reached!\n");
        return NULL;
    }

    c = &cvar_list[num_cvars++];
    strcpy(c->name, name);
    strcpy(c->stringValue, defaultValue);
    strcpy(c->helpText, helpText);
    c->flags = flags;
    Cvar_UpdateValues(c);

    return c;
}

Cvar* Cvar_Find(const char* name) {
    for (int i = 0; i < num_cvars; ++i) {
        if (_stricmp(cvar_list[i].name, name) == 0) {
            return &cvar_list[i];
        }
    }
    return NULL;
}

void Cvar_Set(const char* name, const char* value) {
    Cvar* c = Cvar_Find(name);
    if (c) {
        if (c->flags & CVAR_HIDDEN) {
            Console_Printf("Cvar '%s' is protected and cannot be modified from the console.", name);
            return;
        }
        if (c->flags & CVAR_CHEAT) {
            Console_Printf_Error("Cvar '%s' is cheat protected.", name);
            return;
        }
        strncpy(c->stringValue, value, MAX_COMMAND_LENGTH - 1);
        c->stringValue[MAX_COMMAND_LENGTH - 1] = '\0';
        Cvar_UpdateValues(c);
        Console_Printf("Cvar '%s' set to '%s'", name, value);
    }
    else {
        Console_Printf_Error("Cvar '%s' not found.", name);
    }
}

void Cvar_EngineSet(const char* name, const char* value) {
    Cvar* c = Cvar_Find(name);
    if (c) {
        strncpy(c->stringValue, value, MAX_COMMAND_LENGTH - 1);
        c->stringValue[MAX_COMMAND_LENGTH - 1] = '\0';
        Cvar_UpdateValues(c);
    }
    else {
        Console_Printf("Cvar '%s' not found.\n", name);
    }
}

float Cvar_GetFloat(const char* name) {
    Cvar* c = Cvar_Find(name);
    return c ? c->floatValue : 0.0f;
}

int Cvar_GetInt(const char* name) {
    Cvar* c = Cvar_Find(name);
    return c ? c->intValue : 0;
}

const char* Cvar_GetString(const char* name) {
    Cvar* c = Cvar_Find(name);
    return c ? c->stringValue : "";
}

int Cvar_GetCount() {
    return num_cvars;
}

const Cvar* Cvar_GetCvar(int index) {
    if (index >= 0 && index < num_cvars) {
        return &cvar_list[index];
    }
    return NULL;
}