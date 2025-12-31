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
#ifndef EDITOR_GEOMETRY_H
#define EDITOR_GEOMETRY_H

#include "editor_internal.h"

#ifdef __cplusplus
extern "C" {
#endif

	void Editor_SubdivideBrushFace(Scene* scene, Engine* engine, int brush_index, int face_index, int u_divs, int v_divs);
	void Editor_CreateBrushFromPreview(Scene* scene, Engine* engine, Brush* preview);

	void Editor_UpdatePreviewBrushFromWorldMinMax();
	void Editor_UpdatePreviewBrushForInitialDrag(Vec3 p1_world_drag, Vec3 p2_world_drag, ViewportType creation_view);

	void Editor_AdjustPreviewBrushByHandle(Vec2 mouse_pos_in_viewport, ViewportType current_view);
	void Editor_AdjustPreviewBrush(Vec2 mouse_pos, ViewportType adjust_view);
	void Editor_AdjustSelectedBrushByHandle(Scene* scene, Engine* engine, Vec2 mouse_pos, ViewportType view);

#ifdef __cplusplus
}
#endif

#endif