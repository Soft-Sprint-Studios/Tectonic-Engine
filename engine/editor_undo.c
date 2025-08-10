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
    ActionType type;
    char description[64];
    EntityState* before_states;
    int num_before_states;
    EntityState* after_states;
    int num_after_states;
} Action;

static Action* g_undo_stack[MAX_UNDO_ACTIONS];
static Action* g_redo_stack[MAX_UNDO_ACTIONS];
static int g_undo_top = -1;
static int g_redo_top = -1;

static EntityState* g_multi_before_states = NULL;
static int g_num_multi_before_states = 0;
static bool g_is_modifying = false;

extern void SceneObject_UpdateMatrix(SceneObject* obj);
extern void Brush_UpdateMatrix(Brush* b);
extern void Decal_UpdateMatrix(Decal* d);
extern void ParallaxRoom_UpdateMatrix(ParallaxRoom* p);
extern void Light_InitShadowMap(Light* light);
extern void Light_DestroyShadowMap(Light* light);
extern void Physics_SetWorldTransform(RigidBodyHandle body, Mat4 transform);
extern void Brush_CreateRenderData(Brush* b);
extern void Brush_DeepCopy(Brush* dest, const Brush* src);
extern void Brush_FreeData(Brush* b);
extern void ParticleEmitter_Init(ParticleEmitter* emitter, ParticleSystem* system, Vec3 position);
extern void ParticleEmitter_Free(ParticleEmitter* emitter);
extern void VideoPlayer_Load(VideoPlayer* vp);
extern void VideoPlayer_Free(VideoPlayer* vp);

void _raw_delete_model(Scene* scene, int index, Engine* engine) {
    if (index < 0 || index >= scene->numObjects) return;

    SceneObject* obj_to_delete = &scene->objects[index];
    if (obj_to_delete->model) Model_Free(obj_to_delete->model);
    if (obj_to_delete->bone_matrices) free(obj_to_delete->bone_matrices);
    if (obj_to_delete->physicsBody) Physics_RemoveRigidBody(engine->physicsWorld, obj_to_delete->physicsBody);
    if (obj_to_delete->bakedVertexColors) free(obj_to_delete->bakedVertexColors);
    if (obj_to_delete->bakedVertexDirections) free(obj_to_delete->bakedVertexDirections);

    if (index < scene->numObjects - 1) {
        scene->objects[index] = scene->objects[scene->numObjects - 1];
    }

    scene->numObjects--;

    if (scene->numObjects > 0) {
        scene->objects = (SceneObject*)realloc(scene->objects, scene->numObjects * sizeof(SceneObject));
    }
    else {
        free(scene->objects);
        scene->objects = NULL;
    }
}

void _raw_delete_brush(Scene* scene, Engine* engine, int index) {
    if (index < 0 || index >= scene->numBrushes) return;

    Brush_FreeData(&scene->brushes[index]);
    if (scene->brushes[index].physicsBody) {
        Physics_RemoveRigidBody(engine->physicsWorld, scene->brushes[index].physicsBody);
    }

    if (index < scene->numBrushes - 1) {
        scene->brushes[index] = scene->brushes[scene->numBrushes - 1];
    }

    scene->numBrushes--;
}

void _raw_delete_light(Scene* scene, int index) {
    if (index < 0 || index >= scene->numActiveLights) return;
    Light_DestroyShadowMap(&scene->lights[index]);
    for (int i = index; i < scene->numActiveLights - 1; ++i) scene->lights[i] = scene->lights[i + 1];
    scene->numActiveLights--;
}

void _raw_delete_decal(Scene* scene, int index) {
    if (index < 0 || index >= scene->numDecals) return;
    for (int i = index; i < scene->numDecals - 1; ++i) scene->decals[i] = scene->decals[i + 1];
    scene->numDecals--;
}

void _raw_delete_sound_entity(Scene* scene, int index) {
    if (index < 0 || index >= scene->numSoundEntities) return;
    if (scene->soundEntities[index].sourceID != 0) SoundSystem_DeleteSource(scene->soundEntities[index].sourceID);
    for (int i = index; i < scene->numSoundEntities - 1; ++i) scene->soundEntities[i] = scene->soundEntities[i + 1];
    scene->numSoundEntities--;
}

void _raw_delete_particle_emitter(Scene* scene, int index) {
    if (index < 0 || index >= scene->numParticleEmitters) return;
    ParticleEmitter_Free(&scene->particleEmitters[index]);
    if (scene->particleEmitters[index].system) ParticleSystem_Free(scene->particleEmitters[index].system);
    for (int i = index; i < scene->numParticleEmitters - 1; ++i) scene->particleEmitters[i] = scene->particleEmitters[i + 1];
    scene->numParticleEmitters--;
}

void _raw_delete_sprite(Scene* scene, int index) {
    if (index < 0 || index >= scene->numSprites) return;
    for (int i = index; i < scene->numSprites - 1; ++i) scene->sprites[i] = scene->sprites[i + 1];
    scene->numSprites--;
}

void _raw_delete_video_player(Scene* scene, int index) {
    if (index < 0 || index >= scene->numVideoPlayers) return;
    VideoPlayer_Free(&scene->videoPlayers[index]);
    for (int i = index; i < scene->numVideoPlayers - 1; ++i) scene->videoPlayers[i] = scene->videoPlayers[i + 1];
    scene->numVideoPlayers--;
}

void _raw_delete_parallax_room(Scene* scene, int index) {
    if (index < 0 || index >= scene->numParallaxRooms) return;
    if (scene->parallaxRooms[index].cubemapTexture) glDeleteTextures(1, &scene->parallaxRooms[index].cubemapTexture);
    for (int i = index; i < scene->numParallaxRooms - 1; ++i) scene->parallaxRooms[i] = scene->parallaxRooms[i + 1];
    scene->numParallaxRooms--;
}

void _raw_delete_logic_entity(Scene* scene, int index) {
    if (index < 0 || index >= scene->numLogicEntities) return;
    for (int i = index; i < scene->numLogicEntities - 1; ++i) scene->logicEntities[i] = scene->logicEntities[i + 1];
    scene->numLogicEntities--;
}

static void free_entity_state_data(EntityState* state) {
    if (state->type == ENTITY_BRUSH) {
        Brush_FreeData(&state->data.brush);
    }
}

static void free_action_data(Action* action) {
    if (!action) return;
    for (int i = 0; i < action->num_before_states; ++i) free_entity_state_data(&action->before_states[i]);
    if (action->before_states) free(action->before_states);
    for (int i = 0; i < action->num_after_states; ++i) free_entity_state_data(&action->after_states[i]);
    if (action->after_states) free(action->after_states);
}

void capture_state(EntityState* state, Scene* scene, EntityType type, int index) {
    state->type = type; state->index = index;
    memset(state->modelPath, 0, sizeof(state->modelPath)); memset(state->parFile, 0, sizeof(state->parFile)); memset(state->soundPath, 0, sizeof(state->soundPath));
    switch (type) {
    case ENTITY_MODEL: state->data.object = scene->objects[index]; strcpy(state->modelPath, scene->objects[index].modelPath); break;
    case ENTITY_BRUSH: Brush_DeepCopy(&state->data.brush, &scene->brushes[index]); break;
    case ENTITY_LIGHT: state->data.light = scene->lights[index]; break;
    case ENTITY_DECAL: state->data.decal = scene->decals[index]; break;
    case ENTITY_SOUND: state->data.soundEntity = scene->soundEntities[index]; strcpy(state->soundPath, scene->soundEntities[index].soundPath); break;
    case ENTITY_PARTICLE_EMITTER: memcpy(&state->data.particleEmitter, &scene->particleEmitters[index], offsetof(ParticleEmitter, particles)); strcpy(state->parFile, scene->particleEmitters[index].parFile); break;
    case ENTITY_SPRITE: state->data.sprite = scene->sprites[index]; break;
    case ENTITY_VIDEO_PLAYER: state->data.videoPlayer = scene->videoPlayers[index]; break;
    case ENTITY_PARALLAX_ROOM: state->data.parallaxRoom = scene->parallaxRooms[index]; break;
    case ENTITY_PLAYERSTART: state->data.playerStart = scene->playerStart; break;
    case ENTITY_LOGIC: state->data.logicEntity = scene->logicEntities[index]; break;
    default: break;
    }
}

static void apply_state(Scene* scene, Engine* engine, EntityState* state, bool is_creation) {
    switch (state->type) {
    case ENTITY_MODEL: {
        if (is_creation) {
            scene->numObjects++;
            scene->objects = (SceneObject*)realloc(scene->objects, scene->numObjects * sizeof(SceneObject));
            memmove(&scene->objects[state->index + 1], &scene->objects[state->index], (scene->numObjects - 1 - state->index) * sizeof(SceneObject));
        }

        SceneObject* obj = &scene->objects[state->index];
        if (!is_creation) {
            if (obj->model) Model_Free(obj->model);
            if (obj->physicsBody) Physics_RemoveRigidBody(engine->physicsWorld, obj->physicsBody);
            if (obj->bakedVertexColors) free(obj->bakedVertexColors);
        }

        *obj = state->data.object;
        strcpy(obj->modelPath, state->modelPath);
        obj->model = Model_Load(obj->modelPath);
        obj->physicsBody = NULL;

        if (obj->model && obj->model->combinedVertexData && obj->model->totalIndexCount > 0) {
            Mat4 physics_transform = create_trs_matrix(obj->pos, obj->rot, (Vec3) { 1, 1, 1 });
            obj->physicsBody = Physics_CreateStaticTriangleMesh(engine->physicsWorld, obj->model->combinedVertexData, obj->model->totalVertexCount, obj->model->combinedIndexData, obj->model->totalIndexCount, physics_transform, obj->scale);
        }
        break;
    }
    case ENTITY_BRUSH: {
        if (is_creation) {
            if (scene->numBrushes >= MAX_BRUSHES) return;
            memmove(&scene->brushes[state->index + 1], &scene->brushes[state->index], (scene->numBrushes - state->index) * sizeof(Brush));
            scene->numBrushes++;
            memset(&scene->brushes[state->index], 0, sizeof(Brush));
        }

        Brush* b = &scene->brushes[state->index];
        if (!is_creation) {
            if (b->physicsBody) Physics_RemoveRigidBody(engine->physicsWorld, b->physicsBody);
            Brush_FreeData(b);
        }

        Brush_DeepCopy(b, &state->data.brush);
        Brush_UpdateMatrix(b);
        Brush_CreateRenderData(b);
        b->physicsBody = NULL;

        if (Brush_IsSolid(b) && b->numVertices > 0) {
            if (b->mass > 0.0f) {
                b->physicsBody = Physics_CreateDynamicBrush(engine->physicsWorld, (const float*)b->vertices, b->numVertices, b->mass, b->modelMatrix);
            }
            else {
                Vec3* world_verts = (Vec3*)malloc(b->numVertices * sizeof(Vec3));
                for (int i = 0; i < b->numVertices; ++i) world_verts[i] = mat4_mul_vec3(&b->modelMatrix, b->vertices[i].pos);
                b->physicsBody = Physics_CreateStaticConvexHull(engine->physicsWorld, (const float*)world_verts, b->numVertices);
                free(world_verts);
            }
        }
        break;
    }
    case ENTITY_LIGHT: {
        if (is_creation) {
            if (scene->numActiveLights >= MAX_LIGHTS) return;
            memmove(&scene->lights[state->index + 1], &scene->lights[state->index], (scene->numActiveLights - state->index) * sizeof(Light));
            scene->numActiveLights++;
        }
        if (!is_creation) Light_DestroyShadowMap(&scene->lights[state->index]);
        scene->lights[state->index] = state->data.light;
        Light_InitShadowMap(&scene->lights[state->index]);
        break;
    }
    case ENTITY_DECAL:
        if (is_creation) {
            if (scene->numDecals >= MAX_DECALS) return;
            memmove(&scene->decals[state->index + 1], &scene->decals[state->index], (scene->numDecals - state->index) * sizeof(Decal));
            scene->numDecals++;
        }
        scene->decals[state->index] = state->data.decal;
        break;
    case ENTITY_SOUND:
        if (is_creation) {
            if (scene->numSoundEntities >= MAX_SOUNDS) return;
            memmove(&scene->soundEntities[state->index + 1], &scene->soundEntities[state->index], (scene->numSoundEntities - state->index) * sizeof(SoundEntity));
            scene->numSoundEntities++;
        }
        if (!is_creation && scene->soundEntities[state->index].sourceID != 0) SoundSystem_DeleteSource(scene->soundEntities[state->index].sourceID);
        scene->soundEntities[state->index] = state->data.soundEntity;
        scene->soundEntities[state->index].bufferID = SoundSystem_LoadSound(state->soundPath);
        break;
    case ENTITY_PARTICLE_EMITTER:
        if (is_creation) {
            if (scene->numParticleEmitters >= MAX_PARTICLE_EMITTERS) return;
            memmove(&scene->particleEmitters[state->index + 1], &scene->particleEmitters[state->index], (scene->numParticleEmitters - state->index) * sizeof(ParticleEmitter));
            scene->numParticleEmitters++;
        }
        if (!is_creation) {
            ParticleEmitter_Free(&scene->particleEmitters[state->index]);
            if (scene->particleEmitters[state->index].system) ParticleSystem_Free(scene->particleEmitters[state->index].system);
        }
        scene->particleEmitters[state->index] = state->data.particleEmitter;
        ParticleSystem* ps = ParticleSystem_Load(state->parFile);
        if (ps) ParticleEmitter_Init(&scene->particleEmitters[state->index], ps, state->data.particleEmitter.pos);
        break;
    case ENTITY_SPRITE:
        if (is_creation) {
            if (scene->numSprites >= MAX_SPRITES) return;
            memmove(&scene->sprites[state->index + 1], &scene->sprites[state->index], (scene->numSprites - state->index) * sizeof(Sprite));
            scene->numSprites++;
        }
        scene->sprites[state->index] = state->data.sprite;
        break;
    case ENTITY_VIDEO_PLAYER:
        if (is_creation) {
            if (scene->numVideoPlayers >= MAX_VIDEO_PLAYERS) return;
            memmove(&scene->videoPlayers[state->index + 1], &scene->videoPlayers[state->index], (scene->numVideoPlayers - state->index) * sizeof(VideoPlayer));
            scene->numVideoPlayers++;
        }
        if (!is_creation) VideoPlayer_Free(&scene->videoPlayers[state->index]);
        scene->videoPlayers[state->index] = state->data.videoPlayer;
        VideoPlayer_Load(&scene->videoPlayers[state->index]);
        break;
    case ENTITY_PARALLAX_ROOM:
        if (is_creation) {
            if (scene->numParallaxRooms >= MAX_PARALLAX_ROOMS) return;
            memmove(&scene->parallaxRooms[state->index + 1], &scene->parallaxRooms[state->index], (scene->numParallaxRooms - state->index) * sizeof(ParallaxRoom));
            scene->numParallaxRooms++;
        }
        if (!is_creation && scene->parallaxRooms[state->index].cubemapTexture) glDeleteTextures(1, &scene->parallaxRooms[state->index].cubemapTexture);
        scene->parallaxRooms[state->index] = state->data.parallaxRoom;
        break;
    case ENTITY_LOGIC:
        if (is_creation) {
            if (scene->numLogicEntities >= MAX_LOGIC_ENTITIES) return;
            memmove(&scene->logicEntities[state->index + 1], &scene->logicEntities[state->index], (scene->numLogicEntities - state->index) * sizeof(LogicEntity));
            scene->numLogicEntities++;
        }
        scene->logicEntities[state->index] = state->data.logicEntity;
        break;
    case ENTITY_PLAYERSTART:
        scene->playerStart = state->data.playerStart;
        break;
    default: break;
    }
}

static void clear_stack(Action** stack, int* top) {
    for (int i = 0; i <= *top; ++i) { free_action_data(stack[i]); free(stack[i]); }
    *top = -1;
}

void Undo_Init() { g_undo_top = -1; g_redo_top = -1; g_is_modifying = false; }

void Undo_Shutdown() {
    clear_stack(g_undo_stack, &g_undo_top);
    clear_stack(g_redo_stack, &g_redo_top);
    if (g_multi_before_states) {
        for (int i = 0; i < g_num_multi_before_states; ++i) free_entity_state_data(&g_multi_before_states[i]);
        free(g_multi_before_states);
        g_multi_before_states = NULL;
    }
}

static void Undo_PushAction(Action* action) {
    clear_stack(g_redo_stack, &g_redo_top);
    if (g_undo_top >= MAX_UNDO_ACTIONS - 1) {
        free_action_data(g_undo_stack[0]); free(g_undo_stack[0]);
        memmove(&g_undo_stack[0], &g_undo_stack[1], (MAX_UNDO_ACTIONS - 1) * sizeof(Action*));
        g_undo_top--;
    }
    g_undo_top++; g_undo_stack[g_undo_top] = action;
    Editor_SetMapDirty(true);
}

static void deep_copy_entity_state(EntityState* dest, const EntityState* src) {
    *dest = *src;
    if (src->type == ENTITY_BRUSH) {
        Brush_DeepCopy(&dest->data.brush, &src->data.brush);
    }
}

static Action* deep_copy_action(const Action* src) {
    Action* dest = (Action*)calloc(1, sizeof(Action));
    if (!dest) return NULL;

    dest->type = src->type;
    strncpy(dest->description, src->description, sizeof(dest->description) - 1);

    dest->num_before_states = src->num_before_states;
    if (src->num_before_states > 0) {
        dest->before_states = (EntityState*)calloc(src->num_before_states, sizeof(EntityState));
        for (int i = 0; i < src->num_before_states; ++i) {
            deep_copy_entity_state(&dest->before_states[i], &src->before_states[i]);
        }
    }

    dest->num_after_states = src->num_after_states;
    if (src->num_after_states > 0) {
        dest->after_states = (EntityState*)calloc(src->num_after_states, sizeof(EntityState));
        for (int i = 0; i < src->num_after_states; ++i) {
            deep_copy_entity_state(&dest->after_states[i], &src->after_states[i]);
        }
    }
    return dest;
}

void Undo_PerformUndo(Scene* scene, Engine* engine) {
    if (g_undo_top < 0) return;
    Action* action = g_undo_stack[g_undo_top--];
    if (g_redo_top >= MAX_UNDO_ACTIONS - 1) { free_action_data(g_redo_stack[0]); free(g_redo_stack[0]); memmove(&g_redo_stack[0], &g_redo_stack[1], (MAX_UNDO_ACTIONS - 1) * sizeof(Action*)); g_redo_top--; }
    g_redo_stack[++g_redo_top] = deep_copy_action(action);
    switch (action->type) {
    case ACTION_MODIFY_ENTITY:  if (action->num_after_states == 1 && action->num_before_states > 1) {
            _raw_delete_brush(scene, engine, action->after_states[0].index);
            for (int i = 0; i < action->num_before_states; ++i) {
                apply_state(scene, engine, &action->before_states[i], true);
            }
        } else {
            for (int i = 0; i < action->num_before_states; ++i) {
                apply_state(scene, engine, &action->before_states[i], false);
            }
        } break;
    case ACTION_CREATE_ENTITY:  Editor_ClearSelection(); for (int i = action->num_after_states - 1; i >= 0; --i) {
        switch (action->after_states[i].type) {
        case ENTITY_MODEL: _raw_delete_model(scene, action->after_states[i].index, engine); break;
        case ENTITY_BRUSH: _raw_delete_brush(scene, engine, action->after_states[i].index); break;
        case ENTITY_LIGHT: _raw_delete_light(scene, action->after_states[i].index); break;
        case ENTITY_DECAL: _raw_delete_decal(scene, action->after_states[i].index); break;
        case ENTITY_SOUND: _raw_delete_sound_entity(scene, action->after_states[i].index); break;
        case ENTITY_PARTICLE_EMITTER: _raw_delete_particle_emitter(scene, action->after_states[i].index); break;
        case ENTITY_SPRITE: _raw_delete_sprite(scene, action->after_states[i].index); break;
        case ENTITY_VIDEO_PLAYER: _raw_delete_video_player(scene, action->after_states[i].index); break;
        case ENTITY_PARALLAX_ROOM: _raw_delete_parallax_room(scene, action->after_states[i].index); break;
        case ENTITY_LOGIC: _raw_delete_logic_entity(scene, action->after_states[i].index); break;
        default: break;
        }
    } break;
    case ACTION_DELETE_ENTITY: for (int i = 0; i < action->num_before_states; ++i) apply_state(scene, engine, &action->before_states[i], true); break;
    default: break;
    }
}

void Undo_PerformRedo(Scene* scene, Engine* engine) {
    if (g_redo_top < 0) return;
    Action* action = g_redo_stack[g_redo_top--];
    g_undo_stack[++g_undo_top] = action;
    switch (action->type) {
    case ACTION_MODIFY_ENTITY:   if (action->num_after_states == 1 && action->num_before_states > 1) {
            for (int i = action->num_before_states - 1; i >= 0; --i) {
                _raw_delete_brush(scene, engine, action->before_states[i].index);
            }
            apply_state(scene, engine, &action->after_states[0], true);
        } else {
            for (int i = 0; i < action->num_after_states; ++i) {
                apply_state(scene, engine, &action->after_states[i], false);
            }
        } break;
    case ACTION_CREATE_ENTITY: for (int i = 0; i < action->num_after_states; ++i) apply_state(scene, engine, &action->after_states[i], true); break;
    case ACTION_DELETE_ENTITY: for (int i = action->num_before_states - 1; i >= 0; --i) {
        switch (action->before_states[i].type) {
        case ENTITY_MODEL: _raw_delete_model(scene, action->before_states[i].index, engine); break;
        case ENTITY_BRUSH: _raw_delete_brush(scene, engine, action->before_states[i].index); break;
        case ENTITY_LIGHT: _raw_delete_light(scene, action->before_states[i].index); break;
        case ENTITY_DECAL: _raw_delete_decal(scene, action->before_states[i].index); break;
        case ENTITY_SOUND: _raw_delete_sound_entity(scene, action->before_states[i].index); break;
        case ENTITY_PARTICLE_EMITTER: _raw_delete_particle_emitter(scene, action->before_states[i].index); break;
        case ENTITY_SPRITE: _raw_delete_sprite(scene, action->before_states[i].index); break;
        case ENTITY_VIDEO_PLAYER: _raw_delete_video_player(scene, action->before_states[i].index); break;
        case ENTITY_PARALLAX_ROOM: _raw_delete_parallax_room(scene, action->before_states[i].index); break;
        case ENTITY_LOGIC: _raw_delete_logic_entity(scene, action->before_states[i].index); break;
        default: break;
        }
    } break;
    default: break;
    }
}

static void capture_unique_states(Scene* scene, EditorSelection* selections, int num_selections, EntityState** states, int* num_states) {
    if (num_selections == 0) { *states = NULL; *num_states = 0; return; }
    *states = (EntityState*)calloc(num_selections, sizeof(EntityState));
    *num_states = 0;
    for (int i = 0; i < num_selections; ++i) {
        EditorSelection* sel = &selections[i]; bool found = false;
        for (int j = 0; j < *num_states; ++j) { if ((*states)[j].type == sel->type && (*states)[j].index == sel->index) { found = true; break; } }
        if (!found) { capture_state(&(*states)[*num_states], scene, sel->type, sel->index); (*num_states)++; }
    }
    if (*num_states < num_selections) *states = (EntityState*)realloc(*states, *num_states * sizeof(EntityState));
}

void Undo_BeginMultiEntityModification(Scene* scene, EditorSelection* selections, int num_selections) {
    if (g_is_modifying) return;
    capture_unique_states(scene, selections, num_selections, &g_multi_before_states, &g_num_multi_before_states);
    g_is_modifying = true;
}

void Undo_EndMultiEntityModification(Scene* scene, EditorSelection* selections, int num_selections, const char* description) {
    if (!g_is_modifying) return;
    g_is_modifying = false;
    Action* action = (Action*)calloc(1, sizeof(Action)); if (!action) return;
    action->type = ACTION_MODIFY_ENTITY; strncpy(action->description, description, sizeof(action->description) - 1);
    action->before_states = g_multi_before_states; action->num_before_states = g_num_multi_before_states;
    capture_unique_states(scene, selections, num_selections, &action->after_states, &action->num_after_states);
    g_multi_before_states = NULL; g_num_multi_before_states = 0;
    Undo_PushAction(action);
}

void Undo_PushCreateMultipleEntities(Scene* scene, EditorSelection* selections, int num_selections, const char* description) {
    Action* action = (Action*)calloc(1, sizeof(Action)); if (!action) return;
    action->type = ACTION_CREATE_ENTITY; strncpy(action->description, description, sizeof(action->description) - 1);
    capture_unique_states(scene, selections, num_selections, &action->after_states, &action->num_after_states);
    action->before_states = NULL; action->num_before_states = 0;
    Undo_PushAction(action);
}

void Undo_PushDeleteMultipleEntities(Scene* scene, EntityState* deleted_states, int num_states, const char* description) {
    Action* action = (Action*)calloc(1, sizeof(Action)); if (!action) return;
    action->type = ACTION_DELETE_ENTITY; strncpy(action->description, description, sizeof(action->description) - 1);
    action->before_states = deleted_states; action->num_before_states = num_states;
    action->after_states = NULL; action->num_after_states = 0;
    Undo_PushAction(action);
}

void Undo_PushMergeAction(Scene* scene, EntityState* before_states, int num_before, EntityState* after_states, int num_after, const char* description) {
    Action* action = (Action*)calloc(1, sizeof(Action));
    if (!action) return;
    action->type = ACTION_MODIFY_ENTITY;
    strncpy(action->description, description, sizeof(action->description) - 1);
    action->before_states = before_states;
    action->num_before_states = num_before;
    action->after_states = after_states;
    action->num_after_states = num_after;
    Undo_PushAction(action);
}

void Undo_BeginEntityModification(Scene* scene, EntityType type, int index) {
    EditorSelection sel = { type, index, -1, -1 };
    Undo_BeginMultiEntityModification(scene, &sel, 1);
}

void Undo_EndEntityModification(Scene* scene, EntityType type, int index, const char* description) {
    EditorSelection sel = { type, index, -1, -1 };
    Undo_EndMultiEntityModification(scene, &sel, 1, description);
}

void Undo_PushCreateEntity(Scene* scene, EntityType type, int index, const char* description) {
    EditorSelection sel = { type, index, -1, -1 };
    Undo_PushCreateMultipleEntities(scene, &sel, 1, description);
}

void Undo_PushDeleteEntity(Scene* scene, EntityType type, int index, const char* description) {
    EntityState* state = (EntityState*)malloc(sizeof(EntityState));
    capture_state(state, scene, type, index);
    Undo_PushDeleteMultipleEntities(scene, state, 1, description);
}