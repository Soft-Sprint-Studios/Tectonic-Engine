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
#ifndef IO_SYSTEM_H
#define IO_SYSTEM_H

//----------------------------------------//
// Brief: The input/output handling
//----------------------------------------//

#include "map.h"


constexpr int MAX_IO_CONNECTIONS = 1024;
constexpr int MAX_PENDING_EVENTS = 256;

    typedef struct {
        Bool active;
        EntityType sourceType;
        Int sourceIndex;
        Char outputName[64];
        Char targetName[64];
        Char inputName[64];
        Char parameter[64];
        Float delay;
        Bool fireOnce;
        Bool hasFired;
    } IOConnection;

    typedef struct {
        Bool active;
        Char targetName[64];
        Char inputName[64];
        Char parameter[64];
        Float executionTime;
    } PendingEvent;

    void IO_Init();
    void IO_Shutdown();
    void IO_Clear();

    IOConnection* IO_AddConnection(EntityType sourceType, Int sourceIndex, const Char* output);
    void IO_RemoveConnection(Int connection_index);
    Int IO_GetConnectionsForEntity(EntityType type, Int index, IOConnection** connections_out, Int max_out);

    Bool IO_FindNamedEntity(Scene* scene, const Char* name, Vec3* out_pos, Vec3* out_angles);
    void IO_FireOutput(EntityType sourceType, Int sourceIndex, const Char* outputName, Float currentTime, const Char* parameter);
    void IO_ProcessPendingEvents(Float currentTime, Scene* scene, Engine* engine);
    LogicEntity* FindActiveEntityByClass(Scene* scene, const Char* classname);
    void ExecuteInput(const Char* targetName, const Char* inputName, const Char* parameter, Scene* scene, Engine* engine);

    const Char* Brush_GetProperty(Brush* b, const Char* key, const Char* default_val);
    const Char* LogicEntity_GetProperty(LogicEntity* ent, const Char* key, const Char* default_val);

    Char** IO_ScanDirectory(const Char* dir_path, const Char** extensions, Int num_extensions, Int* out_count);
    void IO_FreeFileList(Char** list, Int count);

    extern IOConnection g_io_connections[MAX_IO_CONNECTIONS];
    extern Int g_num_io_connections;


#endif // IO_SYSTEM_H