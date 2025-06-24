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

Cvar* Cvar_Register(const char* name, const char* defaultValue, const char* helpText) {
    Cvar* c = Cvar_Find(name);
    if (c) {
        strcpy(c->stringValue, defaultValue);
        strcpy(c->helpText, helpText);
        Cvar_UpdateValues(c);
        return c;
    }

    if (num_cvars >= MAX_CVARS) {
        printf("ERROR: Max CVars reached!\n");
        return NULL;
    }

    c = &cvar_list[num_cvars++];
    strcpy(c->name, name);
    strcpy(c->stringValue, defaultValue);
    strcpy(c->helpText, helpText);
    Cvar_UpdateValues(c);

    printf("Registered Cvar: %s (default: \"%s\")\n", name, defaultValue);
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
        strcpy(c->stringValue, value);
        Cvar_UpdateValues(c);
    }
    else {
        printf("Cvar '%s' not found.\n", name);
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