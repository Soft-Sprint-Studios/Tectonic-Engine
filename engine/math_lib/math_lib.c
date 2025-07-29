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

#include "math_lib.h"
#include <float.h>

Vec3 vec3_add(Vec3 a, Vec3 b) {
    return (Vec3) { a.x + b.x, a.y + b.y, a.z + b.z };
}

Vec3 vec3_sub(Vec3 a, Vec3 b) {
    return (Vec3) { a.x - b.x, a.y - b.y, a.z - b.z };
}

Vec3 vec3_muls(Vec3 v, float s) {
    return (Vec3) { v.x* s, v.y* s, v.z* s };
}

Vec3 vec3_mul(Vec3 a, Vec3 b) {
    return (Vec3) { a.x* b.x, a.y* b.y, a.z* b.z };
}

float vec3_dot(Vec3 a, Vec3 b) {
    return a.x * b.x + a.y * b.y + a.z * b.z;
}

float vec3_length_sq(Vec3 v) {
    return v.x * v.x + v.y * v.y + v.z * v.z;
}

float vec3_length(Vec3 v) {
    return sqrtf(vec3_length_sq(v));
}

void vec3_normalize(Vec3* v) {
    float length = vec3_length(*v);
    if (length > 0.0001f) {
        v->x /= length;
        v->y /= length;
        v->z /= length;
    }
}

Vec3 vec3_cross(Vec3 a, Vec3 b) {
    return (Vec3) {
        a.y* b.z - a.z * b.y,
            a.z* b.x - a.x * b.z,
            a.x* b.y - a.y * b.x
    };
}

Vec4 vec4_add(Vec4 a, Vec4 b) {
    return (Vec4) { a.x + b.x, a.y + b.y, a.z + b.z, a.w + b.w };
}

Vec4 vec4_muls(Vec4 v, float s) {
    return (Vec4) { v.x* s, v.y* s, v.z* s, v.w* s };
}

Vec3 mat4_mul_vec3(const Mat4* m, Vec3 v) {
    Vec3 res;
    res.x = m->m[0] * v.x + m->m[4] * v.y + m->m[8] * v.z + m->m[12];
    res.y = m->m[1] * v.x + m->m[5] * v.y + m->m[9] * v.z + m->m[13];
    res.z = m->m[2] * v.x + m->m[6] * v.y + m->m[10] * v.z + m->m[14];
    return res;
}

Vec3 mat4_mul_vec3_dir(const Mat4* m, Vec3 v) {
    Vec3 res;
    res.x = m->m[0] * v.x + m->m[4] * v.y + m->m[8] * v.z;
    res.y = m->m[1] * v.x + m->m[5] * v.y + m->m[9] * v.z;
    res.z = m->m[2] * v.x + m->m[6] * v.y + m->m[10] * v.z;
    return res;
}

Vec4 mat4_mul_vec4(const Mat4* m, Vec4 v) {
    Vec4 res;
    res.x = m->m[0] * v.x + m->m[4] * v.y + m->m[8] * v.z + m->m[12] * v.w;
    res.y = m->m[1] * v.x + m->m[5] * v.y + m->m[9] * v.z + m->m[13] * v.w;
    res.z = m->m[2] * v.x + m->m[6] * v.y + m->m[10] * v.z + m->m[14] * v.w;
    res.w = m->m[3] * v.x + m->m[7] * v.y + m->m[11] * v.z + m->m[15] * v.w;
    return res;
}

void mat4_identity(Mat4* m) {
    memset(m->m, 0, sizeof(float) * 16);
    m->m[0] = m->m[5] = m->m[10] = m->m[15] = 1.0f;
}

void mat4_multiply(Mat4* result, const Mat4* a, const Mat4* b) {
    Mat4 res;
    for (int c = 0; c < 4; ++c) {
        for (int r = 0; r < 4; ++r) {
            float sum = 0.0f;
            for (int k = 0; k < 4; ++k) {
                sum += a->m[k * 4 + r] * b->m[c * 4 + k];
            }
            res.m[c * 4 + r] = sum;
        }
    }
    *result = res;
}

Mat4 mat4_translate(Vec3 pos) {
    Mat4 m;
    mat4_identity(&m);
    m.m[12] = pos.x;
    m.m[13] = pos.y;
    m.m[14] = pos.z;
    return m;
}

Mat4 mat4_scale(Vec3 scale) {
    Mat4 m;
    mat4_identity(&m);
    m.m[0] = scale.x;
    m.m[5] = scale.y;
    m.m[10] = scale.z;
    return m;
}

Mat4 mat4_rotate_x(float rad) {
    Mat4 m = { 0 };
    float c = cosf(rad), s = sinf(rad);
    m.m[0] = 1;
    m.m[5] = c;
    m.m[6] = s;
    m.m[9] = -s;
    m.m[10] = c;
    m.m[15] = 1;
    return m;
}

Mat4 mat4_rotate_y(float rad) {
    Mat4 m = { 0 };
    float c = cosf(rad), s = sinf(rad);
    m.m[0] = c;
    m.m[2] = -s;
    m.m[5] = 1;
    m.m[8] = s;
    m.m[10] = c;
    m.m[15] = 1;
    return m;
}

Mat4 mat4_rotate_z(float rad) {
    Mat4 m = { 0 };
    float c = cosf(rad), s = sinf(rad);
    m.m[0] = c;
    m.m[1] = s;
    m.m[4] = -s;
    m.m[5] = c;
    m.m[10] = 1;
    m.m[15] = 1;
    return m;
}

Mat4 mat4_perspective(float fov_rad, float aspect, float near_p, float far_p) {
    Mat4 m;
    mat4_identity(&m);
    float f = 1.0f / tanf(fov_rad / 2.0f);
    m.m[0] = f / aspect;
    m.m[5] = f;
    m.m[10] = (far_p + near_p) / (near_p - far_p);
    m.m[11] = -1.0f;
    m.m[14] = (2.0f * far_p * near_p) / (near_p - far_p);
    m.m[15] = 0.0f;
    return m;
}

Mat4 mat4_lookAt(Vec3 eye, Vec3 center, Vec3 up) {
    Vec3 f = vec3_sub(center, eye);
    vec3_normalize(&f);
    Vec3 s = vec3_cross(f, up);
    vec3_normalize(&s);
    Vec3 u = vec3_cross(s, f);

    Mat4 m = { 0 };
    m.m[0] = s.x;
    m.m[4] = s.y;
    m.m[8] = s.z;
    m.m[12] = -vec3_dot(s, eye);
    m.m[1] = u.x;
    m.m[5] = u.y;
    m.m[9] = u.z;
    m.m[13] = -vec3_dot(u, eye);
    m.m[2] = -f.x;
    m.m[6] = -f.y;
    m.m[10] = -f.z;
    m.m[14] = vec3_dot(f, eye);
    m.m[3] = 0;
    m.m[7] = 0;
    m.m[11] = 0;
    m.m[15] = 1;
    return m;
}

Mat4 mat4_ortho(float left, float right, float bottom, float top, float near_p, float far_p) {
    Mat4 m;
    mat4_identity(&m);
    m.m[0] = 2.0f / (right - left);
    m.m[5] = 2.0f / (top - bottom);
    m.m[10] = -2.0f / (far_p - near_p);
    m.m[12] = -(right + left) / (right - left);
    m.m[13] = -(top + bottom) / (top - bottom);
    m.m[14] = -(far_p + near_p) / (far_p - near_p);
    return m;
}

bool mat4_inverse(const Mat4* m, Mat4* out) {
    float inv[16], det;

    inv[0] = m->m[5] * m->m[10] * m->m[15] -
        m->m[5] * m->m[11] * m->m[14] -
        m->m[9] * m->m[6] * m->m[15] +
        m->m[9] * m->m[7] * m->m[14] +
        m->m[13] * m->m[6] * m->m[11] -
        m->m[13] * m->m[7] * m->m[10];

    inv[4] = -m->m[4] * m->m[10] * m->m[15] +
        m->m[4] * m->m[11] * m->m[14] +
        m->m[8] * m->m[6] * m->m[15] -
        m->m[8] * m->m[7] * m->m[14] -
        m->m[12] * m->m[6] * m->m[11] +
        m->m[12] * m->m[7] * m->m[10];

    inv[8] = m->m[4] * m->m[9] * m->m[15] -
        m->m[4] * m->m[11] * m->m[13] -
        m->m[8] * m->m[5] * m->m[15] +
        m->m[8] * m->m[7] * m->m[13] +
        m->m[12] * m->m[5] * m->m[11] -
        m->m[12] * m->m[7] * m->m[9];

    inv[12] = -m->m[4] * m->m[9] * m->m[14] +
        m->m[4] * m->m[10] * m->m[13] +
        m->m[8] * m->m[5] * m->m[14] -
        m->m[8] * m->m[6] * m->m[13] -
        m->m[12] * m->m[5] * m->m[10] +
        m->m[12] * m->m[6] * m->m[9];

    inv[1] = -m->m[1] * m->m[10] * m->m[15] +
        m->m[1] * m->m[11] * m->m[14] +
        m->m[9] * m->m[2] * m->m[15] -
        m->m[9] * m->m[3] * m->m[14] -
        m->m[13] * m->m[2] * m->m[11] +
        m->m[13] * m->m[3] * m->m[10];

    inv[5] = m->m[0] * m->m[10] * m->m[15] -
        m->m[0] * m->m[11] * m->m[14] -
        m->m[8] * m->m[2] * m->m[15] +
        m->m[8] * m->m[3] * m->m[14] +
        m->m[12] * m->m[2] * m->m[11] -
        m->m[12] * m->m[3] * m->m[10];

    inv[9] = -m->m[0] * m->m[9] * m->m[15] +
        m->m[0] * m->m[11] * m->m[13] +
        m->m[8] * m->m[1] * m->m[15] -
        m->m[8] * m->m[3] * m->m[13] -
        m->m[12] * m->m[1] * m->m[11] +
        m->m[12] * m->m[3] * m->m[9];

    inv[13] = m->m[0] * m->m[9] * m->m[14] -
        m->m[0] * m->m[10] * m->m[13] -
        m->m[8] * m->m[1] * m->m[14] +
        m->m[8] * m->m[2] * m->m[13] +
        m->m[12] * m->m[1] * m->m[10] -
        m->m[12] * m->m[2] * m->m[9];

    inv[2] = m->m[1] * m->m[6] * m->m[15] -
        m->m[1] * m->m[7] * m->m[14] -
        m->m[5] * m->m[2] * m->m[15] +
        m->m[5] * m->m[3] * m->m[14] +
        m->m[13] * m->m[2] * m->m[7] -
        m->m[13] * m->m[3] * m->m[6];

    inv[6] = -m->m[0] * m->m[6] * m->m[15] +
        m->m[0] * m->m[7] * m->m[14] +
        m->m[4] * m->m[2] * m->m[15] -
        m->m[4] * m->m[3] * m->m[14] -
        m->m[12] * m->m[2] * m->m[7] +
        m->m[12] * m->m[3] * m->m[6];

    inv[10] = m->m[0] * m->m[5] * m->m[15] -
        m->m[0] * m->m[7] * m->m[13] -
        m->m[4] * m->m[1] * m->m[15] +
        m->m[4] * m->m[3] * m->m[13] +
        m->m[12] * m->m[1] * m->m[7] -
        m->m[12] * m->m[3] * m->m[5];

    inv[14] = -m->m[0] * m->m[5] * m->m[14] +
        m->m[0] * m->m[6] * m->m[13] +
        m->m[4] * m->m[1] * m->m[14] -
        m->m[4] * m->m[2] * m->m[13] -
        m->m[12] * m->m[1] * m->m[6] +
        m->m[12] * m->m[2] * m->m[5];

    inv[3] = -m->m[1] * m->m[6] * m->m[11] +
        m->m[1] * m->m[7] * m->m[10] +
        m->m[5] * m->m[2] * m->m[11] -
        m->m[5] * m->m[3] * m->m[10] -
        m->m[9] * m->m[2] * m->m[7] +
        m->m[9] * m->m[3] * m->m[6];

    inv[7] = m->m[0] * m->m[6] * m->m[11] -
        m->m[0] * m->m[7] * m->m[10] -
        m->m[4] * m->m[2] * m->m[11] +
        m->m[4] * m->m[3] * m->m[10] +
        m->m[8] * m->m[2] * m->m[7] -
        m->m[8] * m->m[3] * m->m[6];

    inv[11] = -m->m[0] * m->m[5] * m->m[11] +
        m->m[0] * m->m[7] * m->m[9] +
        m->m[4] * m->m[1] * m->m[11] -
        m->m[4] * m->m[3] * m->m[9] -
        m->m[8] * m->m[1] * m->m[7] +
        m->m[8] * m->m[3] * m->m[5];

    inv[15] = m->m[0] * m->m[5] * m->m[10] -
        m->m[0] * m->m[6] * m->m[9] -
        m->m[4] * m->m[1] * m->m[10] +
        m->m[4] * m->m[2] * m->m[9] +
        m->m[8] * m->m[1] * m->m[6] -
        m->m[8] * m->m[2] * m->m[5];

    det = m->m[0] * inv[0] + m->m[1] * inv[4] + m->m[2] * inv[8] + m->m[3] * inv[12];

    if (det == 0) return false;

    det = 1.0f / det;

    for (int i = 0; i < 16; i++)
        out->m[i] = inv[i] * det;

    return true;
}

Mat4 create_trs_matrix(Vec3 pos, Vec3 rot_deg, Vec3 scale) {
    Mat4 trans_mat = mat4_translate(pos);
    Mat4 rot_x_mat = mat4_rotate_x(rot_deg.x * (M_PI / 180.0f));
    Mat4 rot_y_mat = mat4_rotate_y(rot_deg.y * (M_PI / 180.0f));
    Mat4 rot_z_mat = mat4_rotate_z(rot_deg.z * (M_PI / 180.0f));
    Mat4 scale_mat = mat4_scale(scale);

    Mat4 rot_mat;
    mat4_multiply(&rot_mat, &rot_y_mat, &rot_x_mat);
    mat4_multiply(&rot_mat, &rot_z_mat, &rot_mat);

    Mat4 model_mat;
    mat4_multiply(&model_mat, &rot_mat, &scale_mat);
    mat4_multiply(&model_mat, &trans_mat, &model_mat);

    return model_mat;
}

void mat4_decompose(const Mat4* matrix, Vec3* translation, Vec3* rotation, Vec3* scale) {
    translation->x = matrix->m[12];
    translation->y = matrix->m[13];
    translation->z = matrix->m[14];

    Vec3 row0 = { matrix->m[0], matrix->m[1], matrix->m[2] };
    Vec3 row1 = { matrix->m[4], matrix->m[5], matrix->m[6] };
    Vec3 row2 = { matrix->m[8], matrix->m[9], matrix->m[10] };

    scale->x = vec3_length(row0);
    scale->y = vec3_length(row1);
    scale->z = vec3_length(row2);

    if (fabs(scale->x) < 1e-6f || fabs(scale->y) < 1e-6f || fabs(scale->z) < 1e-6f) {
        rotation->x = rotation->y = rotation->z = 0.0f;
        return;
    }

    Mat4 rot_mat;
    memcpy(&rot_mat, matrix, sizeof(Mat4));

    rot_mat.m[0] /= scale->x;
    rot_mat.m[1] /= scale->x;
    rot_mat.m[2] /= scale->x;
    rot_mat.m[4] /= scale->y;
    rot_mat.m[5] /= scale->y;
    rot_mat.m[6] /= scale->y;
    rot_mat.m[8] /= scale->z;
    rot_mat.m[9] /= scale->z;
    rot_mat.m[10] /= scale->z;

    float sy = sqrtf(rot_mat.m[0] * rot_mat.m[0] + rot_mat.m[4] * rot_mat.m[4]);
    bool singular = sy < 1e-6f;

    float x_rad, y_rad, z_rad;

    if (!singular) {
        x_rad = atan2f(rot_mat.m[9], rot_mat.m[10]);
        y_rad = atan2f(-rot_mat.m[8], sy);
        z_rad = atan2f(rot_mat.m[4], rot_mat.m[0]);
    }
    else {
        x_rad = atan2f(-rot_mat.m[6], rot_mat.m[5]);
        y_rad = atan2f(-rot_mat.m[8], sy);
        z_rad = 0.0f;
    }

    const float rad_to_deg = 180.0f / M_PI;
    rotation->x = x_rad * rad_to_deg;
    rotation->y = y_rad * rad_to_deg;
    rotation->z = z_rad * rad_to_deg;
}

bool RayIntersectsOBB(Vec3 rayOrigin, Vec3 rayDir, const Mat4* modelMatrix, Vec3 localAABBMin, Vec3 localAABBMax, float* t) {
    float tMin = 0.0f;
    float tMax = FLT_MAX;

    Mat4 invModelMatrix;
    if (!mat4_inverse(modelMatrix, &invModelMatrix)) {
        return false;
    }

    Vec3 rayOrigin_local = mat4_mul_vec3(&invModelMatrix, rayOrigin);
    Vec3 rayDir_local = mat4_mul_vec3_dir(&invModelMatrix, rayDir);

    for (int i = 0; i < 3; i++) {
        float dir = ((float*)&rayDir_local)[i];
        float origin = ((float*)&rayOrigin_local)[i];
        float min = ((float*)&localAABBMin)[i];
        float max = ((float*)&localAABBMax)[i];

        if (fabsf(dir) < 1e-6f) {
            if (origin < min || origin > max)
                return false;
        }
        else {
            float ood = 1.0f / dir;
            float t1 = (min - origin) * ood;
            float t2 = (max - origin) * ood;

            if (t1 > t2) {
                float temp = t1;
                t1 = t2;
                t2 = temp;
            }

            if (t1 > tMin) tMin = t1;
            if (t2 < tMax) tMax = t2;
            if (tMin > tMax) return false;
        }
    }

    if (t) *t = tMin;
    return true;
}

bool RayIntersectsTriangle(Vec3 ray_origin, Vec3 ray_dir, Vec3 v0, Vec3 v1, Vec3 v2, float* t_out) {
    const float EPSILON = 1e-7f;

    Vec3 edge1 = vec3_sub(v1, v0);
    Vec3 edge2 = vec3_sub(v2, v0);
    Vec3 h = vec3_cross(ray_dir, edge2);

    float a = vec3_dot(edge1, h);
    if (a > -EPSILON && a < EPSILON)
        return false;

    float f = 1.0f / a;
    Vec3 s = vec3_sub(ray_origin, v0);
    float u = f * vec3_dot(s, h);
    if (u < 0.0f || u > 1.0f)
        return false;

    Vec3 q = vec3_cross(s, edge1);
    float v = f * vec3_dot(ray_dir, q);
    if (v < 0.0f || u + v > 1.0f)
        return false;

    float t = f * vec3_dot(edge2, q);
    if (t > EPSILON) {
        if (t_out) *t_out = t;
        return true;
    }

    return false;
}

void extract_frustum_planes(const Mat4* m, Frustum* frustum, bool normalize) {
    frustum->planes[0].x = m->m[3] + m->m[0];
    frustum->planes[0].y = m->m[7] + m->m[4];
    frustum->planes[0].z = m->m[11] + m->m[8];
    frustum->planes[0].w = m->m[15] + m->m[12];

    frustum->planes[1].x = m->m[3] - m->m[0];
    frustum->planes[1].y = m->m[7] - m->m[4];
    frustum->planes[1].z = m->m[11] - m->m[8];
    frustum->planes[1].w = m->m[15] - m->m[12];

    frustum->planes[2].x = m->m[3] + m->m[1];
    frustum->planes[2].y = m->m[7] + m->m[5];
    frustum->planes[2].z = m->m[11] + m->m[9];
    frustum->planes[2].w = m->m[15] + m->m[13];

    frustum->planes[3].x = m->m[3] - m->m[1];
    frustum->planes[3].y = m->m[7] - m->m[5];
    frustum->planes[3].z = m->m[11] - m->m[9];
    frustum->planes[3].w = m->m[15] - m->m[13];

    frustum->planes[4].x = m->m[3] + m->m[2];
    frustum->planes[4].y = m->m[7] + m->m[6];
    frustum->planes[4].z = m->m[11] + m->m[10];
    frustum->planes[4].w = m->m[15] + m->m[14];

    frustum->planes[5].x = m->m[3] - m->m[2];
    frustum->planes[5].y = m->m[7] - m->m[6];
    frustum->planes[5].z = m->m[11] - m->m[10];
    frustum->planes[5].w = m->m[15] - m->m[14];

    if (normalize) {
        for (int i = 0; i < 6; i++) {
            float mag = sqrtf(frustum->planes[i].x * frustum->planes[i].x +
                frustum->planes[i].y * frustum->planes[i].y +
                frustum->planes[i].z * frustum->planes[i].z);
            if (mag > 1e-4f) {
                frustum->planes[i].x /= mag;
                frustum->planes[i].y /= mag;
                frustum->planes[i].z /= mag;
                frustum->planes[i].w /= mag;
            }
        }
    }
}

bool frustum_check_aabb(const Frustum* frustum, Vec3 mins, Vec3 maxs) {
    for (int i = 0; i < 6; i++) {
        Vec3 p_vertex = {
            (frustum->planes[i].x > 0) ? maxs.x : mins.x,
            (frustum->planes[i].y > 0) ? maxs.y : mins.y,
            (frustum->planes[i].z > 0) ? maxs.z : mins.z
        };

        float d = frustum->planes[i].x * p_vertex.x +
            frustum->planes[i].y * p_vertex.y +
            frustum->planes[i].z * p_vertex.z +
            frustum->planes[i].w;

        if (d < 0)
            return false;
    }

    return true;
}

Vec3 barycentric_coords(Vec2 p, Vec2 a, Vec2 b, Vec2 c) {
    Vec2 v0 = { b.x - a.x, b.y - a.y };
    Vec2 v1 = { c.x - a.x, c.y - a.y };
    Vec2 v2 = { p.x - a.x, p.y - a.y };
    float d00 = v0.x * v0.x + v0.y * v0.y;
    float d01 = v0.x * v1.x + v0.y * v1.y;
    float d11 = v1.x * v1.x + v1.y * v1.y;
    float d20 = v2.x * v0.x + v2.y * v0.y;
    float d21 = v2.x * v1.x + v2.y * v1.y;
    float denom = d00 * d11 - d01 * d01;
    if (fabsf(denom) < 1e-5) return (Vec3) { -1.0f, -1.0f, -1.0f };

    float v = (d11 * d20 - d01 * d21) / denom;
    float w = (d00 * d21 - d01 * d20) / denom;
    float u = 1.0f - v - w;
    return (Vec3) { u, v, w };
}