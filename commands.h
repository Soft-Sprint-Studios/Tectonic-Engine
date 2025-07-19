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

#ifdef __cplusplus
extern "C" {
#endif

typedef void (*command_func_t)(int argc, char** argv);

typedef struct {
    const char* name;
    command_func_t function;
    const char* description;
} Command;

void Commands_Init(void);
void Commands_Shutdown(void);
void Commands_Register(const char* name, command_func_t func, const char* description);
void Commands_Execute(int argc, char** argv);
int Commands_GetCount();
const Command* Commands_GetCommand(int index);

#ifdef __cplusplus
}
#endif

#endif