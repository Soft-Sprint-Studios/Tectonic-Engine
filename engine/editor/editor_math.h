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
#ifndef EDITOR_MATH_H
#define EDITOR_MATH_H

#include "editor_internal.h"


	Float SnapValue(Float value, Float snap_interval);

	Vec3 ScreenToWorld(Vec2 screen_pos, ViewportType viewport);
	Vec3 ScreenToWorld_Unsnapped_ForOrthoPicking(Vec2 screen_pos, ViewportType viewport);
	Vec3 ScreenToWorld_Clip(Vec2 screen_pos, ViewportType viewport);
	Vec2 WorldToScreen(Vec3 world_pos, ViewportType viewport);

	Float dist_RaySegment(Vec3 ray_origin, Vec3 ray_dir, Vec3 seg_p0, Vec3 seg_p1, Float* t_ray, Float* t_seg);
	Bool ray_plane_intersect(Vec3 ray_origin, Vec3 ray_dir, Vec3 plane_normal, Float plane_d, Vec3* intersect_point);


#endif // EDITOR_MATH_H