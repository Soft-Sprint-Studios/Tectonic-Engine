/*
 * Copyright © 2025 Soft Sprint Studios
 * All rights reserved.
 *
 * This file is proprietary and confidential. Unauthorized reproduction,
 * modification, or distribution is strictly prohibited unless explicit
 * written permission is granted by Soft Sprint Studios.
 */
#ifndef GAMECONFIG_H
#define GAMECONFIG_H

//----------------------------------------//
// Brief: The "gameconf.txt"
//----------------------------------------//

#include "level0_api.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    char startmap[128];
    char gamename[128];
} GameConfig;

LEVEL0_API void GameConfig_Init(void);
LEVEL0_API const GameConfig* GameConfig_Get(void);

#ifdef __cplusplus
}
#endif

#endif // GAMECONFIG_H