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
#ifndef EDITOR_UNDO_H
#define EDITOR_UNDO_H

//----------------------------------------//
// Brief: Handles the level editor undos
//----------------------------------------//

#include "map.h"

#ifdef __cplusplus
extern "C" {
#endif

void Undo_Init();
void Undo_Shutdown();
void Undo_PerformUndo(Scene* scene, Engine* engine);
void Undo_PerformRedo(Scene* scene, Engine* engine);
void Undo_BeginEntityModification(Scene* scene, EntityType type, int index);
void Undo_EndEntityModification(Scene* scene, EntityType type, int index, const char* description);
void Undo_PushCreateEntity(Scene* scene, EntityType type, int index, const char* description);
void Undo_PushDeleteEntity(Scene* scene, EntityType type, int index, const char* description);
void _raw_delete_brush(Scene* scene, Engine* engine, int index);

#ifdef __cplusplus
}
#endif

#endif // EDITOR_UNDO_H