/*
 * Copyright © 2025 Soft Sprint Studios
 * All rights reserved.
 *
 * This file is proprietary and confidential. Unauthorized reproduction,
 * modification, or distribution is strictly prohibited unless explicit
 * written permission is granted by Soft Sprint Studios.
 */
#ifdef ARCH_64BIT
#include "lightmapper.h"
#include "gl_console.h"
#include "math_lib.h"
#include <vector>
#include <string>
#include <thread>
#include <mutex>
#include <atomic>
#include <memory>
#include <variant>
#include <filesystem>
#include <fstream>
#include <algorithm>
#include <chrono>
#include <cctype>
#include <cmath>
#include <cfloat>
#include <stdexcept>
#include <limits>
#include <map>
#include <set>
#include <random>
#include <SDL_image.h>
#include <embree4/rtcore.h>
#include "stb_image_write.h"
#include <OpenImageDenoise/oidn.h>

namespace
{
    namespace fs = std::filesystem;

    constexpr float SHADOW_BIAS = 0.01f;
    constexpr int BLUR_RADIUS = 2;
    constexpr int INDIRECT_SAMPLES_PER_POINT = 128;
    constexpr float LUXELS_PER_UNIT = 16.0f;

    void embree_error_function(void* userPtr, RTCError error, const char* str)
    {
        Console_Printf_Error("[Embree] Error %d: %s", error, str);
    }

    void oidn_error_function(void* userPtr, OIDNError error, const char* message)
    {
        Console_Printf_Error("[OIDN] Error %d: %s", error, message);
    }

    struct BrushFaceJobData
    {
        int brush_index;
        int face_index;
        fs::path output_dir;
    };

    struct ModelVertexJobData
    {
        int model_index;
        unsigned int vertex_index;
        Vec4* output_color_buffer;
        Vec4* output_direction_buffer;
    };

    using JobPayload = std::variant<BrushFaceJobData, ModelVertexJobData>;

    class Lightmapper
    {
    public:
        Lightmapper(Scene* scene, int resolution, int bounces);
        ~Lightmapper();
        void generate();

    private:
        void build_embree_scene();
        void prepare_jobs();
        void worker_main();
        void process_job(const JobPayload& job, std::mt19937& rng);
        void process_brush_face(const BrushFaceJobData& data, std::mt19937& rng);
        void process_model_vertex(const ModelVertexJobData& data, std::mt19937& rng);

        Vec3 calculate_direct_light(const Vec3& pos, const Vec3& normal, Vec3& out_dominant_dir) const;
        Vec3 calculate_indirect_light(const Vec3& origin, const Vec3& normal, std::mt19937& rng, Vec3& out_indirect_dir);
        Vec4 get_reflectivity_at_hit(unsigned int primID) const;

        void precalculate_material_reflectivity();
        bool is_in_shadow(const Vec3& start, const Vec3& end) const;
        static void apply_gaussian_blur(std::vector<float>& data, int width, int height, int channels);
        static void apply_gaussian_blur(std::vector<unsigned char>& data, int width, int height, int channels);
        static std::string sanitize_filename(std::string input);
        static Vec3 cosine_weighted_direction_in_hemisphere(const Vec3& normal, std::mt19937& gen);

        Scene* m_scene;
        int m_resolution;
        int m_bounces;
        fs::path m_output_path;

        RTCDevice m_rtc_device;
        RTCScene m_rtc_scene;
        OIDNDevice m_oidn_device;

        std::vector<JobPayload> m_jobs;
        std::atomic<size_t> m_next_job_index{ 0 };

        std::vector<std::unique_ptr<Vec4[]>> m_model_color_buffers;
        std::vector<std::unique_ptr<Vec4[]>> m_model_direction_buffers;
        std::map<const Material*, Vec4> m_material_reflectivity;
        std::vector<const BrushFace*> m_primID_to_face_map;
        std::vector<const Brush*> m_primID_to_brush_map;
    };

    Lightmapper::Lightmapper(Scene* scene, int resolution, int bounces)
        : m_scene(scene), m_resolution(resolution), m_bounces(bounces), m_rtc_device(nullptr), m_rtc_scene(nullptr)
    {
        m_rtc_device = rtcNewDevice(nullptr);
        if (!m_rtc_device)
        {
            throw std::runtime_error("Failed to create Embree device.");
        }
        rtcSetDeviceErrorFunction(m_rtc_device, embree_error_function, nullptr);
        build_embree_scene();
        m_oidn_device = oidnNewDevice(OIDN_DEVICE_TYPE_CPU);
        if (!m_oidn_device)
        {
            throw std::runtime_error("Failed to create OIDN device.");
        }
        oidnSetDeviceErrorFunction(m_oidn_device, oidn_error_function, nullptr);
        oidnCommitDevice(m_oidn_device);
    }

    Lightmapper::~Lightmapper()
    {
        if (m_rtc_scene)
        {
            rtcReleaseScene(m_rtc_scene);
        }
        if (m_rtc_device)
        {
            rtcReleaseDevice(m_rtc_device);
        }
        if (m_oidn_device)
        {
            oidnReleaseDevice(m_oidn_device);
        }
    }

    void Lightmapper::build_embree_scene()
    {
        m_rtc_scene = rtcNewScene(m_rtc_device);
        rtcSetSceneBuildQuality(m_rtc_scene, RTC_BUILD_QUALITY_HIGH);
        rtcSetSceneFlags(m_rtc_scene, RTC_SCENE_FLAG_ROBUST);

        std::vector<Vec3> all_vertices;
        std::vector<unsigned int> all_indices;
        m_primID_to_face_map.clear();
        m_primID_to_brush_map.clear();

        for (int i = 0; i < m_scene->numBrushes; ++i)
        {
            const Brush& b = m_scene->brushes[i];
            if (b.isTrigger || b.isReflectionProbe || b.isDSP) continue;

            for (int j = 0; j < b.numFaces; ++j)
            {
                const BrushFace& face = b.faces[j];
                if (face.numVertexIndices < 3) continue;

                for (int k = 0; k < face.numVertexIndices - 2; ++k)
                {
                    unsigned int base_index = static_cast<unsigned int>(all_vertices.size());
                    all_vertices.push_back(mat4_mul_vec3(&b.modelMatrix, b.vertices[face.vertexIndices[0]].pos));
                    all_vertices.push_back(mat4_mul_vec3(&b.modelMatrix, b.vertices[face.vertexIndices[k + 1]].pos));
                    all_vertices.push_back(mat4_mul_vec3(&b.modelMatrix, b.vertices[face.vertexIndices[k + 2]].pos));
                    all_indices.push_back(base_index);
                    all_indices.push_back(base_index + 1);
                    all_indices.push_back(base_index + 2);
                    m_primID_to_face_map.push_back(&face);
                    m_primID_to_brush_map.push_back(&b);
                }
            }
        }

        for (int i = 0; i < m_scene->numObjects; ++i)
        {
            const SceneObject& obj = m_scene->objects[i];
            if (!obj.model || !obj.model->combinedIndexData) continue;

            for (unsigned int j = 0; j < obj.model->totalIndexCount; j += 3)
            {
                unsigned int base_index = static_cast<unsigned int>(all_vertices.size());
                unsigned int i0 = obj.model->combinedIndexData[j];
                unsigned int i1 = obj.model->combinedIndexData[j + 1];
                unsigned int i2 = obj.model->combinedIndexData[j + 2];
                Vec3 v0 = { obj.model->combinedVertexData[i0 * 3 + 0], obj.model->combinedVertexData[i0 * 3 + 1], obj.model->combinedVertexData[i0 * 3 + 2] };
                Vec3 v1 = { obj.model->combinedVertexData[i1 * 3 + 0], obj.model->combinedVertexData[i1 * 3 + 1], obj.model->combinedVertexData[i1 * 3 + 2] };
                Vec3 v2 = { obj.model->combinedVertexData[i2 * 3 + 0], obj.model->combinedVertexData[i2 * 3 + 1], obj.model->combinedVertexData[i2 * 3 + 2] };
                all_vertices.push_back(mat4_mul_vec3(&obj.modelMatrix, v0));
                all_vertices.push_back(mat4_mul_vec3(&obj.modelMatrix, v1));
                all_vertices.push_back(mat4_mul_vec3(&obj.modelMatrix, v2));
                all_indices.push_back(base_index);
                all_indices.push_back(base_index + 1);
                all_indices.push_back(base_index + 2);
                m_primID_to_face_map.push_back(nullptr);
                m_primID_to_brush_map.push_back(nullptr);
            }
        }

        if (all_vertices.empty()) return;

        RTCGeometry geom = rtcNewGeometry(m_rtc_device, RTC_GEOMETRY_TYPE_TRIANGLE);
        Vec3* vertices_buf = (Vec3*)rtcSetNewGeometryBuffer(geom, RTC_BUFFER_TYPE_VERTEX, 0, RTC_FORMAT_FLOAT3, sizeof(Vec3), all_vertices.size());
        memcpy(vertices_buf, all_vertices.data(), all_vertices.size() * sizeof(Vec3));
        unsigned int* indices_buf = (unsigned int*)rtcSetNewGeometryBuffer(geom, RTC_BUFFER_TYPE_INDEX, 0, RTC_FORMAT_UINT3, 3 * sizeof(unsigned int), all_indices.size() / 3);
        memcpy(indices_buf, all_indices.data(), all_indices.size() * sizeof(unsigned int));

        rtcCommitGeometry(geom);
        rtcAttachGeometry(m_rtc_scene, geom);
        rtcReleaseGeometry(geom);
        rtcCommitScene(m_rtc_scene);
    }

    bool Lightmapper::is_in_shadow(const Vec3& start, const Vec3& end) const
    {
        if (!m_rtc_scene) return false;

        Vec3 ray_dir = vec3_sub(end, start);
        const float max_dist = vec3_length(ray_dir);
        if (max_dist < SHADOW_BIAS) return false;
        vec3_normalize(&ray_dir);

        RTCRayHit rayhit;
        rayhit.ray.org_x = start.x;
        rayhit.ray.org_y = start.y;
        rayhit.ray.org_z = start.z;
        rayhit.ray.dir_x = ray_dir.x;
        rayhit.ray.dir_y = ray_dir.y;
        rayhit.ray.dir_z = ray_dir.z;
        rayhit.ray.tnear = SHADOW_BIAS;
        rayhit.ray.tfar = max_dist - SHADOW_BIAS;
        rayhit.ray.time = 0.0f;
        rayhit.ray.mask = -1;
        rayhit.ray.flags = 0;
        rayhit.hit.geomID = RTC_INVALID_GEOMETRY_ID;

        RTCIntersectArguments args;
        rtcInitIntersectArguments(&args);

        float light_transmission = 1.0f;

        while (rayhit.ray.tnear < rayhit.ray.tfar)
        {
            rtcIntersect1(m_rtc_scene, &rayhit, &args);

            if (rayhit.hit.geomID == RTC_INVALID_GEOMETRY_ID)
            {
                return light_transmission < 1.0f;
            }

            Vec4 reflectivity = get_reflectivity_at_hit(rayhit.hit.primID);
            light_transmission *= (1.0f - reflectivity.w);

            if (light_transmission < 0.01f)
            {
                return true;
            }

            rayhit.ray.org_x += rayhit.ray.dir_x * rayhit.ray.tfar;
            rayhit.ray.org_y += rayhit.ray.dir_y * rayhit.ray.tfar;
            rayhit.ray.org_z += rayhit.ray.dir_z * rayhit.ray.tfar;
            rayhit.ray.tnear = SHADOW_BIAS;
            rayhit.hit.geomID = RTC_INVALID_GEOMETRY_ID;
        }

        return light_transmission < 1.0f;
    }

    std::string Lightmapper::sanitize_filename(std::string input)
    {
        std::transform(input.begin(), input.end(), input.begin(),
            [](unsigned char c) { return std::isalnum(c) || c == '_' || c == '-' ? c : '_'; });
        return input;
    }

    void Lightmapper::apply_gaussian_blur(std::vector<float>& data, int width, int height, int channels)
    {
        std::vector<float> temp_data(data.size());
        constexpr int KERNEL_SIZE = BLUR_RADIUS * 2 + 1;

        std::vector<float> kernel(KERNEL_SIZE);
        float sigma = static_cast<float>(BLUR_RADIUS) / 2.0f;
        float sum = 0.0f;
        for (int i = 0; i < KERNEL_SIZE; ++i)
        {
            int x = i - BLUR_RADIUS;
            kernel[i] = expf(-static_cast<float>(x * x) / (2.0f * sigma * sigma));
            sum += kernel[i];
        }
        for (float& val : kernel)
        {
            val /= sum;
        }

        std::vector<float> totals(channels);
        for (int y = 0; y < height; ++y)
        {
            for (int x = 0; x < width; ++x)
            {
                std::fill(totals.begin(), totals.end(), 0.0f);
                for (int k = -BLUR_RADIUS; k <= BLUR_RADIUS; ++k)
                {
                    int sample_x = std::clamp(x + k, 0, width - 1);
                    int src_idx = (y * width + sample_x) * channels;
                    float weight = kernel[k + BLUR_RADIUS];
                    for (int c = 0; c < channels; ++c)
                    {
                        totals[c] += data[src_idx + c] * weight;
                    }
                }
                int dst_idx = (y * width + x) * channels;
                for (int c = 0; c < channels; ++c)
                {
                    temp_data[dst_idx + c] = totals[c];
                }
            }
        }

        for (int y = 0; y < height; ++y)
        {
            for (int x = 0; x < width; ++x)
            {
                std::fill(totals.begin(), totals.end(), 0.0f);
                for (int k = -BLUR_RADIUS; k <= BLUR_RADIUS; ++k)
                {
                    int sample_y = std::clamp(y + k, 0, height - 1);
                    int src_idx = (sample_y * width + x) * channels;
                    float weight = kernel[k + BLUR_RADIUS];
                    for (int c = 0; c < channels; ++c)
                    {
                        totals[c] += temp_data[src_idx + c] * weight;
                    }
                }
                int dst_idx = (y * width + x) * channels;
                for (int c = 0; c < channels; ++c)
                {
                    data[dst_idx + c] = totals[c];
                }
            }
        }
    }

    void Lightmapper::apply_gaussian_blur(std::vector<unsigned char>& data, int width, int height, int channels)
    {
        std::vector<unsigned char> temp_data(data.size());
        constexpr int KERNEL_SIZE = BLUR_RADIUS * 2 + 1;

        std::vector<float> kernel(KERNEL_SIZE);
        float sigma = static_cast<float>(BLUR_RADIUS) / 2.0f;
        float sum = 0.0f;
        for (int i = 0; i < KERNEL_SIZE; ++i)
        {
            int x = i - BLUR_RADIUS;
            kernel[i] = expf(-static_cast<float>(x * x) / (2.0f * sigma * sigma));
            sum += kernel[i];
        }
        for (float& val : kernel)
        {
            val /= sum;
        }

        std::vector<float> totals(channels);
        for (int y = 0; y < height; ++y)
        {
            for (int x = 0; x < width; ++x)
            {
                std::fill(totals.begin(), totals.end(), 0.0f);
                for (int k = -BLUR_RADIUS; k <= BLUR_RADIUS; ++k)
                {
                    int sample_x = std::clamp(x + k, 0, width - 1);
                    int src_idx = (y * width + sample_x) * channels;
                    float weight = kernel[k + BLUR_RADIUS];
                    for (int c = 0; c < channels; ++c)
                    {
                        totals[c] += data[src_idx + c] * weight;
                    }
                }
                int dst_idx = (y * width + x) * channels;
                for (int c = 0; c < channels; ++c)
                {
                    temp_data[dst_idx + c] = static_cast<unsigned char>(std::min(255.0f, totals[c]));
                }
            }
        }

        for (int y = 0; y < height; ++y)
        {
            for (int x = 0; x < width; ++x)
            {
                std::fill(totals.begin(), totals.end(), 0.0f);
                for (int k = -BLUR_RADIUS; k <= BLUR_RADIUS; ++k)
                {
                    int sample_y = std::clamp(y + k, 0, height - 1);
                    int src_idx = (sample_y * width + x) * channels;
                    float weight = kernel[k + BLUR_RADIUS];
                    for (int c = 0; c < channels; ++c)
                    {
                        totals[c] += temp_data[src_idx + c] * weight;
                    }
                }
                int dst_idx = (y * width + x) * channels;
                for (int c = 0; c < channels; ++c)
                {
                    data[dst_idx + c] = static_cast<unsigned char>(std::min(255.0f, totals[c]));
                }
            }
        }
    }

    void build_adjacency_list(const SceneObject& obj, std::vector<std::set<unsigned int>>& adjacency)
    {
        unsigned int vertex_count = obj.model->totalVertexCount;
        if (vertex_count == 0) return;

        adjacency.assign(vertex_count, std::set<unsigned int>());
        const unsigned int* indices = obj.model->combinedIndexData;
        unsigned int index_count = obj.model->totalIndexCount;

        for (unsigned int i = 0; i < index_count; i += 3)
        {
            unsigned int i0 = indices[i];
            unsigned int i1 = indices[i + 1];
            unsigned int i2 = indices[i + 2];

            adjacency[i0].insert(i1);
            adjacency[i0].insert(i2);

            adjacency[i1].insert(i0);
            adjacency[i1].insert(i2);

            adjacency[i2].insert(i0);
            adjacency[i2].insert(i1);
        }
    }

    void filter_vertex_data(Vec4* output_buffer, const Vec4* input_buffer,
        const std::vector<std::set<unsigned int>>& adjacency, unsigned int vertex_count)
    {
        for (unsigned int i = 0; i < vertex_count; ++i)
        {
            Vec4 accumulated_value = input_buffer[i];
            int neighbor_count = 1;

            for (unsigned int neighbor_index : adjacency[i])
            {
                accumulated_value = vec4_add(accumulated_value, input_buffer[neighbor_index]);
                neighbor_count++;
            }

            output_buffer[i] = vec4_muls(accumulated_value, 1.0f / neighbor_count);
        }
    }

    void apply_bilateral_filter_pass(Vec4* out_buf, const Vec4* in_buf,
        const std::vector<std::set<unsigned int>>& adjacency,
        const SceneObject& obj, float normal_similarity_threshold)
    {
        unsigned int vertex_count = obj.model->totalVertexCount;
        const float* normals = obj.model->combinedNormalData;

        for (unsigned int i = 0; i < vertex_count; ++i)
        {
            Vec4 total_value = { 0,0,0,0 };
            float total_weight = 0.0f;

            Vec3 current_normal = { normals[i * 3], normals[i * 3 + 1], normals[i * 3 + 2] };

            total_value = vec4_add(total_value, in_buf[i]);
            total_weight += 1.0f;

            for (unsigned int neighbor_idx : adjacency[i])
            {
                Vec3 neighbor_normal = { normals[neighbor_idx * 3], normals[neighbor_idx * 3 + 1], normals[neighbor_idx * 3 + 2] };

                float dot = vec3_dot(current_normal, neighbor_normal);
                if (dot > normal_similarity_threshold)
                {
                    total_value = vec4_add(total_value, in_buf[neighbor_idx]);
                    total_weight += 1.0f;
                }
            }

            if (total_weight > 0)
            {
                out_buf[i] = vec4_muls(total_value, 1.0f / total_weight);
            }
            else
            {
                out_buf[i] = in_buf[i];
            }
        }
    }

    void box_blur(std::vector<float>& out, const std::vector<float>& in, int width, int height, int radius)
    {
        std::vector<float> temp(width * height);
        float scale = 1.0f / (2 * radius + 1);

        for (int y = 0; y < height; ++y) {
            float sum = 0;
            for (int x = -radius; x <= radius; ++x) {
                sum += in[y * width + std::clamp(x, 0, width - 1)];
            }
            for (int x = 0; x < width; ++x) {
                temp[y * width + x] = sum * scale;
                int prev_idx = std::clamp(x - radius, 0, width - 1);
                int next_idx = std::clamp(x + radius + 1, 0, width - 1);
                sum += in[y * width + next_idx] - in[y * width + prev_idx];
            }
        }

        for (int x = 0; x < width; ++x) {
            float sum = 0;
            for (int y = -radius; y <= radius; ++y) {
                sum += temp[std::clamp(y, 0, height - 1) * width + x];
            }
            for (int y = 0; y < height; ++y) {
                out[y * width + x] = sum * scale;
                int prev_idx = std::clamp(y - radius, 0, height - 1);
                int next_idx = std::clamp(y + radius + 1, 0, height - 1);
                sum += temp[next_idx * width + x] - temp[prev_idx * width + x];
            }
        }
    }

    void apply_guided_filter(std::vector<float>& out_p, const std::vector<float>& in_p, const std::vector<float>& guide, int width, int height, int radius, float epsilon)
    {
        int n_pixels = width * height;
        std::vector<float> guide_gray(n_pixels);
        for (int i = 0; i < n_pixels; ++i) {
            guide_gray[i] = (guide[i * 3 + 0] * 0.299f + guide[i * 3 + 1] * 0.587f + guide[i * 3 + 2] * 0.114f);
        }

        std::vector<float> mean_I(n_pixels);
        box_blur(mean_I, guide_gray, width, height, radius);

        std::vector<float> var_I(n_pixels);
        std::vector<float> I_sq(n_pixels);
        for (int i = 0; i < n_pixels; ++i) {
            I_sq[i] = guide_gray[i] * guide_gray[i];
        }
        box_blur(var_I, I_sq, width, height, radius);
        for (int i = 0; i < n_pixels; ++i) {
            var_I[i] -= mean_I[i] * mean_I[i];
        }

        out_p.resize(n_pixels * 3);
        std::vector<float> p_channel(n_pixels);
        std::vector<float> mean_p(n_pixels);
        std::vector<float> cov_Ip(n_pixels);
        std::vector<float> Ip(n_pixels);
        std::vector<float> a(n_pixels), b(n_pixels);
        std::vector<float> mean_a(n_pixels), mean_b(n_pixels);

        for (int c = 0; c < 3; ++c) {
            for (int i = 0; i < n_pixels; ++i) {
                p_channel[i] = in_p[i * 3 + c];
            }

            box_blur(mean_p, p_channel, width, height, radius);

            for (int i = 0; i < n_pixels; ++i) {
                Ip[i] = guide_gray[i] * p_channel[i];
            }
            box_blur(cov_Ip, Ip, width, height, radius);
            for (int i = 0; i < n_pixels; ++i) {
                cov_Ip[i] -= mean_I[i] * mean_p[i];
            }

            for (int i = 0; i < n_pixels; ++i) {
                a[i] = cov_Ip[i] / (var_I[i] + epsilon);
                b[i] = mean_p[i] - a[i] * mean_I[i];
            }

            box_blur(mean_a, a, width, height, radius);
            box_blur(mean_b, b, width, height, radius);

            for (int i = 0; i < n_pixels; ++i) {
                out_p[i * 3 + c] = mean_a[i] * guide_gray[i] + mean_b[i];
            }
        }
    }

    void Lightmapper::process_brush_face(const BrushFaceJobData& data, std::mt19937& rng)
    {
        const Brush& b = m_scene->brushes[data.brush_index];
        const BrushFace& face = b.faces[data.face_index];
        if (face.numVertexIndices < 3) return;

        Vec3 v0 = mat4_mul_vec3(&b.modelMatrix, b.vertices[face.vertexIndices[0]].pos);
        Vec3 v1 = mat4_mul_vec3(&b.modelMatrix, b.vertices[face.vertexIndices[1]].pos);
        Vec3 v2 = mat4_mul_vec3(&b.modelMatrix, b.vertices[face.vertexIndices[2]].pos);
        Vec3 face_normal = vec3_cross(vec3_sub(v1, v0), vec3_sub(v2, v0));
        vec3_normalize(&face_normal);

        Vec3 u_axis, v_axis;
        if (fabsf(face_normal.x) > fabsf(face_normal.y)) u_axis = { -face_normal.z, 0, face_normal.x };
        else u_axis = { 0, face_normal.z, -face_normal.y };
        vec3_normalize(&u_axis);
        v_axis = vec3_cross(face_normal, u_axis);

        float min_u = FLT_MAX, max_u = -FLT_MAX;
        float min_v = FLT_MAX, max_v = -FLT_MAX;
        std::vector<Vec3> world_verts(face.numVertexIndices);
        for (int k = 0; k < face.numVertexIndices; ++k)
        {
            world_verts[k] = mat4_mul_vec3(&b.modelMatrix, b.vertices[face.vertexIndices[k]].pos);
            float u = vec3_dot(world_verts[k], u_axis);
            float v = vec3_dot(world_verts[k], v_axis);
            min_u = std::min(min_u, u); max_u = std::max(max_u, u);
            min_v = std::min(min_v, v); max_v = std::max(max_v, v);
        }

        float u_range = std::max(0.001f, max_u - min_u);
        float v_range = std::max(0.001f, max_v - min_v);

        int lightmap_width = std::clamp(static_cast<int>(ceilf(u_range * LUXELS_PER_UNIT)), 4, m_resolution);
        int lightmap_height = std::clamp(static_cast<int>(ceilf(v_range * LUXELS_PER_UNIT)), 4, m_resolution);

        std::vector<float> direct_lightmap_data(lightmap_width * lightmap_height * 3);
        std::vector<float> indirect_lightmap_data(lightmap_width * lightmap_height * 3);
        std::vector<float> albedo_lightmap_data(lightmap_width * lightmap_height * 3);
        std::vector<float> normal_lightmap_data(lightmap_width * lightmap_height * 3);
        std::vector<float> direction_float_data(lightmap_width * lightmap_height * 3);
        std::vector<unsigned char> dir_lightmap_data(lightmap_width * lightmap_height * 4, 0);

        Vec4 face_reflectivity_v4 = { 0.5f, 0.5f, 0.5f, 1.0f };
        if (face.material) {
            auto it = m_material_reflectivity.find(face.material);
            if (it != m_material_reflectivity.end()) {
                face_reflectivity_v4 = it->second;
            }
        }

        for (int y = 0; y < lightmap_height; ++y)
        {
            for (int x = 0; x < lightmap_width; ++x)
            {
                Vec3 direct_light_color = { 0, 0, 0 };
                Vec3 indirect_light_color = { 0, 0, 0 };
                Vec3 accumulated_direction = { 0, 0, 0 };
                Vec3 indirect_direction = { 0, 0, 0 };

                float u_tex = (static_cast<float>(x) + 0.5f) / lightmap_width;
                float v_tex = (static_cast<float>(y) + 0.5f) / lightmap_height;
                float world_u = min_u + u_tex * u_range;
                float world_v = min_v + v_tex * v_range;
                Vec3 point_on_plane = vec3_add(vec3_muls(u_axis, world_u), vec3_muls(v_axis, world_v));

                Vec3 world_pos;
                bool inside = false;
                for (int k = 0; k < face.numVertexIndices - 2; ++k)
                {
                    Vec3 p0 = world_verts[0], p1 = world_verts[k + 1], p2 = world_verts[k + 2];
                    Vec3 v_p0p1 = vec3_sub(p1, p0), v_p0p2 = vec3_sub(p2, p0), v_p0pt = vec3_sub(point_on_plane, p0);
                    float dot00 = vec3_dot(v_p0p1, v_p0p1), dot01 = vec3_dot(v_p0p1, v_p0p2), dot02 = vec3_dot(v_p0p1, v_p0pt);
                    float dot11 = vec3_dot(v_p0p2, v_p0p2), dot12 = vec3_dot(v_p0p2, v_p0pt);
                    float inv_denom = 1.0f / (dot00 * dot11 - dot01 * dot01);
                    float bary_u = (dot11 * dot02 - dot01 * dot12) * inv_denom;
                    float bary_v = (dot00 * dot12 - dot01 * dot02) * inv_denom;
                    if (bary_u >= 0 && bary_v >= 0 && (bary_u + bary_v < 1))
                    {
                        inside = true;
                        world_pos = vec3_add(p0, vec3_add(vec3_muls(v_p0p1, bary_u), vec3_muls(v_p0p2, bary_v)));
                        break;
                    }
                }

                int hdr_idx = (y * lightmap_width + x) * 3;

                if (inside)
                {
                    direct_light_color = calculate_direct_light(world_pos, face_normal, accumulated_direction);
                    indirect_light_color = calculate_indirect_light(world_pos, face_normal, rng, indirect_direction);
                    accumulated_direction = vec3_add(accumulated_direction, indirect_direction);
                    normal_lightmap_data[hdr_idx + 0] = face_normal.x;
                    normal_lightmap_data[hdr_idx + 1] = face_normal.y;
                    normal_lightmap_data[hdr_idx + 2] = face_normal.z;
                }
                else
                {
                    normal_lightmap_data[hdr_idx + 0] = 0.0f;
                    normal_lightmap_data[hdr_idx + 1] = 0.0f;
                    normal_lightmap_data[hdr_idx + 2] = 0.0f;
                }

                if (vec3_length_sq(accumulated_direction) > 0.0001f) vec3_normalize(&accumulated_direction);
                else accumulated_direction = { 0,0,0 };

                direction_float_data[hdr_idx + 0] = accumulated_direction.x;
                direction_float_data[hdr_idx + 1] = accumulated_direction.y;
                direction_float_data[hdr_idx + 2] = accumulated_direction.z;

                direct_lightmap_data[hdr_idx + 0] = direct_light_color.x;
                direct_lightmap_data[hdr_idx + 1] = direct_light_color.y;
                direct_lightmap_data[hdr_idx + 2] = direct_light_color.z;

                indirect_lightmap_data[hdr_idx + 0] = indirect_light_color.x;
                indirect_lightmap_data[hdr_idx + 1] = indirect_light_color.y;
                indirect_lightmap_data[hdr_idx + 2] = indirect_light_color.z;

                albedo_lightmap_data[hdr_idx + 0] = face_reflectivity_v4.x;
                albedo_lightmap_data[hdr_idx + 1] = face_reflectivity_v4.y;
                albedo_lightmap_data[hdr_idx + 2] = face_reflectivity_v4.z;
            }
        }

        std::vector<float> denoised_indirect_data(lightmap_width * lightmap_height * 3);
        constexpr float BLACK_THRESHOLD = 0.0001f;
        float indirect_sum = 0.0f;
        for (float val : indirect_lightmap_data) {
            indirect_sum += val;
        }

        if (indirect_sum > BLACK_THRESHOLD)
        {
            size_t pixelStride = sizeof(float) * 3;
            size_t rowStride = pixelStride * lightmap_width;

            OIDNFilter filter = oidnNewFilter(m_oidn_device, "RTLightmap");

            oidnSetSharedFilterImage(filter, "color", indirect_lightmap_data.data(), OIDN_FORMAT_FLOAT3, lightmap_width, lightmap_height, 0, pixelStride, rowStride);
            oidnSetSharedFilterImage(filter, "albedo", albedo_lightmap_data.data(), OIDN_FORMAT_FLOAT3, lightmap_width, lightmap_height, 0, pixelStride, rowStride);
            oidnSetSharedFilterImage(filter, "normal", normal_lightmap_data.data(), OIDN_FORMAT_FLOAT3, lightmap_width, lightmap_height, 0, pixelStride, rowStride);
            oidnSetSharedFilterImage(filter, "output", denoised_indirect_data.data(), OIDN_FORMAT_FLOAT3, lightmap_width, lightmap_height, 0, pixelStride, rowStride);

            oidnSetFilterBool(filter, "hdr", true);
            oidnSetFilterBool(filter, "cleanAux", true);
            oidnCommitFilter(filter);
            oidnExecuteFilter(filter);

            const char* errorMessage;
            if (oidnGetDeviceError(m_oidn_device, &errorMessage) != OIDN_ERROR_NONE)
                Console_Printf_Error("[OIDN] Filter execution error: %s", errorMessage);

            oidnReleaseFilter(filter);
        }
        else
        {
            std::fill(denoised_indirect_data.begin(), denoised_indirect_data.end(), 0.0f);
        }

        std::vector<float> final_hdr_lightmap_data(lightmap_width * lightmap_height * 3);
        for (size_t i = 0; i < final_hdr_lightmap_data.size(); ++i) {
            final_hdr_lightmap_data[i] = direct_lightmap_data[i] + denoised_indirect_data[i];
        }

        std::vector<float> filtered_direction_data;
        apply_guided_filter(filtered_direction_data, direction_float_data, final_hdr_lightmap_data, lightmap_width, lightmap_height, 4, 0.01f);

        for (int i = 0; i < lightmap_width * lightmap_height; ++i) {
            int idx3 = i * 3;
            Vec3 dir = { filtered_direction_data[idx3], filtered_direction_data[idx3 + 1], filtered_direction_data[idx3 + 2] };
            if (vec3_length_sq(dir) > 0.0001f) {
                vec3_normalize(&dir);
            }
            else {
                dir = { 0,0,0 };
            }

            int idx4 = i * 4;
            dir_lightmap_data[idx4 + 0] = static_cast<unsigned char>((dir.x * 0.5f + 0.5f) * 255.0f);
            dir_lightmap_data[idx4 + 1] = static_cast<unsigned char>((dir.y * 0.5f + 0.5f) * 255.0f);
            dir_lightmap_data[idx4 + 2] = static_cast<unsigned char>((dir.z * 0.5f + 0.5f) * 255.0f);
            dir_lightmap_data[idx4 + 3] = 255;
        }

        apply_gaussian_blur(final_hdr_lightmap_data, lightmap_width, lightmap_height, 3);

        const int padding = 1;
        int padded_width = lightmap_width + padding * 2;
        int padded_height = lightmap_height + padding * 2;

        std::vector<float> padded_hdr_data(padded_width * padded_height * 3);
        for (int y = 0; y < lightmap_height; ++y) {
            for (int x = 0; x < lightmap_width; ++x) {
                for (int c = 0; c < 3; ++c) {
                    padded_hdr_data[((y + padding) * padded_width + (x + padding)) * 3 + c] = final_hdr_lightmap_data[(y * lightmap_width + x) * 3 + c];
                }
            }
        }
        for (int y = 0; y < padded_height; ++y) {
            for (int x = 0; x < padded_width; ++x) {
                int clamp_x = std::clamp(x, padding, padded_width - 1 - padding);
                int clamp_y = std::clamp(y, padding, padded_height - 1 - padding);
                if (x != clamp_x || y != clamp_y) {
                    for (int c = 0; c < 3; ++c) {
                        padded_hdr_data[(y * padded_width + x) * 3 + c] = padded_hdr_data[((clamp_y)*padded_width + (clamp_x)) * 3 + c];
                    }
                }
            }
        }

        std::vector<unsigned char> padded_dir_data(padded_width * padded_height * 4);
        for (int y = 0; y < lightmap_height; ++y) {
            for (int x = 0; x < lightmap_width; ++x) {
                for (int c = 0; c < 4; ++c) {
                    padded_dir_data[((y + padding) * padded_width + (x + padding)) * 4 + c] = dir_lightmap_data[(y * lightmap_width + x) * 4 + c];
                }
            }
        }
        for (int y = 0; y < padded_height; ++y) {
            for (int x = 0; x < padded_width; ++x) {
                int clamp_x = std::clamp(x, padding, padded_width - 1 - padding);
                int clamp_y = std::clamp(y, padding, padded_height - 1 - padding);
                if (x != clamp_x || y != clamp_y) {
                    for (int c = 0; c < 4; ++c) {
                        padded_dir_data[(y * padded_width + x) * 4 + c] = padded_dir_data[((clamp_y)*padded_width + (clamp_x)) * 4 + c];
                    }
                }
            }
        }

        fs::path color_path = data.output_dir / ("face_" + std::to_string(data.face_index) + "_color.hdr");
        stbi_write_hdr(color_path.string().c_str(), padded_width, padded_height, 3, padded_hdr_data.data());
        fs::path dir_path = data.output_dir / ("face_" + std::to_string(data.face_index) + "_dir.png");
        SDL_Surface* dir_surface = SDL_CreateRGBSurfaceFrom(padded_dir_data.data(), padded_width, padded_height, 32, padded_width * 4,
            0x000000ff, 0x0000ff00, 0x00ff0000, 0xff000000);
        if (dir_surface) {
            IMG_SavePNG(dir_surface, dir_path.string().c_str());
            SDL_FreeSurface(dir_surface);
        }
    }

    void Lightmapper::process_model_vertex(const ModelVertexJobData& data, std::mt19937& rng)
    {
        const SceneObject& obj = m_scene->objects[data.model_index];
        unsigned int v_idx = data.vertex_index;

        Vec3 local_pos = { obj.model->combinedVertexData[v_idx * 3 + 0], obj.model->combinedVertexData[v_idx * 3 + 1], obj.model->combinedVertexData[v_idx * 3 + 2] };
        Vec3 local_normal = { obj.model->combinedNormalData[v_idx * 3 + 0], obj.model->combinedNormalData[v_idx * 3 + 1], obj.model->combinedNormalData[v_idx * 3 + 2] };

        Vec3 world_pos = mat4_mul_vec3(&obj.modelMatrix, local_pos);
        Vec3 world_normal = mat4_mul_vec3_dir(&obj.modelMatrix, local_normal);
        vec3_normalize(&world_normal);

        Vec3 direction_accumulator = { 0,0,0 };
        Vec3 indirect_dir = { 0,0,0 };
        Vec3 direct_light = calculate_direct_light(world_pos, world_normal, direction_accumulator);
        Vec3 indirect_light = calculate_indirect_light(world_pos, world_normal, rng, indirect_dir);

        Vec3 final_light_color = vec3_add(direct_light, indirect_light);
        direction_accumulator = vec3_add(direction_accumulator, indirect_dir);
        data.output_color_buffer[v_idx] = { final_light_color.x, final_light_color.y, final_light_color.z, 1.0f };

        if (vec3_length_sq(direction_accumulator) > 0.0001f) vec3_normalize(&direction_accumulator);
        else direction_accumulator = { 0,0,0 };
        data.output_direction_buffer[v_idx] = { direction_accumulator.x, direction_accumulator.y, direction_accumulator.z, 1.0f };
    }

    void Lightmapper::process_job(const JobPayload& job, std::mt19937& rng)
    {
        std::visit([this, &rng](auto&& arg) {
            using T = std::decay_t<decltype(arg)>;
            if constexpr (std::is_same_v<T, BrushFaceJobData>)
                process_brush_face(arg, rng);
            else if constexpr (std::is_same_v<T, ModelVertexJobData>)
                process_model_vertex(arg, rng);
            }, job);
    }

    void Lightmapper::worker_main()
    {
        std::random_device rd;
        std::mt19937 rng(rd() ^ std::hash<std::thread::id>{}(std::this_thread::get_id()));

        while (true)
        {
            size_t job_index = m_next_job_index.fetch_add(1);
            if (job_index >= m_jobs.size())
            {
                break;
            }
            process_job(m_jobs[job_index], rng);
        }
    }

    void Lightmapper::prepare_jobs()
    {
        fs::path map_path(m_scene->mapPath);
        m_output_path = fs::path("lightmaps") / map_path.stem();
        fs::create_directories(m_output_path);

        size_t total_brush_faces = 0;
        for (int i = 0; i < m_scene->numBrushes; ++i)
        {
            const Brush& b = m_scene->brushes[i];
            if (!b.isTrigger && !b.isWater && !b.isReflectionProbe && !b.isGlass && !b.isDSP)
            {
                total_brush_faces += b.numFaces;
            }
        }

        size_t total_model_vertices = 0;
        for (int i = 0; i < m_scene->numObjects; ++i)
        {
            if (m_scene->objects[i].model)
            {
                total_model_vertices += m_scene->objects[i].model->totalVertexCount;
            }
        }

        if (total_brush_faces + total_model_vertices == 0)
        {
            Console_Printf("[Lightmapper] No bakeable geometry found.");
            return;
        }

        m_jobs.reserve(total_brush_faces + total_model_vertices);
        m_model_color_buffers.resize(m_scene->numObjects);
        m_model_direction_buffers.resize(m_scene->numObjects);

        for (int i = 0; i < m_scene->numObjects; ++i)
        {
            if (m_scene->objects[i].model)
            {
                m_model_color_buffers[i] = std::make_unique<Vec4[]>(m_scene->objects[i].model->totalVertexCount);
                m_model_direction_buffers[i] = std::make_unique<Vec4[]>(m_scene->objects[i].model->totalVertexCount);
            }
        }

        for (int i = 0; i < m_scene->numBrushes; ++i)
        {
            const Brush& b = m_scene->brushes[i];
            if (b.isTrigger || b.isWater || b.isReflectionProbe || b.isGlass || b.isDSP) continue;
            std::string brush_name_str = (strlen(b.targetname) > 0) ? b.targetname : "Brush_" + std::to_string(i);
            fs::path brush_dir = m_output_path / sanitize_filename(brush_name_str);
            fs::create_directories(brush_dir);
            for (int j = 0; j < b.numFaces; ++j)
            {
                m_jobs.emplace_back(BrushFaceJobData{ i, j, brush_dir });
            }
        }

        for (int i = 0; i < m_scene->numObjects; ++i)
        {
            const SceneObject& obj = m_scene->objects[i];
            if (obj.model)
            {
                for (unsigned int v = 0; v < obj.model->totalVertexCount; ++v)
                {
                    m_jobs.emplace_back(ModelVertexJobData{ i, v, m_model_color_buffers[i].get(), m_model_direction_buffers[i].get() });
                }
            }
        }
        Console_Printf("[Lightmapper] Baking %zu faces and %zu vertices.", total_brush_faces, total_model_vertices);
    }

    void Lightmapper::precalculate_material_reflectivity()
    {
        Console_Printf("[Lightmapper] Pre-calculating material reflectivity...");

        std::set<const Material*> unique_materials;
        for (int i = 0; i < m_scene->numBrushes; ++i) {
            const Brush& b = m_scene->brushes[i];
            for (int j = 0; j < b.numFaces; ++j) {
                const BrushFace& face = b.faces[j];
                if (face.material && face.material != &g_NodrawMaterial) {
                    unique_materials.insert(face.material);
                }
            }
        }

        Console_Printf("[Lightmapper] Found %zu unique materials to analyze.", unique_materials.size());

        for (const Material* mat : unique_materials) {
            if (!mat || mat->diffusePath[0] == '\0') {
                m_material_reflectivity[mat] = { 0.5f, 0.5f, 0.5f, 1.0f };
                continue;
            }

            std::string full_path = "textures/" + std::string(mat->diffusePath);
            SDL_Surface* loaded_surface = IMG_Load(full_path.c_str());
            if (!loaded_surface) {
                Console_Printf_Warning("[Lightmapper] Could not load texture for reflectivity: %s", full_path.c_str());
                m_material_reflectivity[mat] = { 0.5f, 0.5f, 0.5f, 1.0f };
                continue;
            }

            SDL_Surface* surface = SDL_ConvertSurfaceFormat(loaded_surface, SDL_PIXELFORMAT_RGBA32, 0);
            SDL_FreeSurface(loaded_surface);

            if (!surface) {
                Console_Printf_Error("[Lightmapper] Could not convert surface for: %s", full_path.c_str());
                m_material_reflectivity[mat] = { 0.5f, 0.5f, 0.5f, 1.0f };
                continue;
            }

            long long total_r = 0, total_g = 0, total_b = 0, total_a = 0;
            int width = surface->w;
            int height = surface->h;
            int texels = width * height;

            Uint8* pixels = (Uint8*)surface->pixels;
            for (int i = 0; i < texels; ++i) {
                total_r += pixels[i * 4 + 0];
                total_g += pixels[i * 4 + 1];
                total_b += pixels[i * 4 + 2];
                total_a += pixels[i * 4 + 3];
            }

            SDL_FreeSurface(surface);

            Vec4 reflectivity = {
                static_cast<float>(total_r) / texels / 255.0f,
                static_cast<float>(total_g) / texels / 255.0f,
                static_cast<float>(total_b) / texels / 255.0f,
                static_cast<float>(total_a) / texels / 255.0f
            };

            m_material_reflectivity[mat] = reflectivity;
        }
        Console_Printf("[Lightmapper] Reflectivity calculation complete.");
    }

    Vec3 Lightmapper::calculate_direct_light(const Vec3& pos, const Vec3& normal, Vec3& out_dominant_dir) const
    {
        Vec3 direct_light = { 0,0,0 };
        out_dominant_dir = { 0,0,0 };
        Vec3 point_to_light_check = vec3_add(pos, vec3_muls(normal, SHADOW_BIAS));

        for (int k = 0; k < m_scene->numActiveLights; ++k)
        {
            const Light& light = m_scene->lights[k];
            if (!light.is_static) continue;

            Vec3 light_dir = vec3_sub(light.position, pos);
            float dist = vec3_length(light_dir);
            vec3_normalize(&light_dir);
            if (dist > light.radius) continue;

            float NdotL = std::max(0.0f, vec3_dot(normal, light_dir));
            if (NdotL <= 0.0f) continue;

            if (is_in_shadow(point_to_light_check, light.position)) continue;

            float spotFactor = 1.0f;
            if (light.type == LIGHT_SPOT)
            {
                Vec3 light_forward_vector = vec3_muls(light.direction, -1.0f);
                float theta = vec3_dot(light_dir, light_forward_vector);
                float inner_cone_cos = light.cutOff;
                float outer_cone_cos = light.outerCutOff;

                if (theta < outer_cone_cos) {
                    spotFactor = 0.0f;
                }
                else {
                    float delta = inner_cone_cos - outer_cone_cos;
                    if (delta > 0.0001f) {
                        float t = std::clamp((theta - outer_cone_cos) / delta, 0.0f, 1.0f);
                        spotFactor = t * t * (3.0f - 2.0f * t);
                    }
                    else {
                        spotFactor = (theta >= inner_cone_cos) ? 1.0f : 0.0f;
                    }
                }
            }

            float attenuation = powf(std::max(0.0f, 1.0f - dist / light.radius), 2.0f) * spotFactor;
            Vec3 light_color = vec3_muls(light.color, light.intensity);
            Vec3 light_contribution = vec3_muls(light_color, NdotL * attenuation);
            direct_light = vec3_add(direct_light, light_contribution);

            float contribution_magnitude = vec3_length(light_contribution);
            out_dominant_dir = vec3_add(out_dominant_dir, vec3_muls(light_dir, contribution_magnitude));
        }
        return direct_light;
    }

    Vec3 Lightmapper::cosine_weighted_direction_in_hemisphere(const Vec3& normal, std::mt19937& gen)
    {
        std::uniform_real_distribution<float> dist(0.0f, 1.0f);
        float u1 = dist(gen);
        float u2 = dist(gen);

        float r = sqrtf(u1);
        float theta = 2.0f * (float)M_PI * u2;

        float x = r * cosf(theta);
        float y = r * sinf(theta);
        float z = sqrtf(std::max(0.0f, 1.0f - u1));

        Vec3 up = fabsf(normal.z) < 0.999f ? Vec3{ 0,0,1 } : Vec3{ 1,0,0 };
        Vec3 tangent = vec3_cross(up, normal);
        vec3_normalize(&tangent);
        Vec3 bitangent = vec3_cross(normal, tangent);

        return vec3_add(vec3_muls(tangent, x), vec3_add(vec3_muls(bitangent, y), vec3_muls(normal, z)));
    }

    Vec4 Lightmapper::get_reflectivity_at_hit(unsigned int primID) const
    {
        if (primID < m_primID_to_face_map.size())
        {
            const BrushFace* hit_face = m_primID_to_face_map[primID];
            if (hit_face && hit_face->material)
            {
                auto it = m_material_reflectivity.find(hit_face->material);
                if (it != m_material_reflectivity.end())
                {
                    return it->second;
                }
            }
        }

        return { 0.5f, 0.5f, 0.5f, 1.0f };
    }

    Vec3 Lightmapper::calculate_indirect_light(const Vec3& origin, const Vec3& normal, std::mt19937& rng, Vec3& out_indirect_dir)
    {
        Vec3 accumulated_color = { 0, 0, 0 };
        out_indirect_dir = { 0, 0, 0 };
        std::uniform_real_distribution<float> roulette_dist(0.0f, 1.0f);

        if (m_bounces <= 0 || INDIRECT_SAMPLES_PER_POINT <= 0)
            return accumulated_color;

        for (int i = 0; i < INDIRECT_SAMPLES_PER_POINT; ++i)
        {
            Vec3 ray_origin = origin;
            Vec3 ray_normal = normal;
            Vec3 throughput = { 1.0f, 1.0f, 1.0f };
            Vec3 first_bounce_dir = { 0, 0, 0 };
            Vec3 path_radiance = { 0, 0, 0 };

            for (int bounce = 0; bounce < m_bounces; ++bounce)
            {
                Vec3 bounce_dir = cosine_weighted_direction_in_hemisphere(ray_normal, rng);
                if (bounce == 0)
                    first_bounce_dir = bounce_dir;

                Vec3 trace_origin = vec3_add(ray_origin, vec3_muls(ray_normal, SHADOW_BIAS));

                RTCRayHit rayhit;
                rayhit.ray.org_x = trace_origin.x;
                rayhit.ray.org_y = trace_origin.y;
                rayhit.ray.org_z = trace_origin.z;
                rayhit.ray.dir_x = bounce_dir.x;
                rayhit.ray.dir_y = bounce_dir.y;
                rayhit.ray.dir_z = bounce_dir.z;
                rayhit.ray.tnear = 0.0f;
                rayhit.ray.tfar = FLT_MAX;
                rayhit.ray.mask = -1;
                rayhit.ray.flags = 0;
                rayhit.hit.geomID = RTC_INVALID_GEOMETRY_ID;

                RTCIntersectArguments args;
                rtcInitIntersectArguments(&args);
                args.feature_mask = RTC_FEATURE_FLAG_NONE;

                rtcIntersect1(m_rtc_scene, &rayhit, &args);

                if (rayhit.hit.geomID == RTC_INVALID_GEOMETRY_ID)
                    break;

                Vec3 hit_pos = {
                    trace_origin.x + rayhit.ray.tfar * bounce_dir.x,
                    trace_origin.y + rayhit.ray.tfar * bounce_dir.y,
                    trace_origin.z + rayhit.ray.tfar * bounce_dir.z
                };

                Vec3 hit_normal = { rayhit.hit.Ng_x, rayhit.hit.Ng_y, rayhit.hit.Ng_z };
                vec3_normalize(&hit_normal);

                if (vec3_dot(bounce_dir, hit_normal) > 0.0f)
                    break;

                if (rayhit.hit.primID < m_primID_to_brush_map.size())
                {
                    const Brush* hit_brush = m_primID_to_brush_map[rayhit.hit.primID];
                    if (hit_brush && (hit_brush->isGlass || hit_brush->isWater))
                    {
                        ray_origin = hit_pos;
                        ray_normal = normal;
                        continue;
                    }
                }

                Vec4 reflectivity_at_hit = get_reflectivity_at_hit(rayhit.hit.primID);
                Vec3 albedo = { reflectivity_at_hit.x, reflectivity_at_hit.y, reflectivity_at_hit.z };
                float alpha = reflectivity_at_hit.w;

                if (roulette_dist(rng) > alpha)
                {
                    throughput = vec3_muls(throughput, 1.0f - alpha);
                    ray_origin = hit_pos;
                    continue;
                }

                Vec3 dummy_dir;
                Vec3 direct_light = calculate_direct_light(hit_pos, hit_normal, dummy_dir);
                Vec3 reflected = vec3_mul(direct_light, albedo);

                Vec3 bounce_contribution = vec3_mul(reflected, throughput);

                path_radiance = vec3_add(path_radiance, bounce_contribution);

                throughput = vec3_mul(throughput, albedo);

                float p = std::max({ throughput.x, throughput.y, throughput.z });
                if (p < 0.001f || roulette_dist(rng) > p)
                    break;

                throughput = vec3_muls(throughput, 1.0f / p);

                ray_origin = hit_pos;
                ray_normal = hit_normal;
            }

            accumulated_color = vec3_add(accumulated_color, path_radiance);
            out_indirect_dir = vec3_add(out_indirect_dir, vec3_muls(first_bounce_dir, vec3_length(path_radiance)));
        }

        vec3_normalize(&out_indirect_dir);
        return vec3_muls(accumulated_color, 1.0f / (float)INDIRECT_SAMPLES_PER_POINT);
    }

    void Lightmapper::generate()
    {
        Console_Printf("[Lightmapper] Starting lightmap generation...");
        auto start_time = std::chrono::high_resolution_clock::now();
        m_scene->lightmapResolution = m_resolution;

        precalculate_material_reflectivity();
        prepare_jobs();
        if (m_jobs.empty()) return;

        Console_Printf("[Lightmapper] Using path tracing with %d bounces and %d indirect samples.", m_bounces, INDIRECT_SAMPLES_PER_POINT);

        unsigned int num_threads = std::thread::hardware_concurrency();
        std::vector<std::thread> threads;
        Console_Printf("[Lightmapper] Using %u threads for final gather.", num_threads);

        for (unsigned int i = 0; i < num_threads; ++i)
        {
            threads.emplace_back(&Lightmapper::worker_main, this);
        }
        for (auto& t : threads)
        {
            t.join();
        }

        Console_Printf("[Lightmapper] Denoising model vertex lighting using bilateral filter...");
        const int filter_passes = 10;
        const float normal_threshold = 0.7f;

        for (int i = 0; i < m_scene->numObjects; ++i)
        {
            const SceneObject& obj = m_scene->objects[i];
            if (!obj.model || !m_model_color_buffers[i] || !m_model_direction_buffers[i] || obj.model->totalVertexCount == 0) continue;

            unsigned int vertex_count = obj.model->totalVertexCount;

            std::vector<std::set<unsigned int>> adjacency;
            build_adjacency_list(obj, adjacency);

            auto temp_color_buffer = std::make_unique<Vec4[]>(vertex_count);
            auto temp_direction_buffer = std::make_unique<Vec4[]>(vertex_count);

            Vec4* read_color = m_model_color_buffers[i].get();
            Vec4* write_color = temp_color_buffer.get();
            Vec4* read_dir = m_model_direction_buffers[i].get();
            Vec4* write_dir = temp_direction_buffer.get();

            for (int pass = 0; pass < filter_passes; ++pass) {
                apply_bilateral_filter_pass(write_color, read_color, adjacency, obj, normal_threshold);
                filter_vertex_data(write_dir, read_dir, adjacency, vertex_count);

                std::swap(read_color, write_color);
                std::swap(read_dir, write_dir);
            }

            if (read_color != m_model_color_buffers[i].get()) {
                memcpy(m_model_color_buffers[i].get(), read_color, sizeof(Vec4) * vertex_count);
            }
            if (read_dir != m_model_direction_buffers[i].get()) {
                memcpy(m_model_direction_buffers[i].get(), read_dir, sizeof(Vec4) * vertex_count);
            }
        }

        for (int i = 0; i < m_scene->numObjects; ++i)
        {
            const SceneObject& obj = m_scene->objects[i];
            if (!obj.model || !m_model_color_buffers[i] || !m_model_direction_buffers[i]) continue;
            std::string model_name_str = (strlen(obj.targetname) > 0) ? obj.targetname : "Model_" + std::to_string(i);
            std::string sanitized_name = sanitize_filename(model_name_str);
            fs::path vlm_path = m_output_path / (sanitized_name + ".vlm");
            std::ofstream vlm_file(vlm_path, std::ios::binary);
            if (vlm_file)
            {
                const char header[] = "VLM1";
                unsigned int count = obj.model->totalVertexCount;
                vlm_file.write(header, 4);
                vlm_file.write(reinterpret_cast<const char*>(&count), sizeof(unsigned int));
                vlm_file.write(reinterpret_cast<const char*>(m_model_color_buffers[i].get()), sizeof(Vec4) * count);
            }
            else
            {
                Console_Printf_Error("[Lightmapper] ERROR: Could not write to '%s'", vlm_path.string().c_str());
            }

            fs::path vld_path = m_output_path / (sanitized_name + ".vld");
            std::ofstream vld_file(vld_path, std::ios::binary);
            if (vld_file)
            {
                const char header[] = "VLD1";
                unsigned int count = obj.model->totalVertexCount;
                vld_file.write(header, 4);
                vld_file.write(reinterpret_cast<const char*>(&count), sizeof(unsigned int));
                vld_file.write(reinterpret_cast<const char*>(m_model_direction_buffers[i].get()), sizeof(Vec4) * count);
            }
            else
            {
                Console_Printf_Error("[Lightmapper] ERROR: Could not write to '%s'", vld_path.string().c_str());
            }
        }

        auto end_time = std::chrono::high_resolution_clock::now();
        std::chrono::duration<float> duration = end_time - start_time;
        Console_Printf("[Lightmapper] Finished in %.2f seconds.", duration.count());
    }
}

void Lightmapper_Generate(Scene* scene, Engine* engine, int resolution, int bounces)
{
    try
    {
        Lightmapper mapper(scene, resolution, bounces);
        mapper.generate();
    }
    catch (const std::exception& e)
    {
        Console_Printf_Error("[Lightmapper] C++ Exception: %s", e.what());
    }
    catch (...)
    {
        Console_Printf_Error("[Lightmapper] Unknown C++ exception occurred.");
    }
}
#else
#include "lightmapper.h"
#include "gl_console.h"

void Lightmapper_Generate(Scene* scene, Engine* engine, int resolution, int bounces)
{
    Console_Printf_Error("[Lightmapper] Not available on x86 builds.");
}
#endif