/*
 * Copyright © 2025 Soft Sprint Studios
 * All rights reserved.
 *
 * This file is proprietary and confidential. Unauthorized reproduction,
 * modification, or distribution is strictly prohibited unless explicit
 * written permission is granted by Soft Sprint Studios.
 */
#ifdef ARCH_64BIT
#ifdef min
#undef min
#endif
#ifdef max
#undef max
#endif
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

namespace
{
    namespace fs = std::filesystem;

    constexpr float SHADOW_BIAS = 0.01f;
    constexpr int BLUR_RADIUS = 2;
    constexpr int VPL_SAMPLES_PER_TRIANGLE = 8;
    constexpr float VPL_BRIGHTNESS_THRESHOLD = 0.1f;
    constexpr float INDIRECT_LIGHT_STRENGTH = 0.5f;

    void embree_error_function(void* userPtr, RTCError error, const char* str)
    {
        Console_Printf_Error("[Embree] Error %d: %s", error, str);
    }

    struct VirtualPointLight {
        Vec3 position;
        Vec3 color;
        Vec3 normal;
    };

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
        Lightmapper(Scene* scene, int resolution);
        ~Lightmapper();
        void generate();

    private:
        void build_embree_scene();
        void prepare_jobs();
        void worker_main();
        void process_job(const JobPayload& job);
        void process_brush_face(const BrushFaceJobData& data);
        void process_model_vertex(const ModelVertexJobData& data);
        void generate_virtual_point_lights(int start_brush_index, int end_brush_index);

        void precalculate_material_reflectivity();

        bool is_in_shadow(const Vec3& start, const Vec3& end) const;
        static void apply_gaussian_blur(std::vector<float>& data, int width, int height, int channels);
        static void apply_gaussian_blur(std::vector<unsigned char>& data, int width, int height, int channels);
        static std::string sanitize_filename(std::string input);

        Scene* m_scene;
        int m_resolution;
        fs::path m_output_path;

        RTCDevice m_rtc_device;
        RTCScene m_rtc_scene;

        std::vector<JobPayload> m_jobs;
        std::atomic<size_t> m_next_job_index{ 0 };

        std::vector<std::unique_ptr<Vec4[]>> m_model_color_buffers;
        std::vector<std::unique_ptr<Vec4[]>> m_model_direction_buffers;
        std::vector<VirtualPointLight> m_vpls;
        std::map<const Material*, Vec3> m_material_reflectivity;
        std::mutex m_vpl_mutex;
        std::atomic<int> m_halton_index{ 0 };
    };

    Lightmapper::Lightmapper(Scene* scene, int resolution)
        : m_scene(scene), m_resolution(resolution), m_rtc_device(nullptr), m_rtc_scene(nullptr)
    {
        m_rtc_device = rtcNewDevice(nullptr);
        if (!m_rtc_device)
        {
            throw std::runtime_error("Failed to create Embree device.");
        }
        rtcSetDeviceErrorFunction(m_rtc_device, embree_error_function, nullptr);
        build_embree_scene();
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
    }

    void Lightmapper::build_embree_scene()
    {
        m_rtc_scene = rtcNewScene(m_rtc_device);
        rtcSetSceneBuildQuality(m_rtc_scene, RTC_BUILD_QUALITY_HIGH);
        rtcSetSceneFlags(m_rtc_scene, RTC_SCENE_FLAG_ROBUST);

        std::vector<Vec3> all_vertices;
        std::vector<unsigned int> all_indices;

        for (int i = 0; i < m_scene->numBrushes; ++i)
        {
            const Brush& b = m_scene->brushes[i];
            if (b.isTrigger || b.isWater) continue;

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

        RTCRay ray;
        ray.org_x = start.x;
        ray.org_y = start.y;
        ray.org_z = start.z;
        ray.dir_x = ray_dir.x;
        ray.dir_y = ray_dir.y;
        ray.dir_z = ray_dir.z;
        ray.tnear = SHADOW_BIAS;
        ray.tfar = max_dist - SHADOW_BIAS;
        ray.time = 0.0f;
        ray.mask = -1;
        ray.flags = 0;

        RTCRayQueryContext query_context;
        rtcInitRayQueryContext(&query_context);

        RTCOccludedArguments args;
        rtcInitOccludedArguments(&args);
        args.context = &query_context;

        rtcOccluded1(m_rtc_scene, &ray, &args);

        return ray.tfar == -std::numeric_limits<float>::infinity();
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

    void Lightmapper::process_brush_face(const BrushFaceJobData& data)
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
        std::vector<float> hdr_lightmap_data(m_resolution * m_resolution * 3);
        std::vector<unsigned char> dir_lightmap_data(m_resolution * m_resolution * 4, 0);

        for (int y = 0; y < m_resolution; ++y)
        {
            for (int x = 0; x < m_resolution; ++x)
            {
                Vec3 final_light_color = { 0, 0, 0 };
                Vec3 accumulated_direction = { 0, 0, 0 };

                constexpr int SAMPLES = 4;
                constexpr float sub_pixel_offsets[SAMPLES][2] = { {-0.25f, -0.25f}, {0.25f, -0.25f}, {-0.25f, 0.25f}, {0.25f, 0.25f} };

                for (int s = 0; s < SAMPLES; ++s)
                {
                    float u_tex = (static_cast<float>(x) + sub_pixel_offsets[s][0]) / (m_resolution - 1);
                    float v_tex = (static_cast<float>(y) + sub_pixel_offsets[s][1]) / (m_resolution - 1);
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

                    if (inside)
                    {
                        Vec3 direct_light_accumulator = { 0,0,0 };
                        Vec3 indirect_light_accumulator = { 0,0,0 };
                        Vec3 direction_accumulator_sample = { 0,0,0 };

                        Vec3 point_to_light_check = vec3_add(world_pos, vec3_muls(face_normal, SHADOW_BIAS));

                        for (int k = 0; k < m_scene->numActiveLights; ++k)
                        {
                            const Light& light = m_scene->lights[k];
                            if (!light.is_static) continue;
                            Vec3 light_dir = vec3_sub(light.position, point_to_light_check);
                            float dist = vec3_length(light_dir);
                            vec3_normalize(&light_dir);
                            if (dist > light.radius) continue;

                            float NdotL = std::max(0.0f, vec3_dot(face_normal, light_dir));
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
                            direct_light_accumulator = vec3_add(direct_light_accumulator, light_contribution);

                            float contribution_magnitude = vec3_length(light_contribution);
                            direction_accumulator_sample = vec3_add(direction_accumulator_sample, vec3_muls(light_dir, contribution_magnitude));
                        }

                        for (const auto& vpl : m_vpls) {
                            Vec3 vpl_dir = vec3_sub(vpl.position, point_to_light_check);
                            float dist_sq = vec3_length_sq(vpl_dir);
                            if (dist_sq < 0.001f) continue;
                            float dist = sqrtf(dist_sq);
                            vec3_normalize(&vpl_dir);
                            float NdotL_receiver = std::max(0.0f, vec3_dot(face_normal, vpl_dir));
                            float NdotL_emitter = std::max(0.0f, vec3_dot(vpl.normal, vec3_muls(vpl_dir, -1.0f)));
                            if (NdotL_receiver > 0.0f && NdotL_emitter > 0.0f) {
                                if (!is_in_shadow(point_to_light_check, vpl.position)) {
                                    float falloff = 1.0f / (dist_sq + 1.0f);
                                    Vec3 vpl_contribution = vec3_muls(vpl.color, NdotL_receiver * NdotL_emitter * falloff * INDIRECT_LIGHT_STRENGTH);
                                    indirect_light_accumulator = vec3_add(indirect_light_accumulator, vpl_contribution);
                                    direction_accumulator_sample = vec3_add(direction_accumulator_sample, vec3_muls(vpl_dir, vec3_length(vpl_contribution)));
                                }
                            }
                        }

                        final_light_color = vec3_add(final_light_color, vec3_add(direct_light_accumulator, indirect_light_accumulator));
                        accumulated_direction = vec3_add(accumulated_direction, direction_accumulator_sample);
                    }
                }

                final_light_color = vec3_muls(final_light_color, 1.0f / SAMPLES);
                if (vec3_length_sq(accumulated_direction) > 0.0001f) vec3_normalize(&accumulated_direction);
                else accumulated_direction = { 0,0,0 };

                int hdr_idx = (y * m_resolution + x) * 3;
                hdr_lightmap_data[hdr_idx + 0] = final_light_color.x;
                hdr_lightmap_data[hdr_idx + 1] = final_light_color.y;
                hdr_lightmap_data[hdr_idx + 2] = final_light_color.z;

                int dir_idx = (y * m_resolution + x) * 4;
                dir_lightmap_data[dir_idx + 0] = static_cast<unsigned char>((accumulated_direction.x * 0.5f + 0.5f) * 255.0f);
                dir_lightmap_data[dir_idx + 1] = static_cast<unsigned char>((accumulated_direction.y * 0.5f + 0.5f) * 255.0f);
                dir_lightmap_data[dir_idx + 2] = static_cast<unsigned char>((accumulated_direction.z * 0.5f + 0.5f) * 255.0f);
                dir_lightmap_data[dir_idx + 3] = 255;
            }
        }

        apply_gaussian_blur(hdr_lightmap_data, m_resolution, m_resolution, 3);
        apply_gaussian_blur(dir_lightmap_data, m_resolution, m_resolution, 4);

        fs::path color_path = data.output_dir / ("face_" + std::to_string(data.face_index) + "_color.hdr");
        stbi_write_hdr(color_path.string().c_str(), m_resolution, m_resolution, 3, hdr_lightmap_data.data());
        fs::path dir_path = data.output_dir / ("face_" + std::to_string(data.face_index) + "_dir.png");
        SDL_Surface* dir_surface = SDL_CreateRGBSurfaceFrom(dir_lightmap_data.data(), m_resolution, m_resolution, 32, m_resolution * 4,
            0x000000ff, 0x0000ff00, 0x00ff0000, 0xff000000);
        if (dir_surface) {
            IMG_SavePNG(dir_surface, dir_path.string().c_str());
            SDL_FreeSurface(dir_surface);
        }
    }

    void Lightmapper::process_model_vertex(const ModelVertexJobData& data)
    {
        const SceneObject& obj = m_scene->objects[data.model_index];
        unsigned int v_idx = data.vertex_index;

        Vec3 local_pos = { obj.model->combinedVertexData[v_idx * 3 + 0], obj.model->combinedVertexData[v_idx * 3 + 1], obj.model->combinedVertexData[v_idx * 3 + 2] };
        Vec3 local_normal = { obj.model->combinedNormalData[v_idx * 3 + 0], obj.model->combinedNormalData[v_idx * 3 + 1], obj.model->combinedNormalData[v_idx * 3 + 2] };

        Vec3 world_pos = mat4_mul_vec3(&obj.modelMatrix, local_pos);
        Vec3 world_normal = mat4_mul_vec3_dir(&obj.modelMatrix, local_normal);
        vec3_normalize(&world_normal);

        Vec3 direct_light_accumulator = { 0,0,0 }, direction_accumulator = { 0,0,0 };
        Vec3 point_to_light_check = vec3_add(world_pos, vec3_muls(world_normal, SHADOW_BIAS));

        for (int k = 0; k < m_scene->numActiveLights; ++k)
        {
            const Light& light = m_scene->lights[k];
            if (!light.is_static) continue;

            Vec3 light_dir = vec3_sub(light.position, point_to_light_check);
            float dist = vec3_length(light_dir);
            vec3_normalize(&light_dir);
            if (dist > light.radius) continue;

            float NdotL = std::max(0.0f, vec3_dot(world_normal, light_dir));
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
            direct_light_accumulator = vec3_add(direct_light_accumulator, light_contribution);

            float contribution_magnitude = vec3_length(light_contribution);
            direction_accumulator = vec3_add(direction_accumulator, vec3_muls(light_dir, contribution_magnitude));
        }

        Vec3 indirect_light_accumulator = { 0, 0, 0 };
        for (const auto& vpl : m_vpls) {
            Vec3 vpl_dir = vec3_sub(vpl.position, point_to_light_check);
            float dist_sq = vec3_length_sq(vpl_dir);
            if (dist_sq < 0.001f) continue;
            float dist = sqrtf(dist_sq);
            vec3_normalize(&vpl_dir);
            float NdotL_receiver = std::max(0.0f, vec3_dot(world_normal, vpl_dir));
            float NdotL_emitter = std::max(0.0f, vec3_dot(vpl.normal, vec3_muls(vpl_dir, -1.0f)));
            if (NdotL_receiver > 0.0f && NdotL_emitter > 0.0f) {
                if (!is_in_shadow(point_to_light_check, vpl.position)) {
                    float falloff = 1.0f / (dist_sq + 1.0f);
                    Vec3 vpl_contribution = vec3_muls(vpl.color, NdotL_receiver * NdotL_emitter * falloff * INDIRECT_LIGHT_STRENGTH);
                    indirect_light_accumulator = vec3_add(indirect_light_accumulator, vpl_contribution);
                    direction_accumulator = vec3_add(direction_accumulator, vec3_muls(vpl_dir, vec3_length(vpl_contribution)));
                }
            }
        }

        Vec3 final_light_color = vec3_add(direct_light_accumulator, indirect_light_accumulator);
        data.output_color_buffer[v_idx] = { final_light_color.x, final_light_color.y, final_light_color.z, 1.0f };

        if (vec3_length_sq(direction_accumulator) > 0.0001f) vec3_normalize(&direction_accumulator);
        else direction_accumulator = { 0,0,0 };
        data.output_direction_buffer[v_idx] = { direction_accumulator.x, direction_accumulator.y, direction_accumulator.z, 1.0f };
    }

    void Lightmapper::process_job(const JobPayload& job)
    {
        std::visit([this](auto&& arg) {
            using T = std::decay_t<decltype(arg)>;
            if constexpr (std::is_same_v<T, BrushFaceJobData>)
                process_brush_face(arg);
            else if constexpr (std::is_same_v<T, ModelVertexJobData>)
                process_model_vertex(arg);
            }, job);
    }

    void Lightmapper::worker_main()
    {
        while (true)
        {
            size_t job_index = m_next_job_index.fetch_add(1);
            if (job_index >= m_jobs.size())
            {
                break;
            }
            process_job(m_jobs[job_index]);
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
                m_material_reflectivity[mat] = { 0.5f, 0.5f, 0.5f };
                continue;
            }

            std::string full_path = "textures/" + std::string(mat->diffusePath);
            SDL_Surface* loaded_surface = IMG_Load(full_path.c_str());
            if (!loaded_surface) {
                Console_Printf_Warning("[Lightmapper] Could not load texture for reflectivity: %s", full_path.c_str());
                m_material_reflectivity[mat] = { 0.5f, 0.5f, 0.5f };
                continue;
            }

            SDL_Surface* surface = SDL_ConvertSurfaceFormat(loaded_surface, SDL_PIXELFORMAT_RGBA32, 0);
            SDL_FreeSurface(loaded_surface);

            if (!surface) {
                Console_Printf_Error("[Lightmapper] Could not convert surface for: %s", full_path.c_str());
                m_material_reflectivity[mat] = { 0.5f, 0.5f, 0.5f };
                continue;
            }

            long long total_r = 0, total_g = 0, total_b = 0;
            int width = surface->w;
            int height = surface->h;
            int texels = width * height;

            Uint8* pixels = (Uint8*)surface->pixels;
            for (int i = 0; i < texels; ++i) {
                total_r += pixels[i * 4 + 0];
                total_g += pixels[i * 4 + 1];
                total_b += pixels[i * 4 + 2];
            }

            SDL_FreeSurface(surface);

            Vec3 reflectivity = {
                static_cast<float>(total_r) / texels / 255.0f,
                static_cast<float>(total_g) / texels / 255.0f,
                static_cast<float>(total_b) / texels / 255.0f
            };

            float max_comp = std::max({ reflectivity.x, reflectivity.y, reflectivity.z });
            if (max_comp > 0.001f) {
                float scale = 1.0f / max_comp;
                reflectivity = vec3_muls(reflectivity, scale * 0.75f);
            }

            m_material_reflectivity[mat] = reflectivity;
        }
        Console_Printf("[Lightmapper] Reflectivity calculation complete.");
    }

    void Lightmapper::generate_virtual_point_lights(int start_brush_index, int end_brush_index)
    {
        std::vector<VirtualPointLight> local_vpls;

        for (int i = start_brush_index; i < end_brush_index; ++i)
        {
            const Brush& b = m_scene->brushes[i];
            if (b.isTrigger || b.isWater || b.isReflectionProbe || b.isGlass || b.isDSP) continue;

            for (int j = 0; j < b.numFaces; ++j)
            {
                const BrushFace& face = b.faces[j];
                if (face.numVertexIndices < 3) continue;

                const Material* mat = face.material;
                if (!mat || mat == &g_NodrawMaterial) continue;

                Vec3 reflectivity = { 0.5f, 0.5f, 0.5f };
                auto it = m_material_reflectivity.find(mat);
                if (it != m_material_reflectivity.end()) {
                    reflectivity = it->second;
                }

                for (int k = 0; k < face.numVertexIndices - 2; ++k)
                {
                    Vec3 v0 = mat4_mul_vec3(&b.modelMatrix, b.vertices[face.vertexIndices[0]].pos);
                    Vec3 v1 = mat4_mul_vec3(&b.modelMatrix, b.vertices[face.vertexIndices[k + 1]].pos);
                    Vec3 v2 = mat4_mul_vec3(&b.modelMatrix, b.vertices[face.vertexIndices[k + 2]].pos);

                    Vec3 face_normal = vec3_cross(vec3_sub(v1, v0), vec3_sub(v2, v0));
                    vec3_normalize(&face_normal);

                    for (int s = 0; s < VPL_SAMPLES_PER_TRIANGLE; ++s)
                    {
                        int sample_index = m_halton_index.fetch_add(1);
                        float r1 = halton(sample_index, 2);
                        float r2 = halton(sample_index, 3);
                        if (r1 + r2 > 1.0f) {
                            r1 = 1.0f - r1;
                            r2 = 1.0f - r2;
                        }

                        Vec3 sample_pos = vec3_add(v0, vec3_add(vec3_muls(vec3_sub(v1, v0), r1), vec3_muls(vec3_sub(v2, v0), r2)));
                        Vec3 point_to_light_check = vec3_add(sample_pos, vec3_muls(face_normal, SHADOW_BIAS));

                        Vec3 direct_light = { 0, 0, 0 };
                        for (int l = 0; l < m_scene->numActiveLights; ++l)
                        {
                            const Light& light = m_scene->lights[l];
                            if (!light.is_static) continue;

                            Vec3 light_dir = vec3_sub(light.position, point_to_light_check);
                            float dist = vec3_length(light_dir);
                            vec3_normalize(&light_dir);
                            if (dist > light.radius) continue;

                            float NdotL = std::max(0.0f, vec3_dot(face_normal, light_dir));
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
                            direct_light = vec3_add(direct_light, vec3_muls(light.color, light.intensity * NdotL * attenuation));
                        }

                        float brightness = vec3_dot(direct_light, { 0.299f, 0.587f, 0.114f });
                        if (brightness > VPL_BRIGHTNESS_THRESHOLD) {
                            VirtualPointLight vpl;
                            vpl.position = sample_pos;
                            vpl.color = vec3_mul(direct_light, reflectivity);
                            vpl.normal = face_normal;
                            local_vpls.push_back(vpl);
                        }
                    }
                }
            }
        }

        if (!local_vpls.empty())
        {
            std::lock_guard<std::mutex> lock(m_vpl_mutex);
            m_vpls.insert(m_vpls.end(), local_vpls.begin(), local_vpls.end());
        }
    }

    void Lightmapper::generate()
    {
        Console_Printf("[Lightmapper] Starting lightmap generation...");
        auto start_time = std::chrono::high_resolution_clock::now();
        m_scene->lightmapResolution = m_resolution;

        precalculate_material_reflectivity();

        Console_Printf("[Lightmapper] Generating Virtual Point Lights...");
        unsigned int num_threads = std::thread::hardware_concurrency();
        std::vector<std::thread> threads;
        int brushes_per_thread = (m_scene->numBrushes + num_threads - 1) / num_threads;

        m_vpls.reserve(m_scene->numBrushes * VPL_SAMPLES_PER_TRIANGLE);

        for (unsigned int i = 0; i < num_threads; ++i)
        {
            int start_brush = i * brushes_per_thread;
            int end_brush = std::min(start_brush + brushes_per_thread, m_scene->numBrushes);
            if (start_brush >= end_brush) continue;

            threads.emplace_back(&Lightmapper::generate_virtual_point_lights, this, start_brush, end_brush);
        }

        for (auto& t : threads) {
            t.join();
        }
        threads.clear();

        Console_Printf("[Lightmapper] Generated %zu VPLs.", m_vpls.size());

        prepare_jobs();
        if (m_jobs.empty()) return;

        Console_Printf("[Lightmapper] Using %u threads.", num_threads);

        for (unsigned int i = 0; i < num_threads; ++i)
        {
            threads.emplace_back(&Lightmapper::worker_main, this);
        }
        for (auto& t : threads)
        {
            t.join();
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

void Lightmapper_Generate(Scene* scene, Engine* engine, int resolution)
{
    try
    {
        Lightmapper mapper(scene, resolution);
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

void Lightmapper_Generate(Scene* scene, Engine* engine, int resolution)
{
    Console_Printf_Error("[Lightmapper] Not available on x86 builds.");
}
#endif