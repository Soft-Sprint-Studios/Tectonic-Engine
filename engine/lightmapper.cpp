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
#include <sstream>
#include <SDL_image.h>
#include <embree4/rtcore.h>
#include "stb_image_write.h"
#include <OpenImageDenoise/oidn.h>

namespace
{
    namespace fs = std::filesystem;

    constexpr float SHADOW_BIAS = 0.01f;
    constexpr int BLUR_RADIUS = 2;
    constexpr int INDIRECT_SAMPLES_PER_POINT_BRUSHES = 64;
    constexpr int INDIRECT_SAMPLES_PER_POINT_MODELS = 768;
    constexpr int INDIRECT_SAMPLES_PER_POINT_AMBIENT_PROBES = 64;
    constexpr int INDIRECT_SAMPLES_PER_POINT_DECALS = 64;
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

    struct DecalJobData
    {
        int decal_index;
        fs::path output_dir;
    };

    struct ModelVertexJobData
    {
        int model_index;
        unsigned int vertex_index;
        Vec4* output_color_buffer;
        Vec4* output_direction_buffer;
    };

    struct EmissiveMaterial {
        const Material* material;
        Vec3 color;
        float intensity;
    };

    using JobPayload = std::variant<BrushFaceJobData, ModelVertexJobData, DecalJobData>;

    uint32_t generate_seed_from_pos(const Vec3& pos)
    {
        std::hash<float> hasher;
        uint32_t seed = hasher(pos.x);
        seed ^= hasher(pos.y) + 0x9e3779b9 + (seed << 6) + (seed >> 2);
        seed ^= hasher(pos.z) + 0x9e3779b9 + (seed << 6) + (seed >> 2);
        return seed;
    }

    class Lightmapper
    {
    public:
        Lightmapper(Scene* scene, int resolution, int bounces);
        ~Lightmapper();
        void generate();

    private:
        void build_embree_scene();
        void load_emissive_materials();
        void generate_ambient_probes();
        void prepare_jobs();
        void worker_main();
        void process_job(const JobPayload& job);
        void process_brush_face(const BrushFaceJobData& data);
        void process_decal(const DecalJobData& data);
        void process_model_vertex(const ModelVertexJobData& data);

        Vec3 calculate_direct_light(const Vec3& pos, const Vec3& normal, Vec3& out_dominant_dir) const;
        Vec3 calculate_direct_sun_light_only(const Vec3& pos, const Vec3& normal) const;
        Vec3 calculate_indirect_light(const Vec3& origin, const Vec3& normal, std::mt19937& rng, Vec3& out_indirect_dir, int num_samples);
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
        std::map<const Material*, std::pair<Vec3, float>> m_emissive_materials;
        std::map<const Material*, Vec4> m_material_reflectivity;
        std::map<const BrushFace*, Vec4> m_face_reflectivity;
        std::vector<const BrushFace*> m_primID_to_face_map;
        std::vector<const Brush*> m_primID_to_brush_map;
        std::vector<const Material*> m_primID_to_material_map;
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
        load_emissive_materials();
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
            if (b.isTrigger || b.isReflectionProbe || b.isDSP || b.isWater) continue;

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
            if (obj.mass > 0.0f || !obj.casts_shadows) continue;
            if (!obj.model || !obj.model->combinedIndexData) continue;
            for (int mesh_idx = 0; mesh_idx < obj.model->meshCount; ++mesh_idx) {
                const Mesh& mesh = obj.model->meshes[mesh_idx];
                size_t num_primitives = mesh.indexCount / 3;
                for (size_t k = 0; k < num_primitives; ++k) {
                    m_primID_to_material_map.push_back(mesh.material);
                }
            }

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

    void Lightmapper::load_emissive_materials()
    {
        Console_Printf("[Lightmapper] Loading lights.rad...");
        std::ifstream file("lights.rad");
        if (!file.is_open()) {
            Console_Printf("[Lightmapper] lights.rad not found. No emissive surfaces will be used.");
            return;
        }

        std::string line;
        int line_num = 0;
        while (std::getline(file, line)) {
            line_num++;

            if (line.empty() || line[0] == '/' || line[0] == '#') {
                continue;
            }

            std::istringstream iss(line);
            std::string material_name;
            float r, g, b, intensity;

            if (!(iss >> material_name >> r >> g >> b >> intensity)) {
                Console_Printf_Warning("[Lightmapper] Malformed line %d in lights.rad", line_num);
                continue;
            }

            Material* mat = TextureManager_FindMaterial(material_name.c_str());
            if (mat == &g_MissingMaterial || mat == &g_NodrawMaterial) {
                Console_Printf_Warning("[Lightmapper] Material '%s' from lights.rad not found or is nodraw.", material_name.c_str());
                continue;
            }

            m_emissive_materials[mat] = { {r / 255.0f, g / 255.0f, b / 255.0f}, intensity };
            Console_Printf("[Lightmapper] Loaded emissive material: %s", material_name.c_str());
        }
        Console_Printf("[Lightmapper] Loaded %zu emissive materials.", m_emissive_materials.size());
    }

    void Lightmapper::generate_ambient_probes()
    {
        Console_Printf("[Lightmapper] Generating ambient probes...");

        std::vector<Vec3> probe_positions;
        const float probe_spacing = 1.0f;

        for (int i = 0; i < m_scene->numBrushes; ++i) {
            const Brush& b = m_scene->brushes[i];
            if (b.isTrigger || b.isReflectionProbe || b.isDSP || b.isWater || b.numVertices == 0) continue;

            Vec3 min_aabb = { FLT_MAX, FLT_MAX, FLT_MAX };
            Vec3 max_aabb = { -FLT_MAX, -FLT_MAX, -FLT_MAX };
            for (int j = 0; j < b.numVertices; ++j) {
                Vec3 world_v = mat4_mul_vec3(&b.modelMatrix, b.vertices[j].pos);
                min_aabb.x = std::min(min_aabb.x, world_v.x);
                min_aabb.y = std::min(min_aabb.y, world_v.y);
                min_aabb.z = std::min(min_aabb.z, world_v.z);
                max_aabb.x = std::max(max_aabb.x, world_v.x);
                max_aabb.y = std::max(max_aabb.y, world_v.y);
                max_aabb.z = std::max(max_aabb.z, world_v.z);
            }

            for (float x = min_aabb.x; x <= max_aabb.x; x += probe_spacing) {
                for (float y = min_aabb.y; y <= max_aabb.y; y += probe_spacing) {
                    for (float z = min_aabb.z; z <= max_aabb.z; z += probe_spacing) {

                        Vec3 probe_pos = { x, y, z };
                        std::mt19937 validation_rng(generate_seed_from_pos(probe_pos));

                        const int validation_rays = 16;
                        const float validation_distance = 0.5f;
                        int hits = 0;

                        for (int k = 0; k < validation_rays; ++k)
                        {
                            std::uniform_real_distribution<float> dist(-1.0f, 1.0f);
                            Vec3 ray_dir = { dist(validation_rng), dist(validation_rng), dist(validation_rng) };
                            vec3_normalize(&ray_dir);

                            RTCRay ray;
                            ray.org_x = probe_pos.x;
                            ray.org_y = probe_pos.y;
                            ray.org_z = probe_pos.z;
                            ray.dir_x = ray_dir.x;
                            ray.dir_y = ray_dir.y;
                            ray.dir_z = ray_dir.z;
                            ray.tnear = 0.01f;
                            ray.tfar = validation_distance;
                            ray.mask = -1;
                            ray.flags = 0;

                            RTCOccludedArguments args;
                            rtcInitOccludedArguments(&args);

                            rtcOccluded1(m_rtc_scene, &ray, &args);

                            if (ray.tfar < 0.0f)
                            {
                                hits++;
                            }
                        }

                        if (static_cast<float>(hits) / validation_rays > 0.25f)
                        {
                            continue;
                        }

                        probe_positions.push_back(probe_pos);
                    }
                }
            }
        }

        if (probe_positions.empty()) {
            Console_Printf("[Lightmapper] No suitable locations for ambient probes found.");
            return;
        }

        Console_Printf("[Lightmapper] Placing %zu validated ambient probes.", probe_positions.size());

        m_scene->num_ambient_probes = probe_positions.size();
        m_scene->ambient_probes = new AmbientProbe[m_scene->num_ambient_probes];

        for (size_t i = 0; i < probe_positions.size(); ++i) {
            m_scene->ambient_probes[i].position = probe_positions[i];
            Vec3 dominant_dir_total = { 0,0,0 };
            std::mt19937 lighting_rng(generate_seed_from_pos(probe_positions[i]));

            Vec3 directions[6] = { {1,0,0}, {-1,0,0}, {0,1,0}, {0,-1,0}, {0,0,1}, {0,0,-1} };
            for (int j = 0; j < 6; ++j) {
                Vec3 direct_dir, indirect_dir;
                Vec3 direct_light = calculate_direct_light(probe_positions[i], directions[j], direct_dir);
                Vec3 indirect_light = calculate_indirect_light(probe_positions[i], directions[j], lighting_rng, indirect_dir, INDIRECT_SAMPLES_PER_POINT_AMBIENT_PROBES);
                m_scene->ambient_probes[i].colors[j] = vec3_muls(vec3_add(direct_light, indirect_light), 2.2f);
                dominant_dir_total = vec3_add(dominant_dir_total, vec3_add(direct_dir, indirect_dir));
            }

            if (vec3_length_sq(dominant_dir_total) > 0.001f) {
                vec3_normalize(&dominant_dir_total);
            }
            m_scene->ambient_probes[i].dominant_direction = dominant_dir_total;
        }

        fs::path probe_path = m_output_path / "ambient_probes.amp";
        std::ofstream probe_file(probe_path, std::ios::binary);
        if (probe_file) {
            const char header[] = "AMBI";
            probe_file.write(header, 4);
            probe_file.write(reinterpret_cast<const char*>(&m_scene->num_ambient_probes), sizeof(int));
            probe_file.write(reinterpret_cast<const char*>(m_scene->ambient_probes), sizeof(AmbientProbe) * m_scene->num_ambient_probes);
            Console_Printf("[Lightmapper] Saved %d ambient probes.", m_scene->num_ambient_probes);
        }
        else {
            Console_Printf_Error("[Lightmapper] Failed to save ambient probes file.");
        }
        delete[] m_scene->ambient_probes;
        m_scene->ambient_probes = nullptr;
        m_scene->num_ambient_probes = 0;
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

    static Vec2 calculate_texture_uv_for_vertex(const Brush* b, int face_index, int vertex_index) {
        const BrushFace& face = b->faces[face_index];
        Vec3 pos = b->vertices[vertex_index].pos;

        Vec3 p0 = b->vertices[face.vertexIndices[0]].pos;
        Vec3 p1 = b->vertices[face.vertexIndices[1]].pos;
        Vec3 p2 = b->vertices[face.vertexIndices[2]].pos;
        Vec3 normal_vec = vec3_cross(vec3_sub(p1, p0), vec3_sub(p2, p0));
        vec3_normalize(&normal_vec);

        float absX = fabsf(normal_vec.x), absY = fabsf(normal_vec.y), absZ = fabsf(normal_vec.z);
        int dominant_axis = (absY > absX && absY > absZ) ? 1 : ((absX > absZ) ? 0 : 2);

        float u, v;
        if (dominant_axis == 0) { u = pos.y; v = pos.z; }
        else if (dominant_axis == 1) { u = pos.x; v = pos.z; }
        else { u = pos.x; v = pos.y; }

        float rad = face.uv_rotation * (M_PI / 180.0f);
        float cos_r = cosf(rad); float sin_r = sinf(rad);

        Vec2 final_uv;
        final_uv.x = ((u * cos_r - v * sin_r) / face.uv_scale.x) + face.uv_offset.x;
        final_uv.y = ((u * sin_r + v * cos_r) / face.uv_scale.y) + face.uv_offset.y;

        return final_uv;
    }

    void Lightmapper::process_brush_face(const BrushFaceJobData& data)
    {
        const Brush& b = m_scene->brushes[data.brush_index];
        const BrushFace& face = b.faces[data.face_index];
        if (face.numVertexIndices < 3) return;

        Vec3 face_normal = { 0.0f, 0.0f, 0.0f };
        for (int k = 0; k < face.numVertexIndices - 2; ++k) {
            Vec3 p0 = b.vertices[face.vertexIndices[0]].pos;
            Vec3 p1 = b.vertices[face.vertexIndices[k + 1]].pos;
            Vec3 p2 = b.vertices[face.vertexIndices[k + 2]].pos;
            Vec3 tri_normal = vec3_cross(vec3_sub(p1, p0), vec3_sub(p2, p0));
            face_normal = vec3_add(face_normal, tri_normal);
        }
        vec3_normalize(&face_normal);

        Vec2 min_uv = { FLT_MAX, FLT_MAX };
        Vec2 max_uv = { -FLT_MAX, -FLT_MAX };
        std::vector<Vec3> world_verts(face.numVertexIndices);
        for (int k = 0; k < face.numVertexIndices; ++k) {
            world_verts[k] = mat4_mul_vec3(&b.modelMatrix, b.vertices[face.vertexIndices[k]].pos);
            Vec2 uv = calculate_texture_uv_for_vertex(&b, data.face_index, face.vertexIndices[k]);
            min_uv.x = std::min(min_uv.x, uv.x);
            min_uv.y = std::min(min_uv.y, uv.y);
            max_uv.x = std::max(max_uv.x, uv.x);
            max_uv.y = std::max(max_uv.y, uv.y);
        }

        Vec2 uv_range = { std::max(0.001f, max_uv.x - min_uv.x), std::max(0.001f, max_uv.y - min_uv.y) };
        float u_range = uv_range.x * face.uv_scale.x;
        float v_range = uv_range.y * face.uv_scale.y;

        float effective_luxels_per_unit = LUXELS_PER_UNIT / face.lightmap_scale;
        int lightmap_width = std::clamp(static_cast<int>(ceilf(u_range * effective_luxels_per_unit)), 4, m_resolution);
        int lightmap_height = std::clamp(static_cast<int>(ceilf(v_range * effective_luxels_per_unit)), 4, m_resolution);

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
                float target_u = min_uv.x + u_tex * uv_range.x;
                float target_v = min_uv.y + v_tex * uv_range.y;

                Vec3 world_pos;
                bool inside = false;
                Vec3 point_on_plane;
                for (int k = 0; k < face.numVertexIndices - 2; ++k)
                {
                    int idx0 = face.vertexIndices[0];
                    int idx1 = face.vertexIndices[k + 1];
                    int idx2 = face.vertexIndices[k + 2];
                    Vec2 uv0 = calculate_texture_uv_for_vertex(&b, data.face_index, idx0);
                    Vec2 uv1 = calculate_texture_uv_for_vertex(&b, data.face_index, idx1);
                    Vec2 uv2 = calculate_texture_uv_for_vertex(&b, data.face_index, idx2);
                    Vec3 p0 = world_verts[0], p1 = world_verts[k + 1], p2 = world_verts[k + 2];

                    Vec3 barycentric = barycentric_coords({ target_u, target_v }, uv0, uv1, uv2);

                    if (barycentric.x >= 0 && barycentric.y >= 0 && barycentric.z >= 0) {
                        inside = true;
                        world_pos = vec3_add(vec3_muls(p0, barycentric.x), vec3_add(vec3_muls(p1, barycentric.y), vec3_muls(p2, barycentric.z)));
                        break;
                    }
                }

                int hdr_idx = (y * lightmap_width + x) * 3;

                if (inside)
                {
                    std::mt19937 rng(generate_seed_from_pos(world_pos));
                    direct_light_color = calculate_direct_light(world_pos, face_normal, accumulated_direction);
                    indirect_light_color = calculate_indirect_light(world_pos, face_normal, rng, indirect_direction, INDIRECT_SAMPLES_PER_POINT_BRUSHES);
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
        for (int y = 0; y < lightmap_height; ++y) {
            for (int x = 0; x < lightmap_width; ++x) {
                int idx = (y * lightmap_width + x) * 3;
                Vec3 direct_light = { direct_lightmap_data[idx], direct_lightmap_data[idx + 1], direct_lightmap_data[idx + 2] };
                Vec3 indirect_light = { denoised_indirect_data[idx], denoised_indirect_data[idx + 1], denoised_indirect_data[idx + 2] };

                float u_tex = (static_cast<float>(x) + 0.5f) / lightmap_width;
                float v_tex = (static_cast<float>(y) + 0.5f) / lightmap_height;
                float target_u = min_uv.x + u_tex * uv_range.x;
                float target_v = min_uv.y + v_tex * uv_range.y;
                Vec3 world_pos;
                bool inside = false;
                for (int k = 0; k < face.numVertexIndices - 2; ++k)
                {
                    int idx0 = face.vertexIndices[0];
                    int idx1 = face.vertexIndices[k + 1];
                    int idx2 = face.vertexIndices[k + 2];
                    Vec2 uv0 = calculate_texture_uv_for_vertex(&b, data.face_index, idx0);
                    Vec2 uv1 = calculate_texture_uv_for_vertex(&b, data.face_index, idx1);
                    Vec2 uv2 = calculate_texture_uv_for_vertex(&b, data.face_index, idx2);
                    Vec3 p0 = world_verts[0], p1 = world_verts[k + 1], p2 = world_verts[k + 2];
                    Vec3 barycentric = barycentric_coords({ target_u, target_v }, uv0, uv1, uv2);
                    if (barycentric.x >= 0 && barycentric.y >= 0 && barycentric.z >= 0) {
                        inside = true;
                        world_pos = vec3_add(vec3_muls(p0, barycentric.x), vec3_add(vec3_muls(p1, barycentric.y), vec3_muls(p2, barycentric.z)));
                        break;
                    }
                }

                Vec3 direct_sun_light = { 0,0,0 };
                if (inside) {
                    direct_sun_light = calculate_direct_sun_light_only(world_pos, face_normal);
                }

                Vec3 final_light = vec3_add(vec3_sub(direct_light, direct_sun_light), indirect_light);
                final_hdr_lightmap_data[idx] = final_light.x;
                final_hdr_lightmap_data[idx + 1] = final_light.y;
                final_hdr_lightmap_data[idx + 2] = final_light.z;
            }
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

        int padded_width = lightmap_width + LIGHTMAPPADDING * 2;
        int padded_height = lightmap_height + LIGHTMAPPADDING * 2;

        std::vector<float> padded_hdr_data(padded_width * padded_height * 3);
        for (int y = 0; y < padded_height; ++y) {
            for (int x = 0; x < padded_width; ++x) {
                int src_x = std::clamp(x - LIGHTMAPPADDING, 0, lightmap_width - 1);
                int src_y = std::clamp(y - LIGHTMAPPADDING, 0, lightmap_height - 1);
                for (int c = 0; c < 3; ++c) {
                    padded_hdr_data[(y * padded_width + x) * 3 + c] = final_hdr_lightmap_data[(src_y * lightmap_width + src_x) * 3 + c];
                }
            }
        }

        std::vector<unsigned char> padded_dir_data(padded_width * padded_height * 4);
        for (int y = 0; y < padded_height; ++y) {
            for (int x = 0; x < padded_width; ++x) {
                int src_x = std::clamp(x - LIGHTMAPPADDING, 0, lightmap_width - 1);
                int src_y = std::clamp(y - LIGHTMAPPADDING, 0, lightmap_height - 1);
                for (int c = 0; c < 4; ++c) {
                    padded_dir_data[(y * padded_width + x) * 4 + c] = dir_lightmap_data[(src_y * lightmap_width + src_x) * 4 + c];
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

    void Lightmapper::process_decal(const DecalJobData& data)
    {
        const Decal& decal = m_scene->decals[data.decal_index];
        int lightmap_res = std::max(4, m_resolution / 4);

        Mat4 transform = create_trs_matrix(decal.pos, decal.rot, decal.size);
        Vec3 x_axis = { transform.m[0], transform.m[1], transform.m[2] };
        Vec3 y_axis = { transform.m[4], transform.m[5], transform.m[6] };
        Vec3 normal = { transform.m[8], transform.m[9], transform.m[10] };
        vec3_normalize(&x_axis);
        vec3_normalize(&y_axis);
        vec3_normalize(&normal);

        std::vector<float> direct_lightmap_data(lightmap_res * lightmap_res * 3);
        std::vector<float> indirect_lightmap_data(lightmap_res * lightmap_res * 3);
        std::vector<float> albedo_lightmap_data(lightmap_res * lightmap_res * 3);
        std::vector<float> normal_lightmap_data(lightmap_res * lightmap_res * 3);
        std::vector<float> direction_float_data(lightmap_res * lightmap_res * 3);

        Vec4 decal_reflectivity = { 0.5f, 0.5f, 0.5f, 1.0f };
        if (decal.material) {
            auto it = m_material_reflectivity.find(decal.material);
            if (it != m_material_reflectivity.end()) {
                decal_reflectivity = it->second;
            }
        }

        for (int y = 0; y < lightmap_res; ++y) {
            for (int x = 0; x < lightmap_res; ++x) {
                float u = (static_cast<float>(x) + 0.5f) / lightmap_res;
                float v = (static_cast<float>(y) + 0.5f) / lightmap_res;

                float local_x = u - 0.5f;
                float local_y = v - 0.5f;

                Vec3 local_pos_on_quad = vec3_add(vec3_muls(x_axis, local_x), vec3_muls(y_axis, local_y));
                Vec3 world_pos = vec3_add(decal.pos, local_pos_on_quad);

                std::mt19937 rng(generate_seed_from_pos(world_pos));
                Vec3 dominant_dir = { 0,0,0 }, indirect_dir = { 0,0,0 };
                Vec3 direct_light = calculate_direct_light(world_pos, normal, dominant_dir);
                Vec3 indirect_light = calculate_indirect_light(world_pos, normal, rng, indirect_dir, INDIRECT_SAMPLES_PER_POINT_DECALS);

                int idx = (y * lightmap_res + x) * 3;

                direct_lightmap_data[idx + 0] = direct_light.x;
                direct_lightmap_data[idx + 1] = direct_light.y;
                direct_lightmap_data[idx + 2] = direct_light.z;

                indirect_lightmap_data[idx + 0] = indirect_light.x;
                indirect_lightmap_data[idx + 1] = indirect_light.y;
                indirect_lightmap_data[idx + 2] = indirect_light.z;

                albedo_lightmap_data[idx + 0] = decal_reflectivity.x;
                albedo_lightmap_data[idx + 1] = decal_reflectivity.y;
                albedo_lightmap_data[idx + 2] = decal_reflectivity.z;

                normal_lightmap_data[idx + 0] = normal.x;
                normal_lightmap_data[idx + 1] = normal.y;
                normal_lightmap_data[idx + 2] = normal.z;

                Vec3 total_dir = vec3_add(dominant_dir, indirect_dir);
                if (vec3_length_sq(total_dir) > 0.0001f) vec3_normalize(&total_dir);
                direction_float_data[idx + 0] = total_dir.x;
                direction_float_data[idx + 1] = total_dir.y;
                direction_float_data[idx + 2] = total_dir.z;
            }
        }

        std::vector<float> denoised_indirect_data(lightmap_res * lightmap_res * 3);
        float indirect_sum = 0.0f; for (float val : indirect_lightmap_data) indirect_sum += val;
        if (indirect_sum > 0.0001f) {
            OIDNFilter filter = oidnNewFilter(m_oidn_device, "RTLightmap");
            oidnSetSharedFilterImage(filter, "color", indirect_lightmap_data.data(), OIDN_FORMAT_FLOAT3, lightmap_res, lightmap_res, 0, sizeof(float) * 3, sizeof(float) * 3 * lightmap_res);
            oidnSetSharedFilterImage(filter, "albedo", albedo_lightmap_data.data(), OIDN_FORMAT_FLOAT3, lightmap_res, lightmap_res, 0, sizeof(float) * 3, sizeof(float) * 3 * lightmap_res);
            oidnSetSharedFilterImage(filter, "normal", normal_lightmap_data.data(), OIDN_FORMAT_FLOAT3, lightmap_res, lightmap_res, 0, sizeof(float) * 3, sizeof(float) * 3 * lightmap_res);
            oidnSetSharedFilterImage(filter, "output", denoised_indirect_data.data(), OIDN_FORMAT_FLOAT3, lightmap_res, lightmap_res, 0, sizeof(float) * 3, sizeof(float) * 3 * lightmap_res);
            oidnSetFilterBool(filter, "hdr", true);
            oidnSetFilterBool(filter, "cleanAux", true);
            oidnCommitFilter(filter);
            oidnExecuteFilter(filter);
            oidnReleaseFilter(filter);
        }
        else {
            std::fill(denoised_indirect_data.begin(), denoised_indirect_data.end(), 0.0f);
        }

        std::vector<float> final_hdr_lightmap_data(lightmap_res * lightmap_res * 3);
        for (size_t i = 0; i < final_hdr_lightmap_data.size() / 3; ++i) {
            Vec3 direct_light = { direct_lightmap_data[i * 3], direct_lightmap_data[i * 3 + 1], direct_lightmap_data[i * 3 + 2] };
            Vec3 indirect_light = { denoised_indirect_data[i * 3], denoised_indirect_data[i * 3 + 1], denoised_indirect_data[i * 3 + 2] };
            Vec3 direct_sun_light = calculate_direct_sun_light_only(decal.pos, normal);
            Vec3 final_light = vec3_add(vec3_sub(direct_light, direct_sun_light), indirect_light);
            final_hdr_lightmap_data[i * 3] = final_light.x;
            final_hdr_lightmap_data[i * 3 + 1] = final_light.y;
            final_hdr_lightmap_data[i * 3 + 2] = final_light.z;
        }

        std::vector<float> filtered_direction_data;
        apply_guided_filter(filtered_direction_data, direction_float_data, final_hdr_lightmap_data, lightmap_res, lightmap_res, 4, 0.01f);

        std::vector<unsigned char> dir_data_u8(lightmap_res * lightmap_res * 4);
        for (int i = 0; i < lightmap_res * lightmap_res; ++i) {
            Vec3 dir = { filtered_direction_data[i * 3], filtered_direction_data[i * 3 + 1], filtered_direction_data[i * 3 + 2] };
            if (vec3_length_sq(dir) > 0.0001f) vec3_normalize(&dir); else dir = { 0,0,0 };
            dir_data_u8[i * 4 + 0] = static_cast<unsigned char>((dir.x * 0.5f + 0.5f) * 255.0f);
            dir_data_u8[i * 4 + 1] = static_cast<unsigned char>((dir.y * 0.5f + 0.5f) * 255.0f);
            dir_data_u8[i * 4 + 2] = static_cast<unsigned char>((dir.z * 0.5f + 0.5f) * 255.0f);
            dir_data_u8[i * 4 + 3] = 255;
        }

        fs::path color_path = data.output_dir / "lightmap_color.hdr";
        stbi_write_hdr(color_path.string().c_str(), lightmap_res, lightmap_res, 3, final_hdr_lightmap_data.data());

        fs::path dir_path = data.output_dir / "lightmap_dir.png";
        stbi_write_png(dir_path.string().c_str(), lightmap_res, lightmap_res, 4, dir_data_u8.data(), lightmap_res * 4);
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

        std::mt19937 rng(generate_seed_from_pos(world_pos));
        Vec3 direction_accumulator = { 0,0,0 };
        Vec3 indirect_dir = { 0,0,0 };
        Vec3 direct_light = calculate_direct_light(world_pos, world_normal, direction_accumulator);
        Vec3 indirect_light = calculate_indirect_light(world_pos, world_normal, rng, indirect_dir, INDIRECT_SAMPLES_PER_POINT_MODELS);
        Vec3 direct_sun_light = calculate_direct_sun_light_only(world_pos, world_normal);

        Vec3 final_light_color = vec3_add(vec3_sub(direct_light, direct_sun_light), indirect_light);
        direction_accumulator = vec3_add(direction_accumulator, indirect_dir);
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
            else if constexpr (std::is_same_v<T, DecalJobData>)
                process_decal(arg);
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
            if (b.isTrigger || b.isReflectionProbe || b.isGlass || b.isDSP) continue;
            std::string brush_name_str = (strlen(b.targetname) > 0) ? b.targetname : "Brush_" + std::to_string(i);
            fs::path brush_dir = m_output_path / sanitize_filename(brush_name_str);
            fs::create_directories(brush_dir);
            for (int j = 0; j < b.numFaces; ++j)
            {
                m_jobs.emplace_back(BrushFaceJobData{ i, j, brush_dir });
            }
        }

        for (int i = 0; i < m_scene->numDecals; ++i)
        {
            fs::path decal_dir = m_output_path / ("decal_" + std::to_string(i));
            fs::create_directories(decal_dir);
            m_jobs.emplace_back(DecalJobData{ i, decal_dir });
        }

        for (int i = 0; i < m_scene->numObjects; ++i)
        {
            const SceneObject& obj = m_scene->objects[i];
            if (obj.mass > 0.0f) continue;
            if (obj.model)
            {
                for (unsigned int v = 0; v < obj.model->totalVertexCount; ++v)
                {
                    m_jobs.emplace_back(ModelVertexJobData{ i, v, m_model_color_buffers[i].get(), m_model_direction_buffers[i].get() });
                }
            }
        }
        Console_Printf("[Lightmapper] Baking %zu faces, %zu vertices, and %d decals.", total_brush_faces, total_model_vertices, m_scene->numDecals);
    }

    void Lightmapper::precalculate_material_reflectivity()
    {
        Console_Printf("[Lightmapper] Pre-calculating surface reflectivity...");

        std::set<const Material*> unique_materials;
        for (int i = 0; i < m_scene->numBrushes; ++i) {
            const Brush& b = m_scene->brushes[i];
            for (int j = 0; j < b.numFaces; ++j) {
                const BrushFace& face = b.faces[j];
                if (face.material && face.material != &g_NodrawMaterial) unique_materials.insert(face.material);
                if (face.material2) unique_materials.insert(face.material2);
                if (face.material3) unique_materials.insert(face.material3);
                if (face.material4) unique_materials.insert(face.material4);
            }
        }
        for (int i = 0; i < m_scene->numDecals; ++i) {
            const Decal& d = m_scene->decals[i];
            if (d.material) {
                unique_materials.insert(d.material);
            }
        }
        for (int i = 0; i < m_scene->numObjects; ++i) {
            const SceneObject& obj = m_scene->objects[i];
            if (obj.model) {
                for (int j = 0; j < obj.model->meshCount; ++j) {
                    const Mesh& mesh = obj.model->meshes[j];
                    if (mesh.material) {
                        unique_materials.insert(mesh.material);
                    }
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
                m_material_reflectivity[mat] = { 0.5f, 0.5f, 0.5f, 1.0f };
                continue;
            }

            SDL_Surface* surface = SDL_ConvertSurfaceFormat(loaded_surface, SDL_PIXELFORMAT_RGBA32, 0);
            SDL_FreeSurface(loaded_surface);

            if (!surface) {
                m_material_reflectivity[mat] = { 0.5f, 0.5f, 0.5f, 1.0f };
                continue;
            }

            long long total_r = 0, total_g = 0, total_b = 0, total_a = 0;
            int texels = surface->w * surface->h;
            Uint8* pixels = (Uint8*)surface->pixels;
            for (int i = 0; i < texels; ++i) {
                total_r += pixels[i * 4 + 0];
                total_g += pixels[i * 4 + 1];
                total_b += pixels[i * 4 + 2];
                total_a += pixels[i * 4 + 3];
            }
            SDL_FreeSurface(surface);

            m_material_reflectivity[mat] = {
                static_cast<float>(total_r) / texels / 255.0f,
                static_cast<float>(total_g) / texels / 255.0f,
                static_cast<float>(total_b) / texels / 255.0f,
                static_cast<float>(total_a) / texels / 255.0f
            };
        }

        for (int i = 0; i < m_scene->numBrushes; ++i) {
            const Brush& b = m_scene->brushes[i];
            for (int j = 0; j < b.numFaces; ++j) {
                const BrushFace& face = b.faces[j];
                if (face.numVertexIndices == 0 || !face.material || face.material == &g_NodrawMaterial) {
                    continue;
                }

                Vec4 avg_color = { 0.0f, 0.0f, 0.0f, 0.0f };
                for (int k = 0; k < face.numVertexIndices; ++k) {
                    int vert_idx = face.vertexIndices[k];
                    avg_color = vec4_add(avg_color, b.vertices[vert_idx].color);
                }
                avg_color = vec4_muls(avg_color, 1.0f / face.numVertexIndices);

                Vec4 mat1_refl = m_material_reflectivity.count(face.material) ? m_material_reflectivity[face.material] : Vec4{ 0.5f, 0.5f, 0.5f, 1.0f };
                Vec4 mat2_refl = face.material2 && m_material_reflectivity.count(face.material2) ? m_material_reflectivity[face.material2] : Vec4{ 0.0f, 0.0f, 0.0f, 1.0f };
                Vec4 mat3_refl = face.material3 && m_material_reflectivity.count(face.material3) ? m_material_reflectivity[face.material3] : Vec4{ 0.0f, 0.0f, 0.0f, 1.0f };
                Vec4 mat4_refl = face.material4 && m_material_reflectivity.count(face.material4) ? m_material_reflectivity[face.material4] : Vec4{ 0.0f, 0.0f, 0.0f, 1.0f };

                float weightR = avg_color.x;
                float weightG = avg_color.y;
                float weightB = avg_color.z;
                float totalWeight = std::max(weightR + weightG + weightB, 0.0001f);
                if (totalWeight > 1.0f) {
                    weightR /= totalWeight;
                    weightG /= totalWeight;
                    weightB /= totalWeight;
                }
                float weightBase = 1.0f - (weightR + weightG + weightB);

                Vec4 blended_refl;
                blended_refl.x = mat1_refl.x * weightBase + mat2_refl.x * weightR + mat3_refl.x * weightG + mat4_refl.x * weightB;
                blended_refl.y = mat1_refl.y * weightBase + mat2_refl.y * weightR + mat3_refl.y * weightG + mat4_refl.y * weightB;
                blended_refl.z = mat1_refl.z * weightBase + mat2_refl.z * weightR + mat3_refl.z * weightG + mat4_refl.z * weightB;
                blended_refl.w = mat1_refl.w * weightBase + mat2_refl.w * weightR + mat3_refl.w * weightG + mat4_refl.w * weightB;

                m_face_reflectivity[&face] = blended_refl;
            }
        }
        Console_Printf("[Lightmapper] Reflectivity calculation complete.");
    }

    Vec3 Lightmapper::calculate_direct_sun_light_only(const Vec3& pos, const Vec3& normal) const
    {
        if (m_scene->sun.enabled) {
            Vec3 point_to_light_check = vec3_add(pos, vec3_muls(normal, SHADOW_BIAS));
            Vec3 light_dir = vec3_muls(m_scene->sun.direction, -1.0f);
            float NdotL = std::max(0.0f, vec3_dot(normal, light_dir));
            if (NdotL > 0.0f) {
                if (!is_in_shadow(point_to_light_check, vec3_add(point_to_light_check, vec3_muls(light_dir, 10000.0f)))) {
                    Vec3 light_color = vec3_muls(m_scene->sun.color, m_scene->sun.intensity);
                    return vec3_muls(light_color, NdotL);
                }
            }
        }
        return { 0,0,0 };
    }

    Vec3 Lightmapper::calculate_direct_light(const Vec3& pos, const Vec3& normal, Vec3& out_dominant_dir) const
    {
        Vec3 direct_light = { 0,0,0 };
        out_dominant_dir = { 0,0,0 };
        Vec3 point_to_light_check = vec3_add(pos, vec3_muls(normal, SHADOW_BIAS));

        if (m_scene->sun.enabled) {
            Vec3 light_dir = vec3_muls(m_scene->sun.direction, -1.0f);
            float NdotL = std::max(0.0f, vec3_dot(normal, light_dir));
            if (NdotL > 0.0f) {
                if (!is_in_shadow(point_to_light_check, vec3_add(point_to_light_check, vec3_muls(light_dir, 10000.0f)))) {
                    Vec3 light_color = vec3_muls(m_scene->sun.color, m_scene->sun.intensity);
                    Vec3 light_contribution = vec3_muls(light_color, NdotL);
                    direct_light = vec3_add(direct_light, light_contribution);

                    float contribution_magnitude = vec3_length(light_contribution);
                    out_dominant_dir = vec3_add(out_dominant_dir, vec3_muls(light_dir, contribution_magnitude));
                }
            }
        }

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

            float attenuation = powf(std::max(0.0f, 1.0f - dist / light.radius), 2.0f);
            attenuation /= (dist * dist + 1.0f);
            attenuation *= spotFactor;
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
            if (hit_face)
            {
                auto face_it = m_face_reflectivity.find(hit_face);
                if (face_it != m_face_reflectivity.end()) {
                    return face_it->second;
                }

                if (hit_face->material)
                {
                    auto mat_it = m_material_reflectivity.find(hit_face->material);
                    if (mat_it != m_material_reflectivity.end())
                    {
                        return mat_it->second;
                    }
                }
            }
            else if (primID < m_primID_to_material_map.size())
            {
                const Material* hit_material = m_primID_to_material_map[primID];
                if (hit_material)
                {
                    auto mat_it = m_material_reflectivity.find(hit_material);
                    if (mat_it != m_material_reflectivity.end())
                    {
                        return mat_it->second;
                    }
                }
            }
        }
        return { 0.5f, 0.5f, 0.5f, 1.0f };
    }

    Vec3 Lightmapper::calculate_indirect_light(const Vec3& origin, const Vec3& normal, std::mt19937& rng, Vec3& out_indirect_dir, int num_samples)
    {
        Vec3 accumulated_color = { 0, 0, 0 };
        out_indirect_dir = { 0, 0, 0 };
        std::uniform_real_distribution<float> roulette_dist(0.0f, 1.0f);

        if (m_bounces <= 0 || num_samples <= 0)
        {
            return accumulated_color;
        }

        constexpr int BATCH_SIZE = 16;
        int num_full_batches = num_samples / BATCH_SIZE;

        RTCIntersectArguments args;
        rtcInitIntersectArguments(&args);
        args.feature_mask = RTC_FEATURE_FLAG_NONE;

        for (int i = 0; i < num_full_batches; ++i)
        {
            RTCRayHit16 rayhit16;
            int valid[BATCH_SIZE];
            Vec3 batch_first_bounce_dir[BATCH_SIZE];
            Vec3 path_radiance[BATCH_SIZE];

            for (int k = 0; k < BATCH_SIZE; ++k)
            {
                valid[k] = -1;
                path_radiance[k] = { 0.0f, 0.0f, 0.0f };

                Vec3 bounce_dir = cosine_weighted_direction_in_hemisphere(normal, rng);
                batch_first_bounce_dir[k] = bounce_dir;

                Vec3 trace_origin = vec3_add(origin, vec3_muls(normal, SHADOW_BIAS));

                rayhit16.ray.org_x[k] = trace_origin.x;
                rayhit16.ray.org_y[k] = trace_origin.y;
                rayhit16.ray.org_z[k] = trace_origin.z;
                rayhit16.ray.dir_x[k] = bounce_dir.x;
                rayhit16.ray.dir_y[k] = bounce_dir.y;
                rayhit16.ray.dir_z[k] = bounce_dir.z;
                rayhit16.ray.tnear[k] = 0.0f;
                rayhit16.ray.tfar[k] = FLT_MAX;
                rayhit16.ray.mask[k] = -1;
                rayhit16.ray.flags[k] = 0;
                rayhit16.hit.geomID[k] = RTC_INVALID_GEOMETRY_ID;
            }

            rtcIntersect16(valid, m_rtc_scene, &rayhit16, &args);

            for (int k = 0; k < BATCH_SIZE; ++k)
            {
                if (rayhit16.hit.geomID[k] == RTC_INVALID_GEOMETRY_ID)
                {
                    continue;
                }

                unsigned int primID = rayhit16.hit.primID[k];

                if (primID < m_primID_to_face_map.size()) {
                    const BrushFace* hit_face = m_primID_to_face_map[primID];
                    if (hit_face && hit_face->material) {
                        auto it = m_emissive_materials.find(hit_face->material);
                        if (it != m_emissive_materials.end()) {
                            const Vec3& color = it->second.first;
                            const float intensity = it->second.second;
                            path_radiance[k] = vec3_muls(color, intensity);
                            continue;
                        }
                    }
                }

                Vec3 hit_pos = {
                    rayhit16.ray.org_x[k] + rayhit16.ray.tfar[k] * rayhit16.ray.dir_x[k],
                    rayhit16.ray.org_y[k] + rayhit16.ray.tfar[k] * rayhit16.ray.dir_y[k],
                    rayhit16.ray.org_z[k] + rayhit16.ray.tfar[k] * rayhit16.ray.dir_z[k]
                };

                Vec3 hit_normal = { rayhit16.hit.Ng_x[k], rayhit16.hit.Ng_y[k], rayhit16.hit.Ng_z[k] };
                vec3_normalize(&hit_normal);

                if (vec3_dot({ rayhit16.ray.dir_x[k], rayhit16.ray.dir_y[k], rayhit16.ray.dir_z[k] }, hit_normal) > 0.0f)
                {
                    continue;
                }

                Vec4 reflectivity_at_hit = get_reflectivity_at_hit(primID);
                Vec3 albedo = { reflectivity_at_hit.x, reflectivity_at_hit.y, reflectivity_at_hit.z };

                Vec3 dummy_dir;
                Vec3 direct_light = calculate_direct_light(hit_pos, hit_normal, dummy_dir);
                path_radiance[k] = vec3_mul(direct_light, albedo);
            }

            for (int k = 0; k < BATCH_SIZE; ++k)
            {
                accumulated_color = vec3_add(accumulated_color, path_radiance[k]);
                out_indirect_dir = vec3_add(out_indirect_dir, vec3_muls(batch_first_bounce_dir[k], vec3_length(path_radiance[k])));
            }
        }

        int remaining_samples = num_samples % BATCH_SIZE;
        for (int i = 0; i < remaining_samples; ++i)
        {
            Vec3 throughput = { 1.0f, 1.0f, 1.0f };
            Vec3 path_radiance = { 0, 0, 0 };

            Vec3 bounce_dir = cosine_weighted_direction_in_hemisphere(normal, rng);
            Vec3 trace_origin = vec3_add(origin, vec3_muls(normal, SHADOW_BIAS));

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

            rtcIntersect1(m_rtc_scene, &rayhit, &args);

            if (rayhit.hit.geomID != RTC_INVALID_GEOMETRY_ID)
            {
                if (rayhit.hit.primID < m_primID_to_face_map.size()) {
                    const BrushFace* hit_face = m_primID_to_face_map[rayhit.hit.primID];
                    if (hit_face && hit_face->material) {
                        auto it = m_emissive_materials.find(hit_face->material);
                        if (it != m_emissive_materials.end()) {
                            const Vec3& color = it->second.first;
                            const float intensity = it->second.second;
                            path_radiance = vec3_add(path_radiance, vec3_mul(vec3_muls(color, intensity), throughput));
                        }
                    }
                }
                if (vec3_length_sq(path_radiance) == 0.0f)
                {
                    Vec3 hit_pos = {
                        trace_origin.x + rayhit.ray.tfar * bounce_dir.x,
                        trace_origin.y + rayhit.ray.tfar * bounce_dir.y,
                        trace_origin.z + rayhit.ray.tfar * bounce_dir.z
                    };
                    Vec3 hit_normal = { rayhit.hit.Ng_x, rayhit.hit.Ng_y, rayhit.hit.Ng_z };
                    vec3_normalize(&hit_normal);
                    if (vec3_dot(bounce_dir, hit_normal) <= 0.0f)
                    {
                        Vec4 reflectivity_at_hit = get_reflectivity_at_hit(rayhit.hit.primID);
                        Vec3 albedo = { reflectivity_at_hit.x, reflectivity_at_hit.y, reflectivity_at_hit.z };
                        Vec3 dummy_dir;
                        Vec3 direct_light = calculate_direct_light(hit_pos, hit_normal, dummy_dir);
                        path_radiance = vec3_add(path_radiance, vec3_mul(vec3_mul(direct_light, albedo), throughput));
                    }
                }
            }
            accumulated_color = vec3_add(accumulated_color, path_radiance);
            out_indirect_dir = vec3_add(out_indirect_dir, vec3_muls(bounce_dir, vec3_length(path_radiance)));
        }

        if (vec3_length_sq(out_indirect_dir) > 0.0001f)
        {
            vec3_normalize(&out_indirect_dir);
        }

        return vec3_muls(accumulated_color, 1.0f / (float)num_samples);
    }

    void Lightmapper::generate()
    {
        Console_Printf("[Lightmapper] Starting lightmap generation...");
        auto start_time = std::chrono::high_resolution_clock::now();
        m_scene->lightmapResolution = m_resolution;

        precalculate_material_reflectivity();
        prepare_jobs();
        if (m_jobs.empty()) return;

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

        generate_ambient_probes();

        for (int i = 0; i < m_scene->numObjects; ++i)
        {
            const SceneObject& obj = m_scene->objects[i];
            if (!obj.model || !m_model_color_buffers[i] || !m_model_direction_buffers[i]) continue;

            std::string model_name_str = (strlen(obj.targetname) > 0) ? obj.targetname : "Model_" + std::to_string(i);
            std::string sanitized_name = sanitize_filename(model_name_str);
            fs::path model_dir = m_output_path / sanitized_name;
            fs::create_directories(model_dir);

            fs::path vlm_path = model_dir / "vertex_colors.vlm";
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

            fs::path vld_path = model_dir / "vertex_directions.vld";
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