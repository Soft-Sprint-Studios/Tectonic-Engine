/*
 * Copyright © 2025 Soft Sprint Studios
 * All rights reserved.
 *
 * This file is proprietary and confidential. Unauthorized reproduction,
 * modification, or distribution is strictly prohibited unless explicit
 * written permission is granted by Soft Sprint Studios.
 */
#ifndef COMMANDS_H
#define COMMANDS_H

//----------------------------------------//
// Brief: Concommands are handled here
//----------------------------------------//

#include "level0_api.h"

#ifdef __cplusplus
extern "C" {
#endif

#define CMD_NONE   (0)
#define CMD_CHEAT  (1 << 0)

typedef void (*command_func_t)(int argc, char** argv);

typedef struct {
    const char* name;
    command_func_t function;
    const char* description;
    int flags;
} Command;

LEVEL0_API void Commands_Init(void);
LEVEL0_API void Commands_Shutdown(void);
LEVEL0_API void Commands_Register(const char* name, command_func_t func, const char* description, int flags);
LEVEL0_API void Commands_Execute(int argc, char** argv);
LEVEL0_API int Commands_GetCount();
LEVEL0_API const Command* Commands_GetCommand(int index);

#ifdef __cplusplus
}
#endif

#endif