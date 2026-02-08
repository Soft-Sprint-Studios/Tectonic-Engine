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
#ifndef GAME_DATA_H
#define GAME_DATA_H

#include "map.h"


constexpr int MAX_TGD_ENTITIES = 256;
constexpr int MAX_TGD_PROPERTIES = 32;
constexpr int MAX_TGD_IOS = 32;

    typedef enum {
        TGD_PROP_STRING,
        TGD_PROP_INTEGER,
        TGD_PROP_FLOAT,
        TGD_PROP_COLOR,
        TGD_PROP_CHECKBOX,
        TGD_PROP_MODEL,
        TGD_PROP_SOUND,
        TGD_PROP_PARTICLE,
        TGD_PROP_CHOICES,
        TGD_PROP_TEXTURE,
        TGD_PROP_ENTITIES,
        TGD_PROP_RADIUS
    } TGD_PropertyType;

    typedef struct {
        Char value[64];
        Char display_name[128];
    } TGD_Choice;

    typedef struct {
        Char key[64];
        Char display_name[128];
        Char default_value[1024];
        TGD_PropertyType type;
        TGD_Choice* choices;
        Int num_choices;
    } TGD_Property;

    typedef struct {
        Char name[64];
        Char description[128];
    } TGD_IO;

    typedef struct {
        Char classname[64];
        EntityType base_type;

        TGD_Property properties[MAX_TGD_PROPERTIES];
        Int num_properties;

        TGD_IO inputs[MAX_TGD_IOS];
        Int num_inputs;

        TGD_IO outputs[MAX_TGD_IOS];
        Int num_outputs;
    } TGD_EntityDef;

    void GameData_Init(const Char* filepath);
    void GameData_Shutdown(void);
    const TGD_EntityDef* GameData_FindEntityDef(const Char* classname);
    const Char** GameData_GetBrushEntityClassnames(Int* count);
    const Char** GameData_GetLogicEntityClassnames(Int* count);


#endif // GAME_DATA_H