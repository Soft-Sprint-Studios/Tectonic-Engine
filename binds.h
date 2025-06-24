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

#include <SDL_keycode.h>

#define MAX_BINDS 256
#define MAX_COMMAND_LENGTH 128

typedef struct {
    SDL_Keycode key;
    char command[MAX_COMMAND_LENGTH];
} KeyBind;

void Binds_Init(void);
void Binds_Shutdown(void);
void Binds_Load(const char* filename);
void Binds_Save(const char* filename);
void Binds_Set(const char* keyName, const char* command);
const char* Binds_GetCommand(SDL_Keycode key);

#endif // BINDS_H