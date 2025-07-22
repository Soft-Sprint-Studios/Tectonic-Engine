/*
 * Copyright © 2025 Soft Sprint Studios
 * All rights reserved.
 *
 * This file is proprietary and confidential. Unauthorized reproduction,
 * modification, or distribution is strictly prohibited unless explicit
 * written permission is granted by Soft Sprint Studios.
 */
#ifndef BINDS_H
#define BINDS_H

//----------------------------------------//
// Brief: Handles "binds.txt"
//----------------------------------------//

#include <SDL_keycode.h>
#include "level1_api.h"

#ifdef __cplusplus
extern "C" {
#endif

#define MAX_BINDS 256
#define MAX_COMMAND_LENGTH 128

typedef struct {
    SDL_Keycode key;
    char command[MAX_COMMAND_LENGTH];
} KeyBind;

LEVEL1_API void Binds_Init(void);
LEVEL1_API void Binds_Shutdown(void);
LEVEL1_API void Binds_Load(const char* filename);
LEVEL1_API void Binds_Save(const char* filename);
LEVEL1_API void Binds_Set(const char* keyName, const char* command);
LEVEL1_API void Binds_Unset(const char* keyName);
LEVEL1_API void Binds_UnbindAll(void);
LEVEL1_API const char* Binds_GetCommand(SDL_Keycode key);

#ifdef __cplusplus
}
#endif

#endif // BINDS_H