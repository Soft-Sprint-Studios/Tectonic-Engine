/*
 * Copyright © 2025 Soft Sprint Studios
 * All rights reserved.
 *
 * This file is proprietary and confidential. Unauthorized reproduction,
 * modification, or distribution is strictly prohibited unless explicit
 * written permission is granted by Soft Sprint Studios.
 */
#include "editor_undo.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#define MAX_UNDO_ACTIONS 128

typedef enum {
    ACTION_NONE,
    ACTION_MODIFY_ENTITY,
    ACTION_CREATE_ENTITY,
    ACTION_DELETE_ENTITY
} ActionType;

typedef struct {
    EntityType type;
    int index;
    union {
        SceneObject object;
        Brush brush;
        Light light;
        Decal decal;
        SoundEntity soundEntity;
        ParticleEmitter particleEmitter;
        VideoPlayer videoPlayer;
        PlayerStart playerStart;
        Fog fog;
        PostProcessSettings post;
    } data;
    char modelPath[128];
    char parFile[128];
    char soundPath[128];
} EntityState;

typedef struct {
    ActionType type;
    char description[64];
    EntityState before;
    EntityState after;
} Action;

static Action* g_undo_stack[MAX_UNDO_ACTIONS];
static Action* g_redo_stack[MAX_UNDO_ACTIONS];
static int g_undo_top = -1;
static int g_redo_top = -1;

static EntityState g_before_modification_state;
static bool g_is_modifying = false;

extern void SceneObject_UpdateMatrix(SceneObject* obj);
extern void Brush_UpdateMatrix(Brush* b);
extern void Decal_UpdateMatrix(Decal* d);
extern void Light_InitShadowMap(Light* light);
extern void Light_DestroyShadowMap(Light* light);
extern void Physics_SetWorldTransform(RigidBodyHandle body, Mat4 transform);
extern void Brush_CreateRenderData(Brush* b);
extern void Brush_DeepCopy(Brush* dest, const Brush* src);
extern void Brush_FreeData(Brush* b);
extern void ParticleEmitter_Init(ParticleEmitter* emitter, ParticleSystem* system, Vec3 position);
extern void ParticleEmitter_Free(ParticleEmitter* emitter);

static void _raw_delete_model(Scene* scene, int index, Engine* engine) {
    if (index < 0 || index >= scene->numObjects) return;
    if (scene->objects[index].model) Model_Free(scene->objects[index].model);
    if (scene->objects[index].physicsBody) Physics_RemoveRigidBody(engine->physicsWorld, scene->objects[index].physicsBody);
    for (int i = index; i < scene->numObjects - 1; ++i) scene->objects[i] = scene->objects[i + 1];
    scene->numObjects--;
    if (scene->numObjects > 0) { scene->objects = realloc(scene->objects, scene->numObjects * sizeof(SceneObject)); }
    else { free(scene->objects); scene->objects = NULL; }
}

static void _raw_delete_brush(Scene* scene, Engine* engine, int index) {
    if (index < 0 || index >= scene->numBrushes) return;
    Brush_FreeData(&scene->brushes[index]);
    if (scene->brushes[index].physicsBody) { Physics_RemoveRigidBody(engine->physicsWorld, scene->brushes[index].physicsBody); scene->brushes[index].physicsBody = NULL; }
    for (int i = index; i < scene->numBrushes - 1; ++i) scene->brushes[i] = scene->brushes[i + 1];
    scene->numBrushes--;
}

static void _raw_delete_light(Scene* scene, int index) {
    if (index < 0 || index >= scene->numActiveLights) return;
    Light_DestroyShadowMap(&scene->lights[index]);
    for (int i = index; i < scene->numActiveLights - 1; ++i) scene->lights[i] = scene->lights[i + 1];
    scene->numActiveLights--;
}

static void _raw_delete_decal(Scene* scene, int index) {
    if (index < 0 || index >= scene->numDecals) return;
    for (int i = index; i < scene->numDecals - 1; ++i) scene->decals[i] = scene->decals[i + 1];
    scene->numDecals--;
}

static void _raw_delete_sound_entity(Scene* scene, int index) {
    if (index < 0 || index >= scene->numSoundEntities) return;
    SoundSystem_DeleteSource(scene->soundEntities[index].sourceID);
    for (int i = index; i < scene->numSoundEntities - 1; ++i) scene->soundEntities[i] = scene->soundEntities[i + 1];
    scene->numSoundEntities--;
}

static void _raw_delete_particle_emitter(Scene* scene, int index) {
    if (index < 0 || index >= scene->numParticleEmitters) return;
    ParticleEmitter_Free(&scene->particleEmitters[index]);
    ParticleSystem_Free(scene->particleEmitters[index].system);
    for (int i = index; i < scene->numParticleEmitters - 1; ++i) scene->particleEmitters[i] = scene->particleEmitters[i + 1];
    scene->numParticleEmitters--;
}

static void free_entity_state_data(EntityState* state) {
    if (state->type == ENTITY_BRUSH) {
        Brush_FreeData(&state->data.brush);
    }
}

static void capture_state(EntityState* state, Scene* scene, EntityType type, int index) {
    state->type = type;
    state->index = index;
    memset(state->modelPath, 0, sizeof(state->modelPath));
    memset(state->parFile, 0, sizeof(state->parFile));
    memset(state->soundPath, 0, sizeof(state->soundPath));
    switch (type) {
    case ENTITY_MODEL:
        state->data.object = scene->objects[index];
        strcpy(state->modelPath, scene->objects[index].modelPath);
        break;
    case ENTITY_BRUSH: Brush_DeepCopy(&state->data.brush, &scene->brushes[index]); break;
    case ENTITY_LIGHT: state->data.light = scene->lights[index]; break;
    case ENTITY_DECAL: state->data.decal = scene->decals[index]; break;
    case ENTITY_SOUND:
        state->data.soundEntity = scene->soundEntities[index];
        strcpy(state->soundPath, scene->soundEntities[index].soundPath);
        break;
    case ENTITY_PARTICLE_EMITTER:
        memcpy(&state->data.particleEmitter, &scene->particleEmitters[index], offsetof(ParticleEmitter, particles));
        strcpy(state->parFile, scene->particleEmitters[index].parFile);
        break;
    case ENTITY_VIDEO_PLAYER:
        state->data.videoPlayer = scene->videoPlayers[index];
        break;
    case ENTITY_PLAYERSTART: state->data.playerStart = scene->playerStart; break;
    }
}

static void apply_state(Scene* scene, Engine* engine, EntityState* state, bool is_creation, bool is_deletion) {
    if (is_deletion) {
        if (state->type == ENTITY_MODEL) _raw_delete_model(scene, state->index, engine);
        else if (state->type == ENTITY_BRUSH) _raw_delete_brush(scene, engine, state->index);
        else if (state->type == ENTITY_LIGHT) _raw_delete_light(scene, state->index);
        else if (state->type == ENTITY_DECAL) _raw_delete_decal(scene, state->index);
        else if (state->type == ENTITY_SOUND) _raw_delete_sound_entity(scene, state->index);
        else if (state->type == ENTITY_PARTICLE_EMITTER) _raw_delete_particle_emitter(scene, state->index);
        return;
    }
    if (is_creation) {
        switch (state->type) {
        case ENTITY_MODEL:
            scene->numObjects++;
            scene->objects = realloc(scene->objects, scene->numObjects * sizeof(SceneObject));
            memmove(&scene->objects[state->index + 1], &scene->objects[state->index], (scene->numObjects - 1 - state->index) * sizeof(SceneObject));
            scene->objects[state->index] = state->data.object;
            strcpy(scene->objects[state->index].modelPath, state->modelPath);
            scene->objects[state->index].model = Model_Load(state->modelPath);
            break;
        case ENTITY_BRUSH: {
            scene->numBrushes++;
            memmove(&scene->brushes[state->index + 1], &scene->brushes[state->index], (scene->numBrushes - 1 - state->index) * sizeof(Brush));
            Brush* b = &scene->brushes[state->index];
            Brush_DeepCopy(b, &state->data.brush);
            Brush_UpdateMatrix(b);
            Brush_CreateRenderData(b);
            if (!b->isTrigger && b->numVertices > 0) {
                Vec3* world_verts = malloc(b->numVertices * sizeof(Vec3));
                for (int i = 0; i < b->numVertices; ++i) {
                    world_verts[i] = mat4_mul_vec3(&b->modelMatrix, b->vertices[i].pos);
                }
                b->physicsBody = Physics_CreateStaticConvexHull(engine->physicsWorld, (const float*)world_verts, b->numVertices);
                free(world_verts);
            }
            else {
                b->physicsBody = NULL;
            }
            break;
        }
        case ENTITY_LIGHT:
            scene->numActiveLights++;
            memmove(&scene->lights[state->index + 1], &scene->lights[state->index], (scene->numActiveLights - 1 - state->index) * sizeof(Light));
            scene->lights[state->index] = state->data.light;
            Light_InitShadowMap(&scene->lights[state->index]);
            break;
        case ENTITY_DECAL:
            scene->numDecals++;
            memmove(&scene->decals[state->index + 1], &scene->decals[state->index], (scene->numDecals - 1 - state->index) * sizeof(Decal));
            scene->decals[state->index] = state->data.decal;
            break;
        case ENTITY_SOUND:
            scene->numSoundEntities++;
            memmove(&scene->soundEntities[state->index + 1], &scene->soundEntities[state->index], (scene->numSoundEntities - 1 - state->index) * sizeof(SoundEntity));
            scene->soundEntities[state->index] = state->data.soundEntity;
            strcpy(scene->soundEntities[state->index].soundPath, state->soundPath);
            scene->soundEntities[state->index].bufferID = SoundSystem_LoadSound(state->soundPath);
            break;
        case ENTITY_PARTICLE_EMITTER:
            scene->numParticleEmitters++;
            memmove(&scene->particleEmitters[state->index + 1], &scene->particleEmitters[state->index], (scene->numParticleEmitters - 1 - state->index) * sizeof(ParticleEmitter));
            scene->particleEmitters[state->index] = state->data.particleEmitter;
            strcpy(scene->particleEmitters[state->index].parFile, state->parFile);
            ParticleSystem* ps = ParticleSystem_Load(state->parFile);
            if (ps) ParticleEmitter_Init(&scene->particleEmitters[state->index], ps, state->data.particleEmitter.pos);
            break;
        case ENTITY_VIDEO_PLAYER:
            scene->numVideoPlayers++;
            memmove(&scene->videoPlayers[state->index + 1], &scene->videoPlayers[state->index], (scene->numVideoPlayers - 1 - state->index) * sizeof(VideoPlayer));
            scene->videoPlayers[state->index] = state->data.videoPlayer;
            VideoPlayer_Load(&scene->videoPlayers[state->index]);
            break;
        }
        return;
    }
    switch (state->type) {
    case ENTITY_MODEL: {
        SceneObject* obj = &scene->objects[state->index];
        obj->pos = state->data.object.pos; obj->rot = state->data.object.rot; obj->scale = state->data.object.scale;
        SceneObject_UpdateMatrix(obj);
        if (obj->physicsBody) Physics_SetWorldTransform(obj->physicsBody, obj->modelMatrix);
        break;
    }
    case ENTITY_BRUSH: {
        Brush* b = &scene->brushes[state->index];
        RigidBodyHandle old_body = b->physicsBody;

        Brush_DeepCopy(b, &state->data.brush);
        Brush_UpdateMatrix(b);
        Brush_CreateRenderData(b);

        if (old_body) {
            Physics_RemoveRigidBody(engine->physicsWorld, old_body);
        }

        if (!b->isTrigger && !b->isReflectionProbe && b->numVertices > 0) {
            Vec3* world_verts = malloc(b->numVertices * sizeof(Vec3));
            for (int i = 0; i < b->numVertices; ++i) world_verts[i] = mat4_mul_vec3(&b->modelMatrix, b->vertices[i].pos);
            b->physicsBody = Physics_CreateStaticConvexHull(engine->physicsWorld, (const float*)world_verts, b->numVertices);
            free(world_verts);
        }
        else {
            b->physicsBody = NULL;
        }
        break;
    }
    case ENTITY_LIGHT: scene->lights[state->index] = state->data.light; break;
    case ENTITY_DECAL: scene->decals[state->index] = state->data.decal; Decal_UpdateMatrix(&scene->decals[state->index]); break;
    case ENTITY_SOUND: scene->soundEntities[state->index] = state->data.soundEntity; break;
    case ENTITY_PARTICLE_EMITTER: memcpy(&scene->particleEmitters[state->index], &state->data.particleEmitter, offsetof(ParticleEmitter, particles)); break;
    case ENTITY_PLAYERSTART: scene->playerStart = state->data.playerStart; break;
    }
}

static void clear_stack(Action** stack, int* top) {
    for (int i = 0; i <= *top; ++i) {
        free_entity_state_data(&stack[i]->before);
        free_entity_state_data(&stack[i]->after);
        free(stack[i]);
    }
    *top = -1;
}

void Undo_Init() {
    g_undo_top = -1;
    g_redo_top = -1;
    g_is_modifying = false;
}

void Undo_Shutdown() {
    clear_stack(g_undo_stack, &g_undo_top);
    clear_stack(g_redo_stack, &g_redo_top);
    free_entity_state_data(&g_before_modification_state);
}

static void Undo_PushAction(Action* action) {
    clear_stack(g_redo_stack, &g_redo_top);
    if (g_undo_top >= MAX_UNDO_ACTIONS - 1) {
        free_entity_state_data(&g_undo_stack[0]->before);
        free_entity_state_data(&g_undo_stack[0]->after);
        free(g_undo_stack[0]);
        memmove(&g_undo_stack[0], &g_undo_stack[1], (MAX_UNDO_ACTIONS - 1) * sizeof(Action*));
        g_undo_top--;
    }
    g_undo_top++;
    g_undo_stack[g_undo_top] = action;
}

void Undo_PerformUndo(Scene* scene, Engine* engine) {
    if (g_undo_top < 0) return;
    Action* action = g_undo_stack[g_undo_top--];
    if (g_redo_top >= MAX_UNDO_ACTIONS - 1) {
        free_entity_state_data(&g_redo_stack[0]->before);
        free_entity_state_data(&g_redo_stack[0]->after);
        free(g_redo_stack[0]);
        memmove(&g_redo_stack[0], &g_redo_stack[1], (MAX_UNDO_ACTIONS - 1) * sizeof(Action*));
        g_redo_top--;
    }
    g_redo_stack[++g_redo_top] = action;

    switch (action->type) {
    case ACTION_MODIFY_ENTITY: apply_state(scene, engine, &action->before, false, false); break;
    case ACTION_CREATE_ENTITY: apply_state(scene, engine, &action->after, false, true); break;
    case ACTION_DELETE_ENTITY: apply_state(scene, engine, &action->before, true, false); break;
    }
}

void Undo_PerformRedo(Scene* scene, Engine* engine) {
    if (g_redo_top < 0) return;
    Action* action = g_redo_stack[g_redo_top--];
    g_undo_stack[++g_undo_top] = action;

    switch (action->type) {
    case ACTION_MODIFY_ENTITY: apply_state(scene, engine, &action->after, false, false); break;
    case ACTION_CREATE_ENTITY: apply_state(scene, engine, &action->after, true, false); break;
    case ACTION_DELETE_ENTITY: apply_state(scene, engine, &action->before, false, true); break;
    }
}

void Undo_BeginEntityModification(Scene* scene, EntityType type, int index) {
    if (g_is_modifying) return;
    capture_state(&g_before_modification_state, scene, type, index);
    g_is_modifying = true;
}

void Undo_EndEntityModification(Scene* scene, EntityType type, int index, const char* description) {
    if (!g_is_modifying) return;
    g_is_modifying = false;

    EntityState after_state;
    memset(&after_state, 0, sizeof(EntityState));
    capture_state(&after_state, scene, type, index);

    bool changed = false;
    if (type == ENTITY_BRUSH) {
        if (g_before_modification_state.data.brush.numVertices != after_state.data.brush.numVertices ||
            g_before_modification_state.data.brush.numFaces != after_state.data.brush.numFaces) {
            changed = true;
        }
        else {
            if (memcmp(g_before_modification_state.data.brush.vertices, after_state.data.brush.vertices, sizeof(BrushVertex) * after_state.data.brush.numVertices) != 0) {
                changed = true;
            }
            if (!changed) {
                for (int i = 0; i < after_state.data.brush.numFaces; ++i) {
                    if (memcmp(g_before_modification_state.data.brush.faces[i].vertexIndices, after_state.data.brush.faces[i].vertexIndices, sizeof(int) * after_state.data.brush.faces[i].numVertexIndices) != 0) {
                        changed = true;
                        break;
                    }
                }
            }
            if (memcmp(&g_before_modification_state.data.brush, &after_state.data.brush, sizeof(Brush) - offsetof(Brush, vertices)) != 0) {
                changed = true;
            }
        }
    }
    else {
        if (memcmp(&g_before_modification_state.data, &after_state.data, sizeof(after_state.data)) != 0) {
            changed = true;
        }
    }

    if (changed) {
        Action* action = (Action*)calloc(1, sizeof(Action));
        if (!action) { free_entity_state_data(&after_state); return; }

        action->type = ACTION_MODIFY_ENTITY;
        strcpy(action->description, description);

        action->before = g_before_modification_state;
        action->after = after_state;

        memset(&g_before_modification_state, 0, sizeof(EntityState));

        Undo_PushAction(action);
    }
    else {
        free_entity_state_data(&g_before_modification_state);
        free_entity_state_data(&after_state);
    }
}

void Undo_PushCreateEntity(Scene* scene, EntityType type, int index, const char* description) {
    Action* action = (Action*)calloc(1, sizeof(Action));
    if (!action) return;
    action->type = ACTION_CREATE_ENTITY;
    strcpy(action->description, description);
    capture_state(&action->after, scene, type, index);
    action->before.type = ENTITY_NONE;
    Undo_PushAction(action);
}

void Undo_PushDeleteEntity(Scene* scene, EntityType type, int index, const char* description) {
    Action* action = (Action*)calloc(1, sizeof(Action));
    if (!action) return;
    action->type = ACTION_DELETE_ENTITY;
    strcpy(action->description, description);
    capture_state(&action->before, scene, type, index);
    action->after.type = ENTITY_NONE;
    Undo_PushAction(action);
}