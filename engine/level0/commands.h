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
#ifndef COMMANDS_H
#define COMMANDS_H

//----------------------------------------//
// Brief: Concommands are handled here
//----------------------------------------//

#include "level0_api.h"


#define CMD_NONE   (0)
#define CMD_CHEAT  (1 << 0)

    typedef void (*command_func_t)(Int argc, Char** argv);

    typedef struct {
        const Char* name;
        command_func_t function;
        const Char* description;
        Int flags;
    } Command;

    LEVEL0_API void Commands_Init(void);
    LEVEL0_API void Commands_Shutdown(void);
    LEVEL0_API void Commands_Register(const Char* name, command_func_t func, const Char* description, Int flags);
    LEVEL0_API void Commands_Execute(Int argc, Char** argv);
    LEVEL0_API Int Commands_GetCount();
    LEVEL0_API const Command* Commands_GetCommand(Int index);


#endif // COMMANDS_H