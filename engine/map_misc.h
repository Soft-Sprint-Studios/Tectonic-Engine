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
#ifndef MAP_MISC_H
#define MAP_MISC_H

#include "map.h"

	void SceneObject_UpdateMatrix(SceneObject* obj);
	void ParallaxRoom_UpdateMatrix(ParallaxRoom* p);
	void Decal_UpdateMatrix(Decal* d);
	void Brush_UpdateMatrix(Brush* b);
	void Brush_FreeData(Brush* b);
	void Brush_DeepCopy(Brush* dest, const Brush* src);
	Bool Brush_IsSolid(const Brush* b);
	void Brush_GetLocalAABB(const Brush* b, Vec3* out_min, Vec3* out_max);
	void Brush_GetWorldAABB(const Brush* b, Vec3* out_min, Vec3* out_max);
	void CreateMapBackup(const Char* originalPath);
	void Brush_CreateRenderData(Brush* b);


#endif // MAP_MISC_H