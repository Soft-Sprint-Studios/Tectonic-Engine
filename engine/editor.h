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
void Editor_DeleteSprite(Scene* scene, int index);

void Editor_DuplicateModel(Scene* scene, Engine* engine, int index);
void Editor_DuplicateBrush(Scene* scene, Engine* engine, int index);
void Editor_DuplicateLight(Scene* scene, int index);
void Editor_DuplicateDecal(Scene* scene, int index);
void Editor_DuplicateSoundEntity(Scene* scene, int index);
void Editor_DuplicateParticleEmitter(Scene* scene, int index);
void Editor_DuplicateVideoPlayer(Scene* scene, int index);
void Editor_DuplicateParallaxRoom(Scene* scene, int index);
void Editor_DuplicateLogicEntity(Scene* scene, Engine* engine, int index);
void Editor_DuplicateSprite(Scene* scene, int index);

#ifdef __cplusplus
}
#endif

#endif // EDITOR_H