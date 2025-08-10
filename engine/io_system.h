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
#pragma once
#ifndef IO_SYSTEM_H
#define IO_SYSTEM_H

//----------------------------------------//
// Brief: The input/output handling
//----------------------------------------//

#include <stdbool.h>
#include "map.h"

#ifdef __cplusplus
extern "C" {
#endif

#define MAX_IO_CONNECTIONS 1024
#define MAX_PENDING_EVENTS 256

    typedef struct {
        bool active;
        EntityType sourceType;
        int sourceIndex;
        char outputName[64];
        char targetName[64];
        char inputName[64];
        char parameter[64];
        float delay;
        bool fireOnce;
        bool hasFired;
    } IOConnection;

    typedef struct {
        bool active;
        char targetName[64];
        char inputName[64];
        char parameter[64];
        float executionTime;
    } PendingEvent;

    void IO_Init();
    void IO_Shutdown();
    void IO_Clear();

    IOConnection* IO_AddConnection(EntityType sourceType, int sourceIndex, const char* output);
    void IO_RemoveConnection(int connection_index);
    int IO_GetConnectionsForEntity(EntityType type, int index, IOConnection** connections_out, int max_out);

    bool IO_FindNamedEntity(Scene* scene, const char* name, Vec3* out_pos, Vec3* out_angles);
    void IO_FireOutput(EntityType sourceType, int sourceIndex, const char* outputName, float currentTime, const char* parameter);
    LogicEntity* FindActiveEntityByClass(Scene* scene, const char* classname);
    void ExecuteInput(const char* targetName, const char* inputName, const char* parameter, Scene* scene, Engine* engine);

    const char* Brush_GetProperty(Brush* b, const char* key, const char* default_val);
    const char* LogicEntity_GetProperty(LogicEntity* ent, const char* key, const char* default_val);

    extern IOConnection g_io_connections[MAX_IO_CONNECTIONS];
    extern int g_num_io_connections;

#ifdef __cplusplus
}
#endif

#endif // IO_SYSTEM_H