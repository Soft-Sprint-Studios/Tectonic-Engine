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

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    char startmap[128];
    char gamename[128];
} GameConfig;

void GameConfig_Init(void);
const GameConfig* GameConfig_Get(void);

#ifdef __cplusplus
}
#endif

#endif // GAMECONFIG_H