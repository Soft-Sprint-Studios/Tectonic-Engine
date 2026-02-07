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
#ifndef EDITOR_SELECTION_H
#define EDITOR_SELECTION_H

#include "editor_internal.h"


	void Editor_AddToSelection(EntityType type, Int index, Int face_index, Int vertex_index);
	void Editor_RemoveFromSelection(EntityType type, Int index);
	void Editor_RemoveFaceFromSelection(Int brush_index, Int face_index);
	Bool Editor_IsSelected(EntityType type, Int index);
	Bool Editor_IsFaceSelected(Int brush_index, Int face_index);
	Bool FindEntityInScene(Scene* scene, const Char* name, EntityType* out_type, Int* out_index);
	Bool Editor_FindNamedEntityPosition(Scene* scene, const Char* name, Vec3* out_pos);
	EditorSelection* Editor_GetPrimarySelection();
	void Editor_ClearSelection();

	void Editor_PickObjectAtScreenPos(Vec2 screen_pos, ViewportType viewport);
	Int Editor_PickVertexAtScreenPos(Scene* scene, Vec2 screen_pos, ViewportType viewport);


#endif // EDITOR_SELECTION_H