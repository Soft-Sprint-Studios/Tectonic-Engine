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
#ifndef EDITOR_MISC_H
#define EDITOR_MISC_H

#include "editor_internal.h"


	void Editor_SetMapDirty(bool is_dirty);
	void Editor_SaveRecentFiles();
	void Editor_LoadRecentFiles();
	void Editor_AddRecentFile(const char* path);
	void Editor_ExecutePendingAction(Engine* engine, Scene* scene, Renderer* renderer);
	void Editor_InitGizmo();
	void Editor_UpdateGizmoHover(Scene* scene, Vec3 ray_origin, Vec3 ray_dir);
	void Editor_InitDebugRenderer();
	void Editor_Init(Engine* engine, Renderer* renderer, Scene* scene);
	void Editor_Shutdown();


#endif // EDITOR_MISC_H