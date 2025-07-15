/*
 * Copyright © 2025 Soft Sprint Studios
 * All rights reserved.
 *
 * This file is proprietary and confidential. Unauthorized reproduction,
 * modification, or distribution is strictly prohibited unless explicit
 * written permission is granted by Soft Sprint Studios.
 */
#ifndef EDITOR_H
#define EDITOR_H

//----------------------------------------//
// Brief: Handles the entire level editor
//----------------------------------------//

#include "map.h"
#include "editor_undo.h"

#ifdef __cplusplus
extern "C" {
#endif

void Editor_Init(Engine* engine, Renderer* renderer, Scene* scene);
void Editor_Shutdown();
void Editor_ProcessEvent(SDL_Event* event, Scene* scene, Engine* engine);
void Editor_Update(Engine* engine, Scene* scene);
void Editor_RenderUI(Engine* engine, Scene* scene, Renderer* renderer);
void Editor_RenderAllViewports(Engine* engine, Renderer* renderer, Scene* scene);
void Editor_SubdivideBrushFace(Scene* scene, Engine* engine, int brush_index, int face_index, int u_divs, int v_divs);

void Editor_DeleteModel(Scene* scene, int index, Engine* engine);
void Editor_DeleteBrush(Scene* scene, Engine* engine, int index);
void Editor_DeleteLight(Scene* scene, int index);
void Editor_DeleteDecal(Scene* scene, int index);
void Editor_DeleteSoundEntity(Scene* scene, int index);
void Editor_DeleteParticleEmitter(Scene* scene, int index);
void Editor_DeleteVideoPlayer(Scene* scene, int index);
void Editor_DeleteParallaxRoom(Scene* scene, int index);
void Editor_DeleteLogicEntity(Scene* scene, int index);

void Editor_DuplicateModel(Scene* scene, Engine* engine, int index);
void Editor_DuplicateBrush(Scene* scene, Engine* engine, int index);
void Editor_DuplicateLight(Scene* scene, int index);
void Editor_DuplicateDecal(Scene* scene, int index);
void Editor_DuplicateSoundEntity(Scene* scene, int index);
void Editor_DuplicateParticleEmitter(Scene* scene, int index);
void Editor_DuplicateVideoPlayer(Scene* scene, int index);
void Editor_DuplicateParallaxRoom(Scene* scene, int index);
void Editor_DuplicateLogicEntity(Scene* scene, Engine* engine, int index);

#ifdef __cplusplus
}
#endif

#endif // EDITOR_H