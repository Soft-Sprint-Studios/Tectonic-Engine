/*
 * Copyright © 2025 Soft Sprint Studios
 * All rights reserved.
 *
 * This file is proprietary and confidential. Unauthorized reproduction,
 * modification, or distribution is strictly prohibited unless explicit
 * written permission is granted by Soft Sprint Studios.
 */
#ifndef WATER_MANAGER_H
#define WATER_MANAGER_H

#include "texturemanager.h"

#define MAX_WATER_DEFS 64

typedef struct {
    char name[64];
    char normalPath[128];
    char dudvPath[128];
    GLuint normalMap;
    GLuint dudvMap;
} WaterDef;

void WaterManager_Init(void);
void WaterManager_Shutdown(void);
void WaterManager_ParseWaters(const char* filepath);
WaterDef* WaterManager_FindWaterDef(const char* name);
int WaterManager_GetWaterDefCount(void);
WaterDef* WaterManager_GetWaterDef(int index);

#endif