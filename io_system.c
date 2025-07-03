/*
 * Copyright © 2025 Soft Sprint Studios
 * All rights reserved.
 *
 * This file is proprietary and confidential. Unauthorized reproduction,
 * modification, or distribution is strictly prohibited unless explicit
 * written permission is granted by Soft Sprint Studios.
 */
#include "io_system.h"
#include "sound_system.h"
#include "video_player.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

IOConnection g_io_connections[MAX_IO_CONNECTIONS];
int g_num_io_connections = 0;
static PendingEvent g_pending_events[MAX_PENDING_EVENTS];
static int g_num_pending_events = 0;

void IO_Init() {
    IO_Clear();
    memset(g_pending_events, 0, sizeof(g_pending_events));
    g_num_pending_events = 0;
    printf("IO System Initialized.\n");
}

void IO_Shutdown() {
    printf("IO System Shutdown.\n");
}

void IO_Clear() {
    memset(g_io_connections, 0, sizeof(g_io_connections));
    g_num_io_connections = 0;
}

IOConnection* IO_AddConnection(EntityType sourceType, int sourceIndex, const char* output) {
    if (g_num_io_connections >= MAX_IO_CONNECTIONS) {
        printf("ERROR: Max IO connections reached!\n");
        return NULL;
    }
    IOConnection* conn = &g_io_connections[g_num_io_connections];
    conn->active = true;
    conn->sourceType = sourceType;
    conn->sourceIndex = sourceIndex;
    strncpy(conn->outputName, output, 63);
    conn->targetName[0] = '\0';
    conn->inputName[0] = '\0';
    conn->delay = 0.0f;
    conn->fireOnce = false;
    conn->hasFired = false;
    g_num_io_connections++;
    return conn;
}

void IO_RemoveConnection(int connection_index) {
    if (connection_index < 0 || connection_index >= g_num_io_connections) return;
    for (int i = connection_index; i < g_num_io_connections - 1; ++i) {
        g_io_connections[i] = g_io_connections[i + 1];
    }
    g_num_io_connections--;
}

int IO_GetConnectionsForEntity(EntityType type, int index, IOConnection** connections_out, int max_out) {
    int count = 0;
    for (int i = 0; i < g_num_io_connections; i++) {
        if (g_io_connections[i].active && g_io_connections[i].sourceType == type && g_io_connections[i].sourceIndex == index) {
            if (count < max_out) {
                connections_out[count] = &g_io_connections[i];
            }
            count++;
        }
    }
    return count;
}


void IO_FireOutput(EntityType sourceType, int sourceIndex, const char* outputName, float currentTime) {
    for (int i = 0; i < g_num_io_connections; ++i) {
        IOConnection* conn = &g_io_connections[i];
        if (conn->active && conn->sourceType == sourceType && conn->sourceIndex == sourceIndex && strcmp(conn->outputName, outputName) == 0) {
            if (conn->fireOnce && conn->hasFired) {
                continue;
            }

            if (g_num_pending_events >= MAX_PENDING_EVENTS) {
                printf("ERROR: Max pending events reached!\n");
                return;
            }

            PendingEvent* event = &g_pending_events[g_num_pending_events++];
            event->active = true;
            strncpy(event->targetName, conn->targetName, 63);
            strncpy(event->inputName, conn->inputName, 63);
            event->executionTime = currentTime + conn->delay;

            conn->hasFired = true;
        }
    }
}

static void ExecuteInput(const char* targetName, const char* inputName, Scene* scene, Engine* engine) {
    for (int i = 0; i < scene->numObjects; ++i) {
        if (strcmp(scene->objects[i].targetname, targetName) == 0) {
            if (strcmp(inputName, "EnablePhysics") == 0) {
                scene->objects[i].isPhysicsEnabled = true;
                Physics_ToggleCollision(engine->physicsWorld, scene->objects[i].physicsBody, true);
            }
            else if (strcmp(inputName, "DisablePhysics") == 0) {
                scene->objects[i].isPhysicsEnabled = false;
                Physics_ToggleCollision(engine->physicsWorld, scene->objects[i].physicsBody, false);
            }
        }
    }
    for (int i = 0; i < scene->numActiveLights; ++i) {
        if (strcmp(scene->lights[i].targetname, targetName) == 0) {
            if (strcmp(inputName, "TurnOn") == 0) scene->lights[i].is_on = true;
            else if (strcmp(inputName, "TurnOff") == 0) scene->lights[i].is_on = false;
            else if (strcmp(inputName, "Toggle") == 0) scene->lights[i].is_on = !scene->lights[i].is_on;
        }
    }
    for (int i = 0; i < scene->numSoundEntities; ++i) {
        if (strcmp(scene->soundEntities[i].targetname, targetName) == 0) {
            if (strcmp(inputName, "PlaySound") == 0) {
                if (scene->soundEntities[i].sourceID != 0) {
                    SoundSystem_DeleteSource(scene->soundEntities[i].sourceID);
                }
                scene->soundEntities[i].sourceID = SoundSystem_PlaySound(scene->soundEntities[i].bufferID, scene->soundEntities[i].pos, scene->soundEntities[i].volume, scene->soundEntities[i].pitch, scene->soundEntities[i].maxDistance, scene->soundEntities[i].is_looping);
            }
            else if (strcmp(inputName, "StopSound") == 0) {
                if (scene->soundEntities[i].sourceID != 0) {
                    SoundSystem_DeleteSource(scene->soundEntities[i].sourceID);
                    scene->soundEntities[i].sourceID = 0;
                }
            }
            else if (strcmp(inputName, "EnableLoop") == 0) {
                scene->soundEntities[i].is_looping = true;
                if (scene->soundEntities[i].sourceID != 0) {
                    SoundSystem_SetSourceLooping(scene->soundEntities[i].sourceID, true);
                }
            }
            else if (strcmp(inputName, "DisableLoop") == 0) {
                scene->soundEntities[i].is_looping = false;
                if (scene->soundEntities[i].sourceID != 0) {
                    SoundSystem_SetSourceLooping(scene->soundEntities[i].sourceID, false);
                }
            }
            else if (strcmp(inputName, "ToggleLoop") == 0) {
                scene->soundEntities[i].is_looping = !scene->soundEntities[i].is_looping;
                if (scene->soundEntities[i].sourceID != 0) {
                    SoundSystem_SetSourceLooping(scene->soundEntities[i].sourceID, scene->soundEntities[i].is_looping);
                }
            }
        }
    }
    for (int i = 0; i < scene->numParticleEmitters; ++i) {
        if (strcmp(scene->particleEmitters[i].targetname, targetName) == 0) {
            if (strcmp(inputName, "TurnOn") == 0) scene->particleEmitters[i].is_on = true;
            else if (strcmp(inputName, "TurnOff") == 0) scene->particleEmitters[i].is_on = false;
            else if (strcmp(inputName, "Toggle") == 0) scene->particleEmitters[i].is_on = !scene->particleEmitters[i].is_on;
        }
    }
    for (int i = 0; i < scene->numVideoPlayers; ++i) {
        if (strcmp(scene->videoPlayers[i].targetname, targetName) == 0) {
            if (strcmp(inputName, "startvideo") == 0) {
                VideoPlayer_Play(&scene->videoPlayers[i]);
            }
            else if (strcmp(inputName, "stopvideo") == 0) {
                VideoPlayer_Stop(&scene->videoPlayers[i]);
            }
            else if (strcmp(inputName, "restartvideo") == 0) {
                VideoPlayer_Restart(&scene->videoPlayers[i]);
            }
        }
    }
}

void IO_ProcessPendingEvents(float currentTime, Scene* scene, Engine* engine) {
    for (int i = 0; i < g_num_pending_events; ++i) {
        PendingEvent* event = &g_pending_events[i];
        if (event->active && currentTime >= event->executionTime) {
            ExecuteInput(event->targetName, event->inputName, scene, engine);
            event->active = false;
        }
    }

    int write_idx = 0;
    for (int read_idx = 0; read_idx < g_num_pending_events; ++read_idx) {
        if (g_pending_events[read_idx].active) {
            if (write_idx != read_idx) {
                g_pending_events[write_idx] = g_pending_events[read_idx];
            }
            write_idx++;
        }
    }
    g_num_pending_events = write_idx;
}