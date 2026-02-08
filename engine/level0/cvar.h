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
#pragma once
#ifndef CVAR_H
#define CVAR_H

//----------------------------------------//
// Brief: console variables
//----------------------------------------//

#include <stdbool.h>
#include "level0_api.h"


    constexpr int MAX_CVARS = 1024;
    constexpr int MAX_COMMAND_LENGTH = 128;

    enum {
        CVAR_NONE = (0),
        CVAR_HIDDEN = (1 << 0),
        CVAR_CHEAT = (1 << 1)
    };

    typedef struct {
        Char name[64];
        Char stringValue[MAX_COMMAND_LENGTH];
        Char defaultValue[MAX_COMMAND_LENGTH];
        Float floatValue;
        Int intValue;
        Char helpText[128];
        Int flags;
    } Cvar;

    extern Cvar cvar_list[MAX_CVARS];
    extern Int num_cvars;

    LEVEL0_API void Cvar_Init();
    LEVEL0_API void Cvar_Load(const Char* filename);
    LEVEL0_API void Cvar_Save(const Char* filename);
    LEVEL0_API Cvar* Cvar_Register(const Char* name, const Char* defaultValue, const Char* helpText, Int flags);
    LEVEL0_API Cvar* Cvar_Find(const Char* name);
    LEVEL0_API void Cvar_Set(const Char* name, const Char* value);
    LEVEL0_API void Cvar_EngineSet(const Char* name, const Char* value);
    LEVEL0_API Float Cvar_GetFloat(const Char* name);
    LEVEL0_API Int Cvar_GetInt(const Char* name);
    LEVEL0_API const Char* Cvar_GetString(const Char* name);
    LEVEL0_API Int Cvar_GetCount();
    LEVEL0_API const Cvar* Cvar_GetCvar(Int index);


#endif // CVAR_H