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
#include "water_manager.h"
#include "gl_console.h"
#include <stdio.h>
#include <string.h>
#include <ctype.h>

static WaterDef g_water_defs[MAX_WATER_DEFS];
static int g_num_water_defs = 0;
static WaterDef g_default_water_def;

void WaterManager_Init(void) {
    memset(g_water_defs, 0, sizeof(g_water_defs));
    g_num_water_defs = 0;
    
    strcpy(g_default_water_def.name, "default_water");
    strcpy(g_default_water_def.normalPath, "water_normal.png");
    strcpy(g_default_water_def.dudvPath, "dudv.png");
    strcpy(g_default_water_def.flowmapPath, "");
    g_default_water_def.normalMap = loadTexture("water_normal.png", false, TEXTURE_LOAD_CONTEXT_WORLD);
    g_default_water_def.dudvMap = loadTexture("dudv.png", false, TEXTURE_LOAD_CONTEXT_WORLD);
    g_default_water_def.flowMap = 0;
    g_default_water_def.flowSpeed = 0.01f;

    Console_Printf("Water Manager Initialized.\n");
}

void WaterManager_Shutdown(void) {
    for (int i = 0; i < g_num_water_defs; ++i) {
        if (g_water_defs[i].normalMap) glDeleteTextures(1, &g_water_defs[i].normalMap);
        if (g_water_defs[i].dudvMap) glDeleteTextures(1, &g_water_defs[i].dudvMap);
        if (g_water_defs[i].flowMap) glDeleteTextures(1, &g_water_defs[i].flowMap);
    }
    if(g_default_water_def.normalMap) glDeleteTextures(1, &g_default_water_def.normalMap);
    if(g_default_water_def.dudvMap) glDeleteTextures(1, &g_default_water_def.dudvMap);
    if (g_default_water_def.flowMap) glDeleteTextures(1, &g_default_water_def.flowMap);
    Console_Printf("Water Manager Shutdown.\n");
}

void WaterManager_ParseWaters(const char* filepath) {
    FILE* file = fopen(filepath, "r");
    if (!file) {
        Console_Printf("WaterManager WARNING: Could not open water file '%s'. Using default only.\n", filepath);
        return;
    }

    char line[256];
    WaterDef* current_def = NULL;

    while (fgets(line, sizeof(line), file)) {
        char* trimmed_line = trim(line);
        if (strlen(trimmed_line) == 0 || trimmed_line[0] == '/' || trimmed_line[0] == '#') continue;

        if (trimmed_line[0] == '"') {
            if (g_num_water_defs >= MAX_WATER_DEFS) break;
            current_def = &g_water_defs[g_num_water_defs];
            memset(current_def, 0, sizeof(WaterDef));
            sscanf(trimmed_line, "\"%[^\"]\"", current_def->name);
        } else if (trimmed_line[0] == '{') {
        } else if (trimmed_line[0] == '}') {
            if (current_def) {
                current_def->normalMap = loadTexture(current_def->normalPath, false, TEXTURE_LOAD_CONTEXT_WORLD);
                current_def->dudvMap = loadTexture(current_def->dudvPath, false, TEXTURE_LOAD_CONTEXT_WORLD);
                if (strlen(current_def->flowmapPath) > 0) {
                    current_def->flowMap = loadTexture(current_def->flowmapPath, false, TEXTURE_LOAD_CONTEXT_WORLD);
                }
                else {
                    current_def->flowMap = 0;
                }
                g_num_water_defs++;
                current_def = NULL;
            }
        } else if (current_def) {
            char key[64], value[128];
            if (sscanf(trimmed_line, "%s = \"%127[^\"]\"", key, value) == 2) {
                if (strcmp(key, "normal") == 0) strcpy(current_def->normalPath, value);
                else if (strcmp(key, "dudv") == 0) strcpy(current_def->dudvPath, value);
                else if (strcmp(key, "flowmap") == 0) strcpy(current_def->flowmapPath, value);
            }
            else {
                float float_val;
                if (sscanf(trimmed_line, "%s = %f", key, &float_val) == 2) {
                    if (strcmp(key, "flowspeed") == 0) {
                        current_def->flowSpeed = float_val;
                    }
                }
            }
        }
    }
    fclose(file);
}

WaterDef* WaterManager_FindWaterDef(const char* name) {
    for (int i = 0; i < g_num_water_defs; ++i) {
        if (strcmp(g_water_defs[i].name, name) == 0) {
            return &g_water_defs[i];
        }
    }
    return &g_default_water_def;
}

int WaterManager_GetWaterDefCount(void) {
    return g_num_water_defs;
}

WaterDef* WaterManager_GetWaterDef(int index) {
    if (index < 0 || index >= g_num_water_defs) return &g_default_water_def;
    return &g_water_defs[index];
}