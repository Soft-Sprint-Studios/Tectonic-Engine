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
#ifndef WATER_MANAGER_H
#define WATER_MANAGER_H

//----------------------------------------//
// Brief: Handles "Waters.def" file
//----------------------------------------//

#include "texturemanager.h"


#define MAX_WATER_DEFS 64

    typedef struct {
        Char name[64];
        Char normalPath[128];
        Char dudvPath[128];
        Char flowmapPath[128];
        GLuint normalMap;
        GLuint dudvMap;
        GLuint flowMap;
        Float flowSpeed;
    } WaterDef;

    void WaterManager_Init(void);
    void WaterManager_Shutdown(void);
    void WaterManager_ParseWaters(const Char* filepath);
    WaterDef* WaterManager_FindWaterDef(const Char* name);
    Int WaterManager_GetWaterDefCount(void);
    WaterDef* WaterManager_GetWaterDef(Int index);


#endif // WATER_MANAGER_H