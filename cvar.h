/*
 * Copyright © 2025 Soft Sprint Studios
 * All rights reserved.
 *
 * This file is proprietary and confidential. Unauthorized reproduction,
 * modification, or distribution is strictly prohibited unless explicit
 * written permission is granted by Soft Sprint Studios.
 */
#ifndef CVAR_H
#define CVAR_H

#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

#define MAX_CVARS 1024

#define CVAR_NONE   (0)
#define CVAR_HIDDEN (1 << 0)

typedef struct {
    char name[64];
    char stringValue[128];
    float floatValue;
    int intValue;
    char helpText[128];
    int flags;
} Cvar;

extern Cvar cvar_list[MAX_CVARS];
extern int num_cvars;

void Cvar_Init();
Cvar* Cvar_Register(const char* name, const char* defaultValue, const char* helpText, int flags);
Cvar* Cvar_Find(const char* name);
void Cvar_Set(const char* name, const char* value);
void Cvar_EngineSet(const char* name, const char* value);
float Cvar_GetFloat(const char* name);
int Cvar_GetInt(const char* name);
const char* Cvar_GetString(const char* name);

#ifdef __cplusplus
}
#endif

#endif // CVAR_H