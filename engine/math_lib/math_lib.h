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
#ifndef MATH_LIB_H
#define MATH_LIB_H

//----------------------------------------//
// Brief: Main math library
//----------------------------------------//

#include <math.h>
#include <string.h>
#include <stdbool.h>
#include "math_api.h"

#ifdef __cplusplus
extern "C" {
#endif

	typedef struct { float x, y; } Vec2;
	typedef struct { float x, y, z; } Vec3;
	typedef struct { float x, y, z, w; } Vec4;
	typedef struct { float m[16]; } Mat4;
	typedef struct { Vec4 planes[6]; } Frustum;

	MATH_API Vec3 vec3_add(Vec3 a, Vec3 b);
	MATH_API Vec3 vec3_sub(Vec3 a, Vec3 b);
	MATH_API Vec3 vec3_muls(Vec3 v, float s);
	MATH_API Vec3 vec3_mul(Vec3 a, Vec3 b);
	MATH_API float vec3_dot(Vec3 a, Vec3 b);
	MATH_API float vec3_length(Vec3 v);
	MATH_API float vec3_length_sq(Vec3 v);
	MATH_API void vec3_normalize(Vec3* v);
	MATH_API Vec3 vec3_cross(Vec3 a, Vec3 b);
	MATH_API Vec3 mat4_mul_vec3(const Mat4* m, Vec3 v);
	MATH_API void mat4_identity(Mat4* m);
	MATH_API void mat4_multiply(Mat4* result, const Mat4* a, const Mat4* b);
	MATH_API bool mat4_inverse(const Mat4* m, Mat4* out);
	MATH_API Mat4 mat4_perspective(float fov_rad, float aspect, float near_p, float far_p);
	MATH_API Mat4 mat4_lookAt(Vec3 eye, Vec3 center, Vec3 up);
	MATH_API Mat4 mat4_ortho(float left, float right, float bottom, float top, float near_p, float far_p);
	MATH_API Mat4 mat4_translate(Vec3 pos);
	MATH_API Mat4 mat4_scale(Vec3 scale);
	MATH_API Mat4 mat4_rotate_x(float radians);
	MATH_API Mat4 mat4_rotate_y(float radians);
	MATH_API Mat4 mat4_rotate_z(float radians);
	MATH_API Vec4 vec4_add(Vec4 a, Vec4 b);
	MATH_API Vec4 vec4_muls(Vec4 v, float s);
	MATH_API Vec3 mat4_mul_vec3_dir(const Mat4* m, Vec3 v);
	MATH_API Vec4 mat4_mul_vec4(const Mat4* m, Vec4 v);
	MATH_API void mat4_decompose(const Mat4* matrix, Vec3* translation, Vec3* rotation, Vec3* scale);
	MATH_API Mat4 create_trs_matrix(Vec3 pos, Vec3 rot_deg, Vec3 scale);
	MATH_API bool RayIntersectsOBB(Vec3 rayOrigin, Vec3 rayDir, const Mat4* modelMatrix, Vec3 localAABBMin, Vec3 localAABBMax, float* t);
	MATH_API void extract_frustum_planes(const Mat4* view_proj, Frustum* frustum, bool normalize);
	MATH_API bool frustum_check_aabb(const Frustum* frustum, Vec3 mins, Vec3 maxs);
	MATH_API Vec3 barycentric_coords(Vec2 p, Vec2 a, Vec2 b, Vec2 c);
	MATH_API bool RayIntersectsTriangle(Vec3 ray_origin, Vec3 ray_dir, Vec3 v0, Vec3 v1, Vec3 v2, float* t_out);

#ifdef __cplusplus
}
#endif

#endif // MATH_LIB_H