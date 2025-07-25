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
#include "texturemanager.h"

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

#include <SDL_image.h>
#include <embree4/rtcore.h>

namespace
{
    namespace fs = std::filesystem;

    constexpr float SHADOW_BIAS = 0.01f;
    constexpr int BLUR_RADIUS = 2;
    constexpr int VPL_GRID_SIZE = 5;
    constexpr float VPL_RADIUS = 256.0f;
    constexpr float VPL_INTENSITY_THRESHOLD = 0.01f;
    constexpr float VPL_INTENSITY_SCALE = 0.5f;

#pragma pack(push, 1)
    struct BMPFileHeader {
        uint16_t file_type{ 0x4D42 };
        uint32_t file_size{ 0 };
        uint16_t reserved1{ 0 };
        uint16_t reserved2{ 0 };
        uint32_t offset_data{ 0 };
    };

    struct BMPInfoHeader {
        uint32_t size{ 40 };
        int32_t width{ 0 };
        int32_t height{ 0 };
        uint16_t planes{ 1 };
        uint16_t bit_count{ 0 };
        uint32_t compression{ 0 };
        uint32_t size_image{ 0 };
        int32_t x_pixels_per_meter{ 0 };
        int32_t y_pixels_per_meter{ 0 };
        uint32_t colors_used{ 0 };
        uint32_t colors_important{ 0 };
    };
#pragma pack(pop)

    void embree_error_function(void* userPtr, RTCError error, const char* str)
    {
        Console_Printf_Error("[Embree] Error %d: %s", error, str);
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

    Vec3 aces_tonemap(Vec3 x) {
        constexpr float a = 2.51f;
        constexpr float b = 0.03f;
        constexpr float c = 2.43f;
        constexpr float d = 0.59f;
        constexpr float e = 0.14f;

        x.x = std::max(0.0f, (x.x * (a * x.x + b)) / (x.x * (c * x.x + d) + e));
        x.y = std::max(0.0f, (x.y * (a * x.y + b)) / (x.y * (c * x.y + d) + e));
        x.z = std::max(0.0f, (x.z * (a * x.z + b)) / (x.z * (c * x.z + d) + e));
        return x;
    }

    class Lightmapper
    {
    public:
        Lightmapper(Scene* scene, int resolution);
        ~Lightmapper();
        void generate();

    private:
        void build_embree_scene();
        void prepare_jobs();
        void generate_vpls();
        void worker_main();
        void process_job(const JobPayload& job);
        void process_brush_face(const BrushFaceJobData& data);
        void process_model_vertex(const ModelVertexJobData& data);

        bool is_in_shadow(const Vec3& start, const Vec3& end) const;
        static void apply_gaussian_blur(std::vector<unsigned char>& data, int width, int height, int channels);
        static void save_bmp(const fs::path& path, const void* data, int width, int height, int bit_depth);
        static std::string sanitize_filename(std::string input);

        SDL_Surface* get_surface(const std::string& path);
        Vec3 sample_surface(SDL_Surface* surface, float u, float v);
        Vec3 get_brush_albedo(const Brush* b, const BrushFace* face, const Vec3& barycentric, int v_idx0, int v_idx1, int v_idx2, const Vec3& face_normal);

        Scene* m_scene;
        int m_resolution;
        fs::path m_output_path;

        RTCDevice m_rtc_device;
        RTCScene m_rtc_scene;

        std::vector<JobPayload> m_jobs;
        std::atomic<size_t> m_next_job_index{ 0 };

        std::vector<std::unique_ptr<Vec4[]>> m_model_color_buffers;
        std::vector<std::unique_ptr<Vec4[]>> m_model_direction_buffers;
        std::vector<Light> m_vpls;

        std::mutex m_surface_cache_mutex;
        std::map<std::string, SDL_Surface*> m_surface_cache;
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
        for (auto const& [key, val] : m_surface_cache)
        {
            SDL_FreeSurface(val);
        }
        m_surface_cache.clear();
    }

    SDL_Surface* Lightmapper::get_surface(const std::string& path)
    {
        std::lock_guard<std::mutex> lock(m_surface_cache_mutex);
        if (m_surface_cache.count(path))
        {
            return m_surface_cache[path];
        }

        SDL_Surface* loaded_surface = IMG_Load(path.c_str());
        if (!loaded_surface)
        {
            Console_Printf_Warning("[Lightmapper] Warning: Failed to load texture '%s'.", path.c_str());
            m_surface_cache[path] = nullptr;
            return nullptr;
        }

        SDL_Surface* formatted_surface = SDL_ConvertSurfaceFormat(loaded_surface, SDL_PIXELFORMAT_RGBA32, 0);
        SDL_FreeSurface(loaded_surface);

        if (!formatted_surface) {
            Console_Printf_Warning("[Lightmapper] Warning: Failed to convert surface for '%s'.", path.c_str());
            m_surface_cache[path] = nullptr;
            return nullptr;
        }


        m_surface_cache[path] = formatted_surface;
        return formatted_surface;
    }

    Vec3 Lightmapper::sample_surface(SDL_Surface* surface, float u, float v)
    {
        if (!surface)
        {
            return { 1.0f, 0.0f, 1.0f };
        }

        u = fmodf(u, 1.0f);
        if (u < 0) u += 1.0f;
        v = fmodf(v, 1.0f);
        if (v < 0) v += 1.0f;

        int x = static_cast<int>(u * surface->w) % surface->w;
        int y = static_cast<int>((1.0f - v) * surface->h) % surface->h;

        uint32_t* pixels = static_cast<uint32_t*>(surface->pixels);
        uint32_t pixel = pixels[y * surface->w + x];

        SDL_Color color;
        SDL_GetRGBA(pixel, surface->format, &color.r, &color.g, &color.b, &color.a);

        return {
            powf(color.r / 255.0f, 2.2f),
            powf(color.g / 255.0f, 2.2f),
            powf(color.b / 255.0f, 2.2f)
        };
    }

    Vec3 Lightmapper::get_brush_albedo(const Brush* b, const BrushFace* face, const Vec3& barycentric, int v_idx0, int v_idx1, int v_idx2, const Vec3& face_normal)
    {
        const BrushVertex& bv0 = b->vertices[v_idx0];
        const BrushVertex& bv1 = b->vertices[v_idx1];
        const BrushVertex& bv2 = b->vertices[v_idx2];

        Vec4 interpolated_color = {
            bv0.color.x * barycentric.x + bv1.color.x * barycentric.y + bv2.color.x * barycentric.z,
            bv0.color.y * barycentric.x + bv1.color.y * barycentric.y + bv2.color.y * barycentric.z,
            bv0.color.z * barycentric.x + bv1.color.z * barycentric.y + bv2.color.z * barycentric.z,
            bv0.color.w * barycentric.x + bv1.color.w * barycentric.y + bv2.color.w * barycentric.z
        };

        Vec3 pos = vec3_add(vec3_add(vec3_muls(bv0.pos, barycentric.x), vec3_muls(bv1.pos, barycentric.y)), vec3_muls(bv2.pos, barycentric.z));

        float absX = fabsf(face_normal.x), absY = fabsf(face_normal.y), absZ = fabsf(face_normal.z);
        int dominant_axis = (absY > absX && absY > absZ) ? 1 : ((absX > absZ) ? 0 : 2);
        float u_coord, v_coord;
        if (dominant_axis == 0) { u_coord = pos.y; v_coord = pos.z; }
        else if (dominant_axis == 1) { u_coord = pos.x; v_coord = pos.z; }
        else { u_coord = pos.x; v_coord = pos.y; }

        auto calculate_uv = [&](float rot_deg, Vec2 scale, Vec2 offset) {
            float rad = rot_deg * (3.14159265f / 180.0f);
            float cos_r = cosf(rad); float sin_r = sinf(rad);
            float u = ((u_coord * cos_r - v_coord * sin_r) / scale.x) + offset.x;
            float v = ((u_coord * sin_r + v_coord * cos_r) / scale.y) + offset.y;
            return Vec2{ u, v };
            };

        Vec3 albedo1 = { 1.0f, 0.0f, 1.0f };
        if (face->material) {
            char* fullPath = prependTexturePath(face->material->diffusePath);
            albedo1 = sample_surface(get_surface(fullPath), calculate_uv(face->uv_rotation, face->uv_scale, face->uv_offset).x, calculate_uv(face->uv_rotation, face->uv_scale, face->uv_offset).y);
            free(fullPath);
        }

        Vec3 albedo2 = { 0,0,0 };
        if (face->material2) {
            char* fullPath = prependTexturePath(face->material2->diffusePath);
            albedo2 = sample_surface(get_surface(fullPath), calculate_uv(face->uv_rotation2, face->uv_scale2, face->uv_offset2).x, calculate_uv(face->uv_rotation2, face->uv_scale2, face->uv_offset2).y);
            free(fullPath);
        }

        Vec3 albedo3 = { 0,0,0 };
        if (face->material3) {
            char* fullPath = prependTexturePath(face->material3->diffusePath);
            albedo3 = sample_surface(get_surface(fullPath), calculate_uv(face->uv_rotation3, face->uv_scale3, face->uv_offset3).x, calculate_uv(face->uv_rotation3, face->uv_scale3, face->uv_offset3).y);
            free(fullPath);
        }

        Vec3 albedo4 = { 0,0,0 };
        if (face->material4) {
            char* fullPath = prependTexturePath(face->material4->diffusePath);
            albedo4 = sample_surface(get_surface(fullPath), calculate_uv(face->uv_rotation4, face->uv_scale4, face->uv_offset4).x, calculate_uv(face->uv_rotation4, face->uv_scale4, face->uv_offset4).y);
            free(fullPath);
        }

        float blendR = interpolated_color.x;
        float blendG = interpolated_color.y;
        float blendB = interpolated_color.z;
        float totalWeight = blendR + blendG + blendB;
        if (totalWeight > 1.0f) {
            blendR /= totalWeight;
            blendG /= totalWeight;
            blendB /= totalWeight;
        }
        float blendBase = 1.0f - (blendR + blendG + blendB);

        return vec3_add(vec3_add(vec3_add(vec3_muls(albedo1, blendBase), vec3_muls(albedo2, blendR)), vec3_muls(albedo3, blendG)), vec3_muls(albedo4, blendB));
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

    void Lightmapper::save_bmp(const fs::path& path, const void* data, int width, int height, int bit_depth)
    {
        BMPFileHeader file_header;
        BMPInfoHeader info_header;

        int channels = bit_depth / 8;
        int row_stride = width * channels;
        int padded_row_stride = (row_stride + 3) & (~3);

        info_header.width = width;
        info_header.height = height;
        info_header.bit_count = bit_depth;
        info_header.size_image = padded_row_stride * height;

        file_header.offset_data = sizeof(BMPFileHeader) + sizeof(BMPInfoHeader);
        file_header.file_size = file_header.offset_data + info_header.size_image;

        std::ofstream file(path, std::ios::binary);
        if (!file) {
            Console_Printf_Error("[Lightmapper] ERROR: Could not write to '%s'", path.string().c_str());
            return;
        }

        file.write(reinterpret_cast<const char*>(&file_header), sizeof(file_header));
        file.write(reinterpret_cast<const char*>(&info_header), sizeof(info_header));

        const unsigned char* pixel_data = static_cast<const unsigned char*>(data);
        const std::vector<unsigned char> padding(padded_row_stride - row_stride, 0);

        for (int i = height - 1; i >= 0; --i) {
            file.write(reinterpret_cast<const char*>(pixel_data + (i * row_stride)), row_stride);
            file.write(reinterpret_cast<const char*>(padding.data()), padding.size());
        }
    }

    void Lightmapper::generate_vpls()
    {
        Console_Printf("[Lightmapper] Generating Virtual Point Lights (VPLs)...");

        for (int i = 0; i < m_scene->numBrushes; ++i)
        {
            const Brush& b = m_scene->brushes[i];
            if (b.isTrigger || b.isWater) continue;

            for (int j = 0; j < b.numFaces; ++j)
            {
                const BrushFace& face = b.faces[j];
                if (face.numVertexIndices < 3) continue;

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
                    min_u = std::min(min_u, vec3_dot(world_verts[k], u_axis));
                    max_u = std::max(max_u, vec3_dot(world_verts[k], u_axis));
                    min_v = std::min(min_v, vec3_dot(world_verts[k], v_axis));
                    max_v = std::max(max_v, vec3_dot(world_verts[k], v_axis));
                }

                float u_range = std::max(0.001f, max_u - min_u);
                float v_range = std::max(0.001f, max_v - min_v);

                for (int y = VPL_GRID_SIZE / 2; y < m_resolution; y += VPL_GRID_SIZE)
                {
                    for (int x = VPL_GRID_SIZE / 2; x < m_resolution; x += VPL_GRID_SIZE)
                    {
                        float u_tex = static_cast<float>(x) / (m_resolution - 1);
                        float v_tex = static_cast<float>(y) / (m_resolution - 1);
                        float world_u = min_u + u_tex * u_range;
                        float world_v = min_v + v_tex * v_range;
                        Vec3 point_on_plane = vec3_add(vec3_muls(u_axis, world_u), vec3_muls(v_axis, world_v));

                        Vec3 world_pos;
                        bool inside = false;
                        for (int k = 0; k < face.numVertexIndices - 2; ++k)
                        {
                            Vec3 p0 = world_verts[0];
                            Vec3 p1 = world_verts[k + 1];
                            Vec3 p2 = world_verts[k + 2];

                            Vec3 v_p0p1 = vec3_sub(p1, p0);
                            Vec3 v_p0p2 = vec3_sub(p2, p0);
                            Vec3 v_p0pt = vec3_sub(point_on_plane, p0);

                            float dot00 = vec3_dot(v_p0p1, v_p0p1);
                            float dot01 = vec3_dot(v_p0p1, v_p0p2);
                            float dot02 = vec3_dot(v_p0p1, v_p0pt);
                            float dot11 = vec3_dot(v_p0p2, v_p0p2);
                            float dot12 = vec3_dot(v_p0p2, v_p0pt);

                            float inv_denom = 1.0f / (dot00 * dot11 - dot01 * dot01);
                            float bary_u = (dot11 * dot02 - dot01 * dot12) * inv_denom;
                            float bary_v = (dot00 * dot12 - dot01 * dot02) * inv_denom;
                            float bary_w = 1.0f - bary_u - bary_v;


                            if (bary_u >= 0.0f && bary_v >= 0.0f && (bary_u + bary_v < 1.0f))
                            {
                                inside = true;
                                world_pos = vec3_add(p0, vec3_add(vec3_muls(v_p0p1, bary_u), vec3_muls(v_p0p2, bary_v)));

                                Vec3 direct_light_accumulator = { 0,0,0 };
                                Vec3 point_to_light_check = vec3_add(world_pos, vec3_muls(face_normal, SHADOW_BIAS));

                                for (int light_idx = 0; light_idx < m_scene->numActiveLights; ++light_idx)
                                {
                                    const Light& light = m_scene->lights[light_idx];
                                    if (!light.is_static) continue;
                                    Vec3 light_dir = vec3_sub(light.position, point_to_light_check);
                                    float dist = vec3_length(light_dir);
                                    if (dist > light.radius) continue;
                                    vec3_normalize(&light_dir);

                                    float NdotL = std::max(0.0f, vec3_dot(face_normal, light_dir));
                                    if (NdotL <= 0.0f) continue;
                                    if (is_in_shadow(point_to_light_check, light.position)) continue;

                                    float attenuation = powf(std::max(0.0f, 1.0f - dist / light.radius), 2.0f);
                                    float spot_factor = 1.0f;
                                    if (light.type == LIGHT_SPOT)
                                    {
                                        float theta = vec3_dot(light_dir, vec3_muls(light.direction, -1.0f));
                                        if (theta < light.outerCutOff) continue;
                                        float epsilon = light.cutOff - light.outerCutOff;
                                        spot_factor = std::min(1.0f, (theta - light.outerCutOff) / epsilon);
                                    }
                                    Vec3 light_color = vec3_muls(light.color, light.intensity);
                                    direct_light_accumulator = vec3_add(direct_light_accumulator, vec3_muls(light_color, NdotL * attenuation * spot_factor));
                                }

                                if (vec3_length_sq(direct_light_accumulator) > VPL_INTENSITY_THRESHOLD)
                                {
                                    Vec3 albedo = get_brush_albedo(&b, &face, { bary_w, bary_u, bary_v }, face.vertexIndices[0], face.vertexIndices[k + 1], face.vertexIndices[k + 2], face_normal);

                                    Light vpl;
                                    vpl.is_static = true;
                                    vpl.type = LIGHT_POINT;
                                    vpl.position = vec3_add(world_pos, vec3_muls(face_normal, SHADOW_BIAS * 2.0f));
                                    vpl.color = vec3_muls(vec3_mul(direct_light_accumulator, albedo), VPL_INTENSITY_SCALE);
                                    vpl.intensity = 1.0f;
                                    vpl.radius = VPL_RADIUS;
                                    vpl.direction = { 0,0,0 };
                                    vpl.cutOff = 0.0f;
                                    vpl.outerCutOff = 0.0f;
                                    m_vpls.push_back(vpl);
                                }
                                break;
                            }
                        }
                    }
                }
            }
        }
        Console_Printf("[Lightmapper] Generated %zu VPLs.", m_vpls.size());
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
        std::vector<unsigned char> lightmap_data(m_resolution * m_resolution * 3, 0);
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
                        Vec3 light_accumulator = { 0,0,0 }, direction_accumulator_sample = { 0,0,0 };
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
                            float attenuation = powf(std::max(0.0f, 1.0f - dist / light.radius), 2.0f);
                            float spot_factor = 1.0f;
                            if (light.type == LIGHT_SPOT)
                            {
                                float theta = vec3_dot(light_dir, vec3_muls(light.direction, -1.0f));
                                if (theta < light.outerCutOff) continue;
                                float epsilon = light.cutOff - light.outerCutOff;
                                spot_factor = std::min(1.0f, (theta - light.outerCutOff) / epsilon);
                            }
                            if (!is_in_shadow(point_to_light_check, light.position))
                            {
                                Vec3 light_color = vec3_muls(light.color, light.intensity);
                                Vec3 light_contribution = vec3_muls(light_color, NdotL * attenuation * spot_factor);
                                light_accumulator = vec3_add(light_accumulator, light_contribution);
                                float contribution_magnitude = vec3_length(light_contribution);
                                direction_accumulator_sample = vec3_add(direction_accumulator_sample, vec3_muls(light_dir, contribution_magnitude));
                            }
                        }

                        for (const auto& light : m_vpls)
                        {
                            Vec3 light_dir = vec3_sub(light.position, point_to_light_check);
                            float dist = vec3_length(light_dir);
                            vec3_normalize(&light_dir);
                            if (dist > light.radius) continue;

                            float NdotL = std::max(0.0f, vec3_dot(face_normal, light_dir));
                            if (NdotL <= 0.0f) continue;

                            float attenuation = powf(std::max(0.0f, 1.0f - dist / light.radius), 2.0f);

                            if (!is_in_shadow(point_to_light_check, light.position))
                            {
                                Vec3 light_contribution = vec3_muls(light.color, NdotL * attenuation);
                                light_accumulator = vec3_add(light_accumulator, light_contribution);
                                float contribution_magnitude = vec3_length(light_contribution);
                                direction_accumulator_sample = vec3_add(direction_accumulator_sample, vec3_muls(light_dir, contribution_magnitude));
                            }
                        }
                        final_light_color = vec3_add(final_light_color, light_accumulator);
                        accumulated_direction = vec3_add(accumulated_direction, direction_accumulator_sample);
                    }
                }

                final_light_color = vec3_muls(final_light_color, 1.0f / SAMPLES);
                final_light_color = aces_tonemap(final_light_color);

                if (vec3_length_sq(accumulated_direction) > 0.0001f) vec3_normalize(&accumulated_direction);
                else accumulated_direction = { 0,0,0 };

                float gamma = 1.0f / 2.2f;
                float r = powf(final_light_color.x, gamma);
                float g = powf(final_light_color.y, gamma);
                float b = powf(final_light_color.z, gamma);

                int idx = (y * m_resolution + x) * 3;
                lightmap_data[idx + 2] = static_cast<unsigned char>(std::min(1.0f, r) * 255.0f);
                lightmap_data[idx + 1] = static_cast<unsigned char>(std::min(1.0f, g) * 255.0f);
                lightmap_data[idx + 0] = static_cast<unsigned char>(std::min(1.0f, b) * 255.0f);

                int dir_idx = (y * m_resolution + x) * 4;
                dir_lightmap_data[dir_idx + 2] = static_cast<unsigned char>((accumulated_direction.x * 0.5f + 0.5f) * 255.0f);
                dir_lightmap_data[dir_idx + 1] = static_cast<unsigned char>((accumulated_direction.y * 0.5f + 0.5f) * 255.0f);
                dir_lightmap_data[dir_idx + 0] = static_cast<unsigned char>((accumulated_direction.z * 0.5f + 0.5f) * 255.0f);
                dir_lightmap_data[dir_idx + 3] = 255;
            }
        }

        apply_gaussian_blur(lightmap_data, m_resolution, m_resolution, 3);
        apply_gaussian_blur(dir_lightmap_data, m_resolution, m_resolution, 4);

        save_bmp(data.output_dir / ("face_" + std::to_string(data.face_index) + "_color.bmp"), lightmap_data.data(), m_resolution, m_resolution, 24);
        save_bmp(data.output_dir / ("face_" + std::to_string(data.face_index) + "_dir.bmp"), dir_lightmap_data.data(), m_resolution, m_resolution, 32);
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

        Vec3 light_accumulator = { 0,0,0 }, direction_accumulator = { 0,0,0 };
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

            float attenuation = powf(std::max(0.0f, 1.0f - dist / light.radius), 2.0f);
            float spot_factor = 1.0f;
            if (light.type == LIGHT_SPOT)
            {
                float theta = vec3_dot(light_dir, vec3_muls(light.direction, -1.0f));
                if (theta < light.outerCutOff) continue;
                float epsilon = light.cutOff - light.outerCutOff;
                spot_factor = std::min(1.0f, (theta - light.outerCutOff) / epsilon);
            }

            if (!is_in_shadow(point_to_light_check, light.position))
            {
                Vec3 light_color = vec3_muls(light.color, light.intensity);
                Vec3 light_contribution = vec3_muls(light_color, NdotL * attenuation * spot_factor);
                light_accumulator = vec3_add(light_accumulator, light_contribution);
                float contribution_magnitude = vec3_length(light_contribution);
                direction_accumulator = vec3_add(direction_accumulator, vec3_muls(light_dir, contribution_magnitude));
            }
        }

        for (const auto& light : m_vpls)
        {
            Vec3 light_dir = vec3_sub(light.position, point_to_light_check);
            float dist = vec3_length(light_dir);
            vec3_normalize(&light_dir);
            if (dist > light.radius) continue;

            float NdotL = std::max(0.0f, vec3_dot(world_normal, light_dir));
            if (NdotL <= 0.0f) continue;

            float attenuation = powf(std::max(0.0f, 1.0f - dist / light.radius), 2.0f);

            if (!is_in_shadow(point_to_light_check, light.position))
            {
                Vec3 light_contribution = vec3_muls(light.color, NdotL * attenuation);
                light_accumulator = vec3_add(light_accumulator, light_contribution);
                float contribution_magnitude = vec3_length(light_contribution);
                direction_accumulator = vec3_add(direction_accumulator, vec3_muls(light_dir, contribution_magnitude));
            }
        }

        light_accumulator = aces_tonemap(light_accumulator);
        float gamma = 1.0f / 2.2f;
        data.output_color_buffer[v_idx] = {
            powf(light_accumulator.x, gamma),
            powf(light_accumulator.y, gamma),
            powf(light_accumulator.z, gamma),
            1.0f
        };

        if (vec3_length_sq(direction_accumulator) > 0.0001f) vec3_normalize(&direction_accumulator);
        else direction_accumulator = { 0,0,0 };

        data.output_direction_buffer[v_idx] = {
            direction_accumulator.x,
            direction_accumulator.y,
            direction_accumulator.z,
            1.0f
        };
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

    void Lightmapper::generate()
    {
        Console_Printf("[Lightmapper] Starting lightmap generation...");
        auto start_time = std::chrono::high_resolution_clock::now();
        m_scene->lightmapResolution = m_resolution;
        generate_vpls();
        prepare_jobs();
        if (m_jobs.empty()) return;

        unsigned int num_threads = std::thread::hardware_concurrency();
        Console_Printf("[Lightmapper] Using %u threads.", num_threads);

        std::vector<std::thread> threads;
        threads.reserve(num_threads);
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