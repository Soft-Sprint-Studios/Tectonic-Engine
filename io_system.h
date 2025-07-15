/*
 * Copyright © 2025 Soft Sprint Studios
 * All rights reserved.
 *
 * This file is proprietary and confidential. Unauthorized reproduction,
 * modification, or distribution is strictly prohibited unless explicit
 * written permission is granted by Soft Sprint Studios.
 */
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

    void IO_FireOutput(EntityType sourceType, int sourceIndex, const char* outputName, float currentTime, const char* parameter);
    void ExecuteInput(const char* targetName, const char* inputName, const char* parameter, Scene* scene, Engine* engine);

    const char* LogicEntity_GetProperty(LogicEntity* ent, const char* key, const char* default_val);

    extern IOConnection g_io_connections[MAX_IO_CONNECTIONS];
    extern int g_num_io_connections;

#ifdef __cplusplus
}
#endif

#endif // IO_SYSTEM_H