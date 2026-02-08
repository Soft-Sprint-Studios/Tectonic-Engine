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
#ifndef BINDS_H
#define BINDS_H

//----------------------------------------//
// Brief: Handles "binds.txt"
//----------------------------------------//

#include <SDL_keycode.h>
#include "level1_api.h"


    constexpr int MAX_BINDS          = 256;
    #define MAX_COMMAND_LENGTH 128

    typedef struct {
        SDL_Keycode key;
        Char command[MAX_COMMAND_LENGTH];
    } KeyBind;

    LEVEL1_API void Binds_Init(void);
    LEVEL1_API void Binds_Shutdown(void);
    LEVEL1_API void Binds_Load(const Char* filename);
    LEVEL1_API void Binds_Save(const Char* filename);
    LEVEL1_API void Binds_Set(const Char* keyName, const Char* command);
    LEVEL1_API void Binds_Unset(const Char* keyName);
    LEVEL1_API void Binds_UnbindAll(void);
    LEVEL1_API const Char* Binds_GetCommand(SDL_Keycode key);


#endif // BINDS_H