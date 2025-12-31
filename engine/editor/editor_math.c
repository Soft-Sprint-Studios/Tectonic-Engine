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
#include "editor_math.h"
#include <math.h>
#include <float.h>

float SnapValue(float value, float snap_interval) {
    if (snap_interval == 0.0f) {
        return value;
    }

    float divided = value / snap_interval;
    float rounded = roundf(divided);
    float result = rounded * snap_interval;

    return result;
}

float SnapAngle(float value, float snap_interval) {
    if (snap_interval == 0.0f) {
        return value;
    }

    float divided = value / snap_interval;
    float rounded = roundf(divided);
    float result = rounded * snap_interval;

    return result;
}

Vec3 ScreenToWorld(Vec2 screen_pos, ViewportType viewport) {
    float width = (float)g_EditorState.viewport_width[viewport]; float height = (float)g_EditorState.viewport_height[viewport];
    if (width <= 0 || height <= 0) return (Vec3) { 0, 0, 0 };
    float aspect = width / height; float zoom = g_EditorState.ortho_cam_zoom[viewport - 1]; Vec3 cam_pos = g_EditorState.ortho_cam_pos[viewport - 1];
    float ndc_x = (screen_pos.x / width) * 2.0f - 1.0f; float ndc_y = 1.0f - (screen_pos.y / height) * 2.0f;
    Vec3 world_pos = { 0 };
    switch (viewport) {
    case VIEW_TOP_XZ: world_pos.x = cam_pos.x + ndc_x * zoom * aspect; world_pos.z = cam_pos.z - ndc_y * zoom; world_pos.y = 0; break;
    case VIEW_FRONT_XY: world_pos.x = cam_pos.x + ndc_x * zoom * aspect; world_pos.y = cam_pos.y + ndc_y * zoom; world_pos.z = 0; break;
    case VIEW_SIDE_YZ: world_pos.z = cam_pos.z - ndc_x * zoom * aspect; world_pos.y = cam_pos.y + ndc_y * zoom; world_pos.x = 0; break;
    default: break;
    }
    if (g_EditorState.snap_to_grid) { world_pos.x = SnapValue(world_pos.x, g_EditorState.grid_size); world_pos.y = SnapValue(world_pos.y, g_EditorState.grid_size); world_pos.z = SnapValue(world_pos.z, g_EditorState.grid_size); }
    return world_pos;
}

Vec3 ScreenToWorld_Unsnapped_ForOrthoPicking(Vec2 screen_pos, ViewportType viewport) {
    if (viewport == VIEW_PERSPECTIVE || viewport >= VIEW_COUNT) {
        return (Vec3) { 0, 0, 0 };
    }
    float width = (float)g_EditorState.viewport_width[viewport];
    float height = (float)g_EditorState.viewport_height[viewport];
    if (width <= 0 || height <= 0) return (Vec3) { 0, 0, 0 };
    float aspect = width / height;
    int ortho_array_idx = viewport - 1;
    float zoom = g_EditorState.ortho_cam_zoom[ortho_array_idx];
    Vec3 cam_center_on_plane = g_EditorState.ortho_cam_pos[ortho_array_idx];
    float ndc_x = (screen_pos.x / width) * 2.0f - 1.0f;
    float ndc_y = 1.0f - (screen_pos.y / height) * 2.0f;
    Vec3 world_pos = { 0 };
    switch (viewport) {
    case VIEW_TOP_XZ:
        world_pos.x = cam_center_on_plane.x + ndc_x * zoom * aspect;
        world_pos.z = cam_center_on_plane.z - ndc_y * zoom;
        world_pos.y = 0;
        break;
    case VIEW_FRONT_XY:
        world_pos.x = cam_center_on_plane.x + ndc_x * zoom * aspect;
        world_pos.y = cam_center_on_plane.y + ndc_y * zoom;
        world_pos.z = 0;
        break;
    case VIEW_SIDE_YZ:
        world_pos.z = cam_center_on_plane.z - ndc_x * zoom * aspect;
        world_pos.y = cam_center_on_plane.y + ndc_y * zoom;
        world_pos.x = 0;
        break;
    default:
        break;
    }
    return world_pos;
}

Vec3 ScreenToWorld_Clip(Vec2 screen_pos, ViewportType viewport) {
    float width = (float)g_EditorState.viewport_width[viewport]; float height = (float)g_EditorState.viewport_height[viewport];
    if (width <= 0 || height <= 0) return (Vec3) { 0, 0, 0 };
    float aspect = width / height; float zoom = g_EditorState.ortho_cam_zoom[viewport - 1]; Vec3 cam_pos = g_EditorState.ortho_cam_pos[viewport - 1];
    float ndc_x = (screen_pos.x / width) * 2.0f - 1.0f; float ndc_y = 1.0f - (screen_pos.y / height) * 2.0f;
    Vec3 world_pos = { 0 };
    switch (viewport) {
    case VIEW_TOP_XZ:   world_pos.x = cam_pos.x + ndc_x * zoom * aspect; world_pos.z = cam_pos.z - ndc_y * zoom; world_pos.y = g_EditorState.clip_plane_depth; break;
    case VIEW_FRONT_XY: world_pos.x = cam_pos.x + ndc_x * zoom * aspect; world_pos.y = cam_pos.y + ndc_y * zoom; world_pos.z = g_EditorState.clip_plane_depth; break;
    case VIEW_SIDE_YZ:  world_pos.z = cam_pos.z - ndc_x * zoom * aspect; world_pos.y = cam_pos.y + ndc_y * zoom; world_pos.x = g_EditorState.clip_plane_depth; break;
    default: break;
    }
    if (g_EditorState.snap_to_grid) {
        world_pos.x = SnapValue(world_pos.x, g_EditorState.grid_size);
        world_pos.y = SnapValue(world_pos.y, g_EditorState.grid_size);
        world_pos.z = SnapValue(world_pos.z, g_EditorState.grid_size);
    }
    return world_pos;
}

Vec2 WorldToScreen(Vec3 world_pos, ViewportType viewport) {
    if (viewport >= VIEW_COUNT) return (Vec2) { 0, 0 };

    Vec4 clip_pos = { world_pos.x, world_pos.y, world_pos.z, 1.0f };

    clip_pos = mat4_mul_vec4(&g_view_matrix[viewport], clip_pos);
    clip_pos = mat4_mul_vec4(&g_proj_matrix[viewport], clip_pos);

    if (clip_pos.w != 0.0f) {
        clip_pos.x /= clip_pos.w;
        clip_pos.y /= clip_pos.w;
    }

    float screen_x = ((clip_pos.x + 1.0f) / 2.0f) * g_EditorState.viewport_width[viewport];
    float screen_y = ((1.0f - clip_pos.y) / 2.0f) * g_EditorState.viewport_height[viewport];

    return (Vec2) { screen_x, screen_y };
}

float dist_RaySegment(Vec3 ray_origin, Vec3 ray_dir, Vec3 seg_p0, Vec3 seg_p1, float* t_ray, float* t_seg) {
    Vec3 seg_dir = vec3_sub(seg_p1, seg_p0);
    Vec3 w0 = vec3_sub(ray_origin, seg_p0);
    float a = vec3_dot(ray_dir, ray_dir);
    float b = vec3_dot(ray_dir, seg_dir);
    float c = vec3_dot(seg_dir, seg_dir);
    float d = vec3_dot(ray_dir, w0);
    float e = vec3_dot(seg_dir, w0);
    float det = a * c - b * b;
    float s, t;
    if (det < 1e-5f) { s = 0.0f; t = e / c; }
    else { s = (b * e - c * d) / det; t = (a * e - b * d) / det; }
    *t_ray = s; *t_seg = fmaxf(0.0f, fminf(1.0f, t));
    Vec3 closest_point_on_ray = vec3_add(ray_origin, vec3_muls(ray_dir, *t_ray));
    Vec3 closest_point_on_seg = vec3_add(seg_p0, vec3_muls(seg_dir, *t_seg));
    return vec3_length(vec3_sub(closest_point_on_ray, closest_point_on_seg));
}

bool ray_plane_intersect(Vec3 ray_origin, Vec3 ray_dir, Vec3 plane_normal, float plane_d, Vec3* intersect_point) {
    float denom = vec3_dot(plane_normal, ray_dir);
    if (fabs(denom) > 1e-6) {
        float t = -(vec3_dot(plane_normal, ray_origin) + plane_d) / denom;
        if (t >= 0) { *intersect_point = vec3_add(ray_origin, vec3_muls(ray_dir, t)); return true; }
    }
    return false;
}