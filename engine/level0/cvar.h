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

//----------------------------------------//
// Brief: console variables
//----------------------------------------//

#include <stdbool.h>
#include "level0_api.h"

#ifdef __cplusplus
extern "C" {
#endif

#define MAX_CVARS 1024
#define MAX_COMMAND_LENGTH 128

#define CVAR_NONE   (0)
#define CVAR_HIDDEN (1 << 0)
#define CVAR_CHEAT  (1 << 1)

typedef struct {
    char name[64];
    char stringValue[MAX_COMMAND_LENGTH];
    float floatValue;
    int intValue;
    char helpText[128];
    int flags;
} Cvar;

extern Cvar cvar_list[MAX_CVARS];
extern int num_cvars;

LEVEL0_API void Cvar_Init();
LEVEL0_API void Cvar_Load(const char* filename);
LEVEL0_API void Cvar_Save(const char* filename);
LEVEL0_API Cvar* Cvar_Register(const char* name, const char* defaultValue, const char* helpText, int flags);
LEVEL0_API Cvar* Cvar_Find(const char* name);
LEVEL0_API void Cvar_Set(const char* name, const char* value);
LEVEL0_API void Cvar_EngineSet(const char* name, const char* value);
LEVEL0_API float Cvar_GetFloat(const char* name);
LEVEL0_API int Cvar_GetInt(const char* name);
LEVEL0_API const char* Cvar_GetString(const char* name);
LEVEL0_API int Cvar_GetCount();
LEVEL0_API const Cvar* Cvar_GetCvar(int index);

#ifdef __cplusplus
}
#endif

#endif // CVAR_H