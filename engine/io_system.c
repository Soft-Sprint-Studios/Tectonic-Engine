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
#include "io_system.h"
#include "sound_system.h"
#include "video_player.h"
#include "cvar.h"
#include "gl_console.h"
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
    Console_Printf("IO System Initialized.\n");
}

void IO_Shutdown() {
    Console_Printf("IO System Shutdown.\n");
}

void IO_Clear() {
    memset(g_io_connections, 0, sizeof(g_io_connections));
    g_num_io_connections = 0;
}

IOConnection* IO_AddConnection(EntityType sourceType, int sourceIndex, const char* output) {
    if (g_num_io_connections >= MAX_IO_CONNECTIONS) {
        Console_Printf_Error("ERROR: Max IO connections reached!\n");
        return NULL;
    }
    IOConnection* conn = &g_io_connections[g_num_io_connections];
    conn->active = true;
    conn->sourceType = sourceType;
    conn->sourceIndex = sourceIndex;
    strncpy(conn->outputName, output, 63);
    conn->targetName[0] = '\0';
    conn->inputName[0] = '\0';
    conn->parameter[0] = '\0';
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

void IO_FireOutput(EntityType sourceType, int sourceIndex, const char* outputName, float currentTime, const char* parameter) {
    for (int i = 0; i < g_num_io_connections; ++i) {
        IOConnection* conn = &g_io_connections[i];
        if (conn->active && conn->sourceType == sourceType && conn->sourceIndex == sourceIndex && strcmp(conn->outputName, outputName) == 0) {
            if (conn->fireOnce && conn->hasFired) {
                continue;
            }

            if (g_num_pending_events >= MAX_PENDING_EVENTS) {
                Console_Printf_Error("ERROR: Max pending events reached!\n");
                return;
            }

            PendingEvent* event = &g_pending_events[g_num_pending_events++];
            event->active = true;
            strncpy(event->targetName, conn->targetName, 63);
            strncpy(event->inputName, conn->inputName, 63);
            if (parameter) {
                strncpy(event->parameter, parameter, 63);
            }
            else {
                strncpy(event->parameter, conn->parameter, 63);
            }
            event->parameter[63] = '\0';
            event->executionTime = currentTime + conn->delay;

            conn->hasFired = true;
        }
    }
}

void IO_ProcessPendingEvents(float currentTime, Scene* scene, Engine* engine) {
    for (int i = 0; i < g_num_pending_events; ++i) {
        PendingEvent* event = &g_pending_events[i];
        if (event->active && currentTime >= event->executionTime) {
            ExecuteInput(event->targetName, event->inputName, event->parameter, scene, engine);
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

LogicEntity* FindActiveEntityByClass(Scene* scene, const char* classname) {
    for (int i = 0; i < scene->numLogicEntities; ++i) {
        if (strcmp(scene->logicEntities[i].classname, classname) == 0 && scene->logicEntities[i].runtime_active) {
            return &scene->logicEntities[i];
        }
    }
    return NULL;
}

void ExecuteInput(const char* targetName, const char* inputName, const char* parameter, Scene* scene, Engine* engine) {
    for (int i = 0; i < scene->numLogicEntities; ++i) {
        if (strcmp(scene->logicEntities[i].targetname, targetName) == 0) {
            LogicEntity* ent = &scene->logicEntities[i];
            if (strcmp(ent->classname, "logic_timer") == 0) {
                if (strcmp(inputName, "StartTimer") == 0) {
                    ent->runtime_active = true;
                    const char* delay_val = LogicEntity_GetProperty(ent, "delay", "1.0");
                    ent->runtime_float_a = atof(delay_val);
                }
                else if (strcmp(inputName, "StopTimer") == 0) {
                    ent->runtime_active = false;
                }
                else if (strcmp(inputName, "ToggleTimer") == 0) {
                    ent->runtime_active = !ent->runtime_active;
                    if (ent->runtime_active && ent->runtime_float_a <= 0) {
                        const char* delay_val = LogicEntity_GetProperty(ent, "delay", "1.0");
                        ent->runtime_float_a = atof(delay_val);
                    }
                }
            }
            else if (strcmp(ent->classname, "math_counter") == 0) {
                int min = atoi(LogicEntity_GetProperty(ent, "min", "0"));
                int max = atoi(LogicEntity_GetProperty(ent, "max", "0"));
                int value = (parameter && strlen(parameter) > 0) ? atoi(parameter) : 1;

                if (strcmp(inputName, "Add") == 0) ent->runtime_float_a += value;
                else if (strcmp(inputName, "Subtract") == 0) ent->runtime_float_a -= value;
                else if (strcmp(inputName, "Multiply") == 0) ent->runtime_float_a *= value;
                else if (strcmp(inputName, "Divide") == 0) {
                    if (value != 0) ent->runtime_float_a /= value;
                    else Console_Printf_Error("[error] math_counter '%s' tried to divide by zero.", ent->targetname);
                }

                if (max != 0 && ent->runtime_float_a >= max) IO_FireOutput(ENTITY_LOGIC, i, "OnHitMax", 0, NULL);
                if (min != 0 && ent->runtime_float_a <= min) IO_FireOutput(ENTITY_LOGIC, i, "OnHitMin", 0, NULL);
            }
            else if (strcmp(ent->classname, "logic_random") == 0) {
                if (strcmp(inputName, "Enable") == 0) {
                    if (!ent->runtime_active) {
                        const char* min_time_str = LogicEntity_GetProperty(ent, "min_time", "0.0");
                        const char* max_time_str = LogicEntity_GetProperty(ent, "max_time", "0.0");
                        ent->runtime_float_a = rand_float_range(atof(min_time_str), atof(max_time_str));
                    }
                    ent->runtime_active = true;
                }
                else if (strcmp(inputName, "Disable") == 0) ent->runtime_active = false;
            }
            else if (strcmp(ent->classname, "logic_relay") == 0) {
                if (strcmp(inputName, "Trigger") == 0 && ent->runtime_active) IO_FireOutput(ENTITY_LOGIC, i, "OnTrigger", engine->lastFrame, NULL);
                else if (strcmp(inputName, "Enable") == 0) ent->runtime_active = true;
                else if (strcmp(inputName, "Disable") == 0) ent->runtime_active = false;
                else if (strcmp(inputName, "Toggle") == 0) ent->runtime_active = !ent->runtime_active;
            }
            else if (strcmp(ent->classname, "point_servercommand") == 0) {
                if (strcmp(inputName, "Command") == 0) {
                    if (parameter && strlen(parameter) > 0) {
                        char cmd_copy[MAX_COMMAND_LENGTH];
                        strncpy(cmd_copy, parameter, MAX_COMMAND_LENGTH - 1);
                        cmd_copy[MAX_COMMAND_LENGTH - 1] = '\0';

                        char* argv[16];
                        int argc = 0;
                        char* p = strtok(cmd_copy, " ");
                        while (p != NULL && argc < 16) {
                            argv[argc++] = p;
                            p = strtok(NULL, " ");
                        }
                        if (argc > 0) {
                            Commands_Execute(argc, argv);
                        }
                    }
                }
            }
            else if (strcmp(ent->classname, "logic_compare") == 0) {
                if (strcmp(inputName, "SetValue") == 0) {
                    ent->runtime_float_a = atof(parameter);
                }
                else if (strcmp(inputName, "SetCompareValue") == 0) {
                    for (int prop_idx = 0; prop_idx < ent->numProperties; ++prop_idx) {
                        if (strcmp(ent->properties[prop_idx].key, "CompareValue") == 0) {
                            strncpy(ent->properties[prop_idx].value, parameter, sizeof(ent->properties[prop_idx].value) - 1);
                            break;
                        }
                    }
                }
                else if (strcmp(inputName, "Compare") == 0 || strcmp(inputName, "SetValueCompare") == 0) {
                    if (strcmp(inputName, "SetValueCompare") == 0) {
                        ent->runtime_float_a = atof(parameter);
                    }

                    float val_a = ent->runtime_float_a;
                    float val_b = atof(LogicEntity_GetProperty(ent, "CompareValue", "0"));
                    char param_out[32];
                    sprintf(param_out, "%f", val_a);

                    if (val_a < val_b) IO_FireOutput(ENTITY_LOGIC, i, "OnLessThan", engine->lastFrame, param_out);
                    if (val_a == val_b) IO_FireOutput(ENTITY_LOGIC, i, "OnEqualTo", engine->lastFrame, param_out);
                    if (val_a != val_b) IO_FireOutput(ENTITY_LOGIC, i, "OnNotEqualTo", engine->lastFrame, param_out);
                    if (val_a > val_b) IO_FireOutput(ENTITY_LOGIC, i, "OnGreaterThan", engine->lastFrame, param_out);
                }
            }
            else if (strcmp(ent->classname, "env_blackhole") == 0) {
                if (strcmp(inputName, "Enable") == 0) {
                    ent->runtime_active = true;
                }
                else if (strcmp(inputName, "Disable") == 0) {
                    ent->runtime_active = false;
                }
            }
            else if (strcmp(ent->classname, "env_fade") == 0) {
                if (strcmp(inputName, "FadeIn") == 0) {
                    ent->runtime_int_a = 1;
                    ent->runtime_float_a = 0.0f;
                }
                else if (strcmp(inputName, "FadeOut") == 0) {
                    ent->runtime_int_a = 2;
                    ent->runtime_float_a = 0.0f;
                }
                else if (strcmp(inputName, "Fade") == 0) {
                    ent->runtime_int_a = 4;
                    ent->runtime_float_a = 0.0f;
                }
            }
            else if (strcmp(ent->classname, "env_shake") == 0) {
                bool global_shake = atoi(LogicEntity_GetProperty(ent, "GlobalShake", "0"));
                float radius = atof(LogicEntity_GetProperty(ent, "radius", "500.0"));
                float dist_sq = vec3_length_sq(vec3_sub(engine->camera.position, ent->pos));

                if (strcmp(inputName, "StartShake") == 0) {
                    if (global_shake || dist_sq < (radius * radius)) {
                        engine->shake_amplitude = atof(LogicEntity_GetProperty(ent, "amplitude", "4.0"));
                        engine->shake_frequency = atof(LogicEntity_GetProperty(ent, "frequency", "40.0"));
                        engine->shake_duration_timer = atof(LogicEntity_GetProperty(ent, "duration", "1.0"));
                    }
                }
                else if (strcmp(inputName, "StopShake") == 0) {
                    if (global_shake || dist_sq < (radius * radius)) {
                        engine->shake_amplitude = 0.0f;
                        engine->shake_duration_timer = 0.0f;
                    }
                }
            }
            else if (strcmp(ent->classname, "env_fog") == 0) {
                if (strcmp(inputName, "Enable") == 0) ent->runtime_active = true;
                else if (strcmp(inputName, "Disable") == 0) ent->runtime_active = false;
            }
            else if (strcmp(ent->classname, "game_end") == 0) {
                if (strcmp(inputName, "EndGame") == 0) {
                    char* disconnect_argv[] = { (char*)"disconnect" };
                    Commands_Execute(1, disconnect_argv);
                }
            }
        }
    }
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
            SceneObject* obj = &scene->objects[i];
            if (strcmp(inputName, "PlayAnimation") == 0) {
                if (obj->model && obj->model->num_animations > 0) {
                    int anim_index = -1;
                    for (int j = 0; j < obj->model->num_animations; ++j) {
                        if (strcmp(obj->model->animations[j].name, parameter) == 0) {
                            anim_index = j;
                            break;
                        }
                    }
                    if (anim_index != -1) {
                        obj->current_animation = anim_index;
                        obj->animation_time = 0.0f;
                        obj->animation_playing = true;
                        if (strstr(parameter, "noloop") != NULL) {
                            obj->animation_looping = false;
                        }
                        else {
                            obj->animation_looping = true;
                        }
                    }
                    else {
                        Console_Printf_Warning("Animation '%s' not found for model '%s'", parameter, obj->targetname);
                    }
                }
                return;
            }
        }
    }
    for (int i = 0; i < scene->numBrushes; ++i) {
        if (strcmp(scene->brushes[i].targetname, targetName) == 0) {
            Brush* b = &scene->brushes[i];
            if (strlen(b->classname) > 0) {
                if (strcmp(b->classname, "func_button") == 0) {
                    if (strcmp(inputName, "Lock") == 0) {
                        for (int k = 0; k < b->numProperties; ++k) if (strcmp(b->properties[k].key, "locked") == 0) strcpy(b->properties[k].value, "1");
                    }
                    else if (strcmp(inputName, "Unlock") == 0) {
                        for (int k = 0; k < b->numProperties; ++k) if (strcmp(b->properties[k].key, "locked") == 0) strcpy(b->properties[k].value, "0");
                    }
                    else if (strcmp(inputName, "Press") == 0) {
                        IO_FireOutput(ENTITY_BRUSH, i, "OnPressed", engine->lastFrame, NULL);
                    }
                }
                else if (strcmp(b->classname, "func_rotating") == 0) {
                    float speed = atof(Brush_GetProperty(b, "speed", "10"));
                    if (strcmp(inputName, "Start") == 0) {
                        b->target_angular_velocity = speed;
                    }
                    else if (strcmp(inputName, "Stop") == 0) {
                        b->target_angular_velocity = 0.0f;
                    }
                    else if (strcmp(inputName, "Toggle") == 0) {
                        b->target_angular_velocity = (b->target_angular_velocity > 0.001f) ? 0.0f : speed;
                    }
                }
                else if (strcmp(b->classname, "func_plat") == 0) {
                    if (strcmp(inputName, "Raise") == 0) {
                        if (b->plat_state == PLAT_STATE_BOTTOM) b->plat_state = PLAT_STATE_UP;
                    }
                    else if (strcmp(inputName, "Lower") == 0) {
                        if (b->plat_state == PLAT_STATE_TOP) b->plat_state = PLAT_STATE_DOWN;
                    }
                    else if (strcmp(inputName, "Toggle") == 0) {
                        if (b->plat_state == PLAT_STATE_TOP) b->plat_state = PLAT_STATE_DOWN;
                        else if (b->plat_state == PLAT_STATE_BOTTOM) b->plat_state = PLAT_STATE_UP;
                    }
                }
                if (strcmp(inputName, "Enable") == 0) {
                    b->runtime_active = true;
                }
                else if (strcmp(inputName, "Disable") == 0) {
                    b->runtime_active = false;
                }
                else if (strcmp(inputName, "Toggle") == 0) {
                    b->runtime_active = !b->runtime_active;
                }
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
    for (int i = 0; i < scene->numSprites; ++i) {
        if (strcmp(scene->sprites[i].targetname, targetName) == 0) {
            if (strcmp(inputName, "TurnOn") == 0) {
                scene->sprites[i].visible = true;
            }
            else if (strcmp(inputName, "TurnOff") == 0) {
                scene->sprites[i].visible = false;
            }
            else if (strcmp(inputName, "Toggle") == 0) {
                scene->sprites[i].visible = !scene->sprites[i].visible;
            }
            return;
        }
    }
}

const char* Brush_GetProperty(Brush* b, const char* key, const char* default_val) {
    for (int i = 0; i < b->numProperties; ++i) {
        if (strcmp(b->properties[i].key, key) == 0) {
            return b->properties[i].value;
        }
    }
    return default_val;
}

const char* LogicEntity_GetProperty(LogicEntity* ent, const char* key, const char* default_val) {
    for (int i = 0; i < ent->numProperties; ++i) {
        if (strcmp(ent->properties[i].key, key) == 0) {
            return ent->properties[i].value;
        }
    }
    return default_val;
}

void LogicSystem_Update(Scene* scene, float deltaTime) {
    for (int i = 0; i < scene->numLogicEntities; ++i) {
        LogicEntity* ent = &scene->logicEntities[i];
        if (strcmp(ent->classname, "logic_timer") == 0) {
            if (ent->runtime_active) {
                ent->runtime_float_a -= deltaTime;
                if (ent->runtime_float_a <= 0) {
                    IO_FireOutput(ENTITY_LOGIC, i, "OnTimer", 0, NULL);

                    const char* repeat_val = LogicEntity_GetProperty(ent, "repeat", "1");
                    int repeat = atoi(repeat_val);

                    if (repeat == -1) {
                        const char* delay_val = LogicEntity_GetProperty(ent, "delay", "1.0");
                        ent->runtime_float_a = atof(delay_val);
                    }
                    else {
                        ent->runtime_active = false;
                    }
                }
            }
        }
        else if (strcmp(ent->classname, "logic_random") == 0) {
            if (ent->runtime_active) {
                ent->runtime_float_a -= deltaTime;
                if (ent->runtime_float_a <= 0) {
                    IO_FireOutput(ENTITY_LOGIC, i, "OnRandom", 0, NULL);
                    const char* min_time_str = LogicEntity_GetProperty(ent, "min_time", "0.0");
                    const char* max_time_str = LogicEntity_GetProperty(ent, "max_time", "0.0");
                    ent->runtime_float_a = rand_float_range(atof(min_time_str), atof(max_time_str));
                }
            }
        }
        else if (strcmp(ent->classname, "env_blackhole") == 0) {
            if (ent->runtime_active) {
                const char* rot_speed_str = LogicEntity_GetProperty(ent, "rotationspeed", "10.0");
                ent->rot.y += atof(rot_speed_str) * deltaTime;
                if (ent->rot.y > 360.0f) ent->rot.y -= 360.0f;
            }
        }
        else if (strcmp(ent->classname, "env_fade") == 0) {
            if (ent->runtime_int_a != 0) {
                scene->post.fade_active = true;
                scene->post.fade_color = (Vec3){ 0, 0, 0 };

                float duration = atof(LogicEntity_GetProperty(ent, "duration", "2.0"));
                if (duration <= 0.0f) duration = 0.01f;
                float holdtime = atof(LogicEntity_GetProperty(ent, "holdtime", "1.0"));
                int renderamt = atoi(LogicEntity_GetProperty(ent, "renderamt", "255"));
                float target_alpha = (float)renderamt / 255.0f;

                ent->runtime_float_a += deltaTime;

                if (ent->runtime_int_a == 1) {
                    scene->post.fade_alpha = fminf(target_alpha, (ent->runtime_float_a / duration) * target_alpha);
                    if (ent->runtime_float_a >= duration) {
                        ent->runtime_int_a = 3;
                        ent->runtime_float_a = 0.0f;
                    }
                }
                else if (ent->runtime_int_a == 2) {
                    scene->post.fade_alpha = fmaxf(0.0f, target_alpha - (ent->runtime_float_a / duration) * target_alpha);
                    if (ent->runtime_float_a >= duration) {
                        ent->runtime_int_a = 0;
                        scene->post.fade_active = false;
                    }
                }
                else if (ent->runtime_int_a == 3) {
                    scene->post.fade_alpha = target_alpha;
                    if (holdtime > 0 && ent->runtime_float_a >= holdtime) {
                    }
                }
                else if (ent->runtime_int_a == 4) {
                    scene->post.fade_alpha = fminf(target_alpha, (ent->runtime_float_a / duration) * target_alpha);
                    if (ent->runtime_float_a >= duration) {
                        ent->runtime_int_a = 5;
                        ent->runtime_float_a = 0.0f;
                    }
                }
                else if (ent->runtime_int_a == 5) {
                    scene->post.fade_alpha = target_alpha;
                    if (ent->runtime_float_a >= holdtime) {
                        ent->runtime_int_a = 2;
                        ent->runtime_float_a = 0.0f;
                    }
                }
            }
        }
    }
}