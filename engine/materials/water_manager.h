/*
 * MIT License
 *
 * Copyright (c) 2025 Soft Sprint Studios
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
#ifndef WATER_MANAGER_H
#define WATER_MANAGER_H

//----------------------------------------//
// Brief: Handles "Waters.def" file
//----------------------------------------//

#include "texturemanager.h"
#include "materials_api.h"

#define MAX_WATER_DEFS 64

typedef struct {
    char name[64];
    char normalPath[128];
    char dudvPath[128];
    char flowmapPath[128];
    GLuint normalMap;
    GLuint dudvMap;
    GLuint flowMap;
    float flowSpeed;
} WaterDef;

MATERIALS_API void WaterManager_Init(void);
MATERIALS_API void WaterManager_Shutdown(void);
MATERIALS_API void WaterManager_ParseWaters(const char* filepath);
MATERIALS_API WaterDef* WaterManager_FindWaterDef(const char* name);
MATERIALS_API int WaterManager_GetWaterDefCount(void);
MATERIALS_API WaterDef* WaterManager_GetWaterDef(int index);

#endif