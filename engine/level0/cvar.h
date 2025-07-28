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