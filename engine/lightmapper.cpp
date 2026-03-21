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
#ifdef ARCH_64BIT
#include "lightmapper.h"
#include "lightmapper_misc.h"
#include "lightmapper_internal.h"
#include "gl_console.h"
#include "math_lib.h"
#include "map_misc.h"
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
#include "xatlas.h"

namespace
{
    namespace fs = filesystem;

    void embree_error_function(void* userPtr, RTCError error, const Char* str)
    {
        Console::Printf_Error("[Embree] Error %d: %s", error, str);
    }

    void oidn_error_function(void* userPtr, OIDNError error, const Char* message)
    {
        Console::Printf_Error("[OIDN] Error %d: %s", error, message);
    }

    struct BrushFaceJobData
    {
        Int brush_index;
        Int face_index;
        fs::path output_dir;
    };

    struct DecalJobData
    {
        Int decal_index;
        fs::path output_dir;
    };

    struct BrushVertexJobData
    {
        Int brush_index;
        Uint vertex_index;
        Vec4* output_color_buffer;
        Vec4* output_direction_buffer;
    };

    struct ModelVertexJobData
    {
        Int model_index;
        fs::path output_dir;
    };

    struct ModelLightmapJobData
    {
        Int model_index;
        fs::path output_dir;
    };

    struct EmissiveMaterial {
        const Material* material;
        Vec3 color;
        Float intensity;
    };

    using JobPayload = variant<BrushFaceJobData, ModelVertexJobData, DecalJobData, BrushVertexJobData, ModelLightmapJobData>;

    static Bool IsBrushBakeable(const Brush& b)
    {
        const Char* disallowed[] = {
            "func_button",
            "func_clip",
            "func_conveyor",
            "func_friction",
            "func_ladder",
            "env_glass",
            "env_reflectionprobe",
            "trigger_autosave",
            "trigger_dspzone",
            "trigger_gravity",
            "trigger_multiple",
			"trigger_once",
            "trigger_teleport",
            "trigger_hurt",
        };

        if (strlen(b.classname) > 0) {
            for (Int i = 0; i < sizeof(disallowed) / sizeof(disallowed[0]); i++) {
                if (strcmp(b.classname, disallowed[i]) == 0) {
                    return false;
                }
            }
        }
        return true;
    }

    class Lightmapper
    {
    public:
        Lightmapper(Scene* scene, Int resolution, Int bounces);
        ~Lightmapper();
        void generate();

    private:
        enum DirectLightMode { BAKE_ALL_FOR_BOUNCES, BAKE_STATIC_DIRECT_ONLY };

        void build_embree_scene();
        void generate_ambient_probes();
        void prepare_jobs();
        void worker_main();
        void process_job(const JobPayload& job);
        void process_brush_face(const BrushFaceJobData& data);
        void process_decal(const DecalJobData& data);
        void process_model_vertex(const ModelVertexJobData& data);
        void process_model_lightmap(const ModelLightmapJobData& data);

        Vec3 calculate_direct_light(const Vec3& pos, const Vec3& normal, Vec3& out_dominant_dir, DirectLightMode mode) const;
        Vec3 calculate_direct_sun_light_only(const Vec3& pos, const Vec3& normal) const;
        Vec3 calculate_indirect_light(const Vec3& origin, const Vec3& normal, mt19937& rng, Vec3& out_indirect_dir, Int num_samples);
        Vec4 get_reflectivity_at_hit(Uint primID) const;

        void precalculate_material_reflectivity();
        Bool is_in_shadow(const Vec3& start, const Vec3& end) const;
        static Vec3 cosine_weighted_direction_in_hemisphere(const Vec3& normal, mt19937& gen);

        Scene* m_scene;
        Int m_resolution;
        Int m_bounces;
        fs::path m_output_path;

        RTCDevice m_rtc_device;
        RTCScene m_rtc_scene;
        OIDNDevice m_oidn_device;

        vector<JobPayload> m_jobs;
        atomic<Usize> m_next_job_index{ 0 };

        vector<unique_ptr<Vec4[]>> m_model_color_buffers;
        vector<unique_ptr<Vec4[]>> m_model_direction_buffers;
        vector<unique_ptr<Vec4[]>> m_brush_color_buffers;
        vector<unique_ptr<Vec4[]>> m_brush_direction_buffers;
        map<const Material*, Vec4> m_material_reflectivity;
        map<const BrushFace*, Vec4> m_face_reflectivity;
        vector<const BrushFace*> m_primID_to_face_map;
        vector<const Brush*> m_primID_to_brush_map;
        vector<const Material*> m_primID_to_material_map;
    };

    Lightmapper::Lightmapper(Scene* scene, Int resolution, Int bounces)
        : m_scene(scene), m_resolution(resolution), m_bounces(bounces), m_rtc_device(nullptr), m_rtc_scene(nullptr)
    {
        m_rtc_device = rtcNewDevice(nullptr);
        if (!m_rtc_device)
        {
            Console::Printf_Error("Failed to create Embree device.");
        }
        rtcSetDeviceErrorFunction(m_rtc_device, embree_error_function, nullptr);
        build_embree_scene();
        m_oidn_device = oidnNewDevice(OIDN_DEVICE_TYPE_CPU);
        if (!m_oidn_device)
        {
            Console::Printf_Error("Failed to create OIDN device.");
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

        vector<Vec3> all_vertices;
        vector<Uint> all_indices;
        m_primID_to_face_map.clear();
        m_primID_to_brush_map.clear();

        for (Int i = 0; i < m_scene->numBrushes; ++i)
        {
            const Brush& b = m_scene->brushes[i];
            if (strcmp(b.classname, "func_water") == 0) {
                continue;
            }
            if (!IsBrushBakeable(b) || !b.casts_shadows) continue;

            for (Int j = 0; j < b.numFaces; ++j)
            {
                const BrushFace& face = b.faces[j];
                if (face.numVertexIndices < 3) continue;

                for (Int k = 0; k < face.numVertexIndices - 2; ++k)
                {
                    Uint base_index = static_cast<Uint>(all_vertices.size());
                    all_vertices.push_back(Math::mat4_mul_vec3(&b.modelMatrix, b.vertices[face.vertexIndices[0]].pos));
                    all_vertices.push_back(Math::mat4_mul_vec3(&b.modelMatrix, b.vertices[face.vertexIndices[k + 1]].pos));
                    all_vertices.push_back(Math::mat4_mul_vec3(&b.modelMatrix, b.vertices[face.vertexIndices[k + 2]].pos));
                    all_indices.push_back(base_index);
                    all_indices.push_back(base_index + 1);
                    all_indices.push_back(base_index + 2);
                    m_primID_to_face_map.push_back(&face);
                    m_primID_to_brush_map.push_back(&b);
                }
            }
        }

        for (Int i = 0; i < m_scene->numObjects; ++i)
        {
            const SceneObject& obj = m_scene->objects[i];
            if (obj.mass > 0.0f || !obj.casts_shadows) continue;
            if (!obj.model || !obj.model->combinedIndexData) continue;
            for (Int mesh_idx = 0; mesh_idx < obj.model->meshCount; ++mesh_idx) {
                const Mesh& mesh = obj.model->meshes[mesh_idx];
                Usize num_primitives = mesh.indexCount / 3;
                for (Usize k = 0; k < num_primitives; ++k) {
                    m_primID_to_material_map.push_back(mesh.material);
                }
            }

            for (Uint j = 0; j < obj.model->totalIndexCount; j += 3)
            {
                Uint base_index = static_cast<Uint>(all_vertices.size());
                Uint i0 = obj.model->combinedIndexData[j];
                Uint i1 = obj.model->combinedIndexData[j + 1];
                Uint i2 = obj.model->combinedIndexData[j + 2];
                Vec3 v0 = { obj.model->combinedVertexData[i0 * 3 + 0], obj.model->combinedVertexData[i0 * 3 + 1], obj.model->combinedVertexData[i0 * 3 + 2] };
                Vec3 v1 = { obj.model->combinedVertexData[i1 * 3 + 0], obj.model->combinedVertexData[i1 * 3 + 1], obj.model->combinedVertexData[i1 * 3 + 2] };
                Vec3 v2 = { obj.model->combinedVertexData[i2 * 3 + 0], obj.model->combinedVertexData[i2 * 3 + 1], obj.model->combinedVertexData[i2 * 3 + 2] };
                all_vertices.push_back(Math::mat4_mul_vec3(&obj.modelMatrix, v0));
                all_vertices.push_back(Math::mat4_mul_vec3(&obj.modelMatrix, v1));
                all_vertices.push_back(Math::mat4_mul_vec3(&obj.modelMatrix, v2));
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
        Uint* indices_buf = (Uint*)rtcSetNewGeometryBuffer(geom, RTC_BUFFER_TYPE_INDEX, 0, RTC_FORMAT_UINT3, 3 * sizeof(Uint), all_indices.size() / 3);
        memcpy(indices_buf, all_indices.data(), all_indices.size() * sizeof(Uint));

        rtcCommitGeometry(geom);
        rtcAttachGeometry(m_rtc_scene, geom);
        rtcReleaseGeometry(geom);
        rtcCommitScene(m_rtc_scene);
    }

    void Lightmapper::generate_ambient_probes()
    {
        Console::Printf("[Lightmapper] Generating ambient probes...");

        vector<Vec3> probe_positions;
        const Float probe_spacing = 1.0f;

        for (Int i = 0; i < m_scene->numBrushes; ++i) {
            const Brush& b = m_scene->brushes[i];
            if (!IsBrushBakeable(b) || b.numVertices == 0) continue;

            Vec3 min_aabb, max_aabb;
            Brush_GetWorldAABB(&b, &min_aabb, &max_aabb);

            for (Float x = min_aabb.x; x <= max_aabb.x; x += probe_spacing) {
                for (Float y = min_aabb.y; y <= max_aabb.y; y += probe_spacing) {
                    for (Float z = min_aabb.z; z <= max_aabb.z; z += probe_spacing) {

                        Vec3 probe_pos = { x, y, z };
                        mt19937 validation_rng(generate_seed_from_pos(probe_pos));

                        constexpr Int validation_rays = 16;
                        constexpr Float validation_distance = 0.5f;
                        Int hits = 0;

                        for (Int k = 0; k < validation_rays; ++k)
                        {
                            uniform_real_distribution<Float> dist(-1.0f, 1.0f);
                            Vec3 ray_dir = { dist(validation_rng), dist(validation_rng), dist(validation_rng) };
                            Math::vec3_normalize(&ray_dir);

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

                        if (static_cast<Float>(hits) / validation_rays > 0.25f)
                        {
                            continue;
                        }

                        probe_positions.push_back(probe_pos);
                    }
                }
            }
        }

        if (probe_positions.empty()) {
            Console::Printf("[Lightmapper] No suitable locations for ambient probes found.");
            return;
        }

        Console::Printf("[Lightmapper] Placing %zu validated ambient probes.", probe_positions.size());

        m_scene->num_ambient_probes = probe_positions.size();
        m_scene->ambient_probes = new AmbientProbe[m_scene->num_ambient_probes];

        for (Usize i = 0; i < probe_positions.size(); ++i) {
            m_scene->ambient_probes[i].position = probe_positions[i];
            Vec3 dominant_dir_total = { 0,0,0 };
            mt19937 lighting_rng(generate_seed_from_pos(probe_positions[i]));

            Vec3 directions[6] = { {1,0,0}, {-1,0,0}, {0,1,0}, {0,-1,0}, {0,0,1}, {0,0,-1} };
            for (Int j = 0; j < 6; ++j) {
                Vec3 direct_dir, indirect_dir;
                Vec3 direct_light = calculate_direct_light(probe_positions[i], directions[j], direct_dir, BAKE_ALL_FOR_BOUNCES);
                Vec3 indirect_light = calculate_indirect_light(probe_positions[i], directions[j], lighting_rng, indirect_dir, INDIRECT_SAMPLES_PER_POINT_AMBIENT_PROBES);
                m_scene->ambient_probes[i].colors[j] = Math::vec3_muls(Math::vec3_add(direct_light, indirect_light), 2.2f);
                dominant_dir_total = Math::vec3_add(dominant_dir_total, Math::vec3_add(direct_dir, indirect_dir));
            }

            if (Math::vec3_length_sq(dominant_dir_total) > 0.001f) {
                Math::vec3_normalize(&dominant_dir_total);
            }
            m_scene->ambient_probes[i].dominant_direction = dominant_dir_total;
        }

        fs::path probe_path = m_output_path / "ambient_probes.amp";
        ofstream probe_file(probe_path, ios::binary);
        if (probe_file) {
            const Char header[] = "AMBI";
            probe_file.write(header, 4);
            probe_file.write(reinterpret_cast<const Char*>(&m_scene->num_ambient_probes), sizeof(Int));
            probe_file.write(reinterpret_cast<const Char*>(m_scene->ambient_probes), sizeof(AmbientProbe) * m_scene->num_ambient_probes);
            Console::Printf("[Lightmapper] Saved %d ambient probes.", m_scene->num_ambient_probes);
        }
        else {
            Console::Printf_Error("[Lightmapper] Failed to save ambient probes file.");
        }
        delete[] m_scene->ambient_probes;
        m_scene->ambient_probes = nullptr;
        m_scene->num_ambient_probes = 0;
    }

    Bool Lightmapper::is_in_shadow(const Vec3& start, const Vec3& end) const
    {
        if (!m_rtc_scene) return false;

        Vec3 ray_dir = Math::vec3_sub(end, start);
        const Float max_dist = Math::vec3_length(ray_dir);
        if (max_dist < SHADOW_BIAS) return false;
        Math::vec3_normalize(&ray_dir);

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

        Float light_transmission = 1.0f;

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

    static Vec2 calculate_texture_uv_for_vertex(const Brush* b, Int face_index, Int vertex_index) {
        const BrushFace& face = b->faces[face_index];
        Vec3 local_pos = b->vertices[vertex_index].pos;

        Vec3 world_pos = Math::mat4_mul_vec3(&b->modelMatrix, local_pos);

        Vec3 p0 = Math::mat4_mul_vec3(&b->modelMatrix, b->vertices[face.vertexIndices[0]].pos);
        Vec3 p1 = Math::mat4_mul_vec3(&b->modelMatrix, b->vertices[face.vertexIndices[1]].pos);
        Vec3 p2 = Math::mat4_mul_vec3(&b->modelMatrix, b->vertices[face.vertexIndices[2]].pos);
        Vec3 world_normal = Math::vec3_cross(Math::vec3_sub(p1, p0), Math::vec3_sub(p2, p0));
        Math::vec3_normalize(&world_normal);

        Float absX = fabsf(world_normal.x);
        Float absY = fabsf(world_normal.y);
        Float absZ = fabsf(world_normal.z);
        Int dominant_axis = (absY > absX && absY > absZ) ? 1 : ((absX > absZ) ? 0 : 2);

        Float u, v;
        if (dominant_axis == 0) {
            u = world_pos.y;
            v = world_pos.z;
        }
        else if (dominant_axis == 1) {
            u = world_pos.x;
            v = world_pos.z;
        }
        else {
            u = world_pos.x;
            v = world_pos.y;
        }

        Float rad = face.uv_rotation * (Common::PI / 180.0f);
        Float cos_r = cosf(rad);
        Float sin_r = sinf(rad);

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

        Vec2 min_uv = { FLT_MAX, FLT_MAX };
        Vec2 max_uv = { -FLT_MAX, -FLT_MAX };
        vector<Vec3> world_verts(face.numVertexIndices);
        for (Int k = 0; k < face.numVertexIndices; ++k) {
            world_verts[k] = Math::mat4_mul_vec3(&b.modelMatrix, b.vertices[face.vertexIndices[k]].pos);
            Vec2 uv = calculate_texture_uv_for_vertex(&b, data.face_index, face.vertexIndices[k]);
            min_uv.x = min(min_uv.x, uv.x);
            min_uv.y = min(min_uv.y, uv.y);
            max_uv.x = max(max_uv.x, uv.x);
            max_uv.y = max(max_uv.y, uv.y);
        }

        Vec2 uv_range = { max(0.001f, max_uv.x - min_uv.x), max(0.001f, max_uv.y - min_uv.y) };
        Float u_range = uv_range.x * face.uv_scale.x;
        Float v_range = uv_range.y * face.uv_scale.y;

        Float effective_luxels_per_unit = LUXELS_PER_UNIT / face.lightmap_scale;
        Int lightmap_width = clamp(static_cast<Int>(ceilf(u_range * effective_luxels_per_unit)), 4, m_resolution);
        Int lightmap_height = clamp(static_cast<Int>(ceilf(v_range * effective_luxels_per_unit)), 4, m_resolution);

        vector<Float> direct_lightmap_data(lightmap_width * lightmap_height * 3);
        vector<Float> indirect_lightmap_data(lightmap_width * lightmap_height * 3);
        vector<Float> albedo_lightmap_data(lightmap_width * lightmap_height * 3);
        vector<Float> normal_lightmap_data(lightmap_width * lightmap_height * 3);
        vector<Float> direction_float_data(lightmap_width * lightmap_height * 3);
        vector<Uchar> dir_lightmap_data(lightmap_width * lightmap_height * 4, 0);

        Vec4 face_reflectivity_v4 = { 0.5f, 0.5f, 0.5f, 1.0f };
        if (face.material) {
            auto it = m_material_reflectivity.find(face.material);
            if (it != m_material_reflectivity.end()) {
                face_reflectivity_v4 = it->second;
            }
        }

        for (Int y = 0; y < lightmap_height; ++y)
        {
            for (Int x = 0; x < lightmap_width; ++x)
            {
                Vec3 direct_light_color = { 0, 0, 0 };
                Vec3 indirect_light_color = { 0, 0, 0 };
                Vec3 accumulated_direction = { 0, 0, 0 };
                Vec3 indirect_direction = { 0, 0, 0 };

                Float u_tex = (static_cast<Float>(x) + 0.5f) / lightmap_width;
                Float v_tex = (static_cast<Float>(y) + 0.5f) / lightmap_height;
                Float target_u = min_uv.x + u_tex * uv_range.x;
                Float target_v = min_uv.y + v_tex * uv_range.y;

                Vec3 world_pos;
                Bool inside = false;
                Vec3 point_normal = { 0.0f, 0.0f, 1.0f };

                for (Int k = 0; k < face.numVertexIndices - 2; ++k)
                {
                    Int idx0 = face.vertexIndices[0];
                    Int idx1 = face.vertexIndices[k + 1];
                    Int idx2 = face.vertexIndices[k + 2];
                    Vec2 uv0 = calculate_texture_uv_for_vertex(&b, data.face_index, idx0);
                    Vec2 uv1 = calculate_texture_uv_for_vertex(&b, data.face_index, idx1);
                    Vec2 uv2 = calculate_texture_uv_for_vertex(&b, data.face_index, idx2);
                    Vec3 p0 = world_verts[0], p1 = world_verts[k + 1], p2 = world_verts[k + 2];

                    Vec3 barycentric = Math::barycentric_coords({ target_u, target_v }, uv0, uv1, uv2);

                    if (barycentric.x >= -1e-4f && barycentric.y >= -1e-4f && barycentric.z >= -1e-4f) {
                        inside = true;
                        world_pos = Math::vec3_add(Math::vec3_muls(p0, barycentric.x), Math::vec3_add(Math::vec3_muls(p1, barycentric.y), Math::vec3_muls(p2, barycentric.z)));
                        Vec3 tri_normal = Math::vec3_cross(Math::vec3_sub(p1, p0), Math::vec3_sub(p2, p0));
                        Math::vec3_normalize(&tri_normal);
                        point_normal = tri_normal;
                        break;
                    }
                }

                Int hdr_idx = (y * lightmap_width + x) * 3;

                if (inside)
                {
                    mt19937 rng(generate_seed_from_pos(world_pos));
                    direct_light_color = calculate_direct_light(world_pos, point_normal, accumulated_direction, BAKE_STATIC_DIRECT_ONLY);
                    indirect_light_color = calculate_indirect_light(world_pos, point_normal, rng, indirect_direction, INDIRECT_SAMPLES_PER_POINT_BRUSHES);
                    accumulated_direction = Math::vec3_add(accumulated_direction, indirect_direction);
                    normal_lightmap_data[hdr_idx + 0] = point_normal.x;
                    normal_lightmap_data[hdr_idx + 1] = point_normal.y;
                    normal_lightmap_data[hdr_idx + 2] = point_normal.z;
                }
                else
                {
                    normal_lightmap_data[hdr_idx + 0] = 0.0f;
                    normal_lightmap_data[hdr_idx + 1] = 0.0f;
                    normal_lightmap_data[hdr_idx + 2] = 0.0f;
                }

                if (Math::vec3_length_sq(accumulated_direction) > BLACK_THRESHOLD) Math::vec3_normalize(&accumulated_direction);
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

        vector<Float> denoised_indirect_data(lightmap_width * lightmap_height * 3);
        Float indirect_sum = 0.0f;
        for (Float val : indirect_lightmap_data) {
            indirect_sum += val;
        }

        if (indirect_sum > BLACK_THRESHOLD)
        {
            Usize pixelStride = sizeof(Float) * 3;
            Usize rowStride = pixelStride * lightmap_width;

            OIDNFilter filter = oidnNewFilter(m_oidn_device, "RTLightmap");

            oidnSetSharedFilterImage(filter, "color", indirect_lightmap_data.data(), OIDN_FORMAT_FLOAT3, lightmap_width, lightmap_height, 0, pixelStride, rowStride);
            oidnSetSharedFilterImage(filter, "albedo", albedo_lightmap_data.data(), OIDN_FORMAT_FLOAT3, lightmap_width, lightmap_height, 0, pixelStride, rowStride);
            oidnSetSharedFilterImage(filter, "normal", normal_lightmap_data.data(), OIDN_FORMAT_FLOAT3, lightmap_width, lightmap_height, 0, pixelStride, rowStride);
            oidnSetSharedFilterImage(filter, "output", denoised_indirect_data.data(), OIDN_FORMAT_FLOAT3, lightmap_width, lightmap_height, 0, pixelStride, rowStride);

            oidnSetFilterBool(filter, "hdr", true);
            oidnSetFilterBool(filter, "cleanAux", true);
            oidnCommitFilter(filter);
            oidnExecuteFilter(filter);

            const Char* errorMessage;
            if (oidnGetDeviceError(m_oidn_device, &errorMessage) != OIDN_ERROR_NONE)
                Console::Printf_Error("[OIDN] Filter execution error: %s", errorMessage);

            oidnReleaseFilter(filter);
        }
        else
        {
            fill(denoised_indirect_data.begin(), denoised_indirect_data.end(), 0.0f);
        }

        vector<Float> final_hdr_lightmap_data(lightmap_width * lightmap_height * 3);
        for (Int y = 0; y < lightmap_height; ++y) {
            for (Int x = 0; x < lightmap_width; ++x) {
                Int idx = (y * lightmap_width + x) * 3;
                Vec3 direct_light = { direct_lightmap_data[idx], direct_lightmap_data[idx + 1], direct_lightmap_data[idx + 2] };
                Vec3 indirect_light = { denoised_indirect_data[idx], denoised_indirect_data[idx + 1], denoised_indirect_data[idx + 2] };

                Float u_tex = (static_cast<Float>(x) + 0.5f) / lightmap_width;
                Float v_tex = (static_cast<Float>(y) + 0.5f) / lightmap_height;
                Float target_u = min_uv.x + u_tex * uv_range.x;
                Float target_v = min_uv.y + v_tex * uv_range.y;
                Vec3 world_pos;
                Bool inside = false;
                Vec3 point_normal = { 0.0f, 0.0f, 1.0f };

                for (Int k = 0; k < face.numVertexIndices - 2; ++k)
                {
                    Int idx0 = face.vertexIndices[0];
                    Int idx1 = face.vertexIndices[k + 1];
                    Int idx2 = face.vertexIndices[k + 2];
                    Vec2 uv0 = calculate_texture_uv_for_vertex(&b, data.face_index, idx0);
                    Vec2 uv1 = calculate_texture_uv_for_vertex(&b, data.face_index, idx1);
                    Vec2 uv2 = calculate_texture_uv_for_vertex(&b, data.face_index, idx2);
                    Vec3 p0 = world_verts[0], p1 = world_verts[k + 1], p2 = world_verts[k + 2];
                    Vec3 barycentric = Math::barycentric_coords({ target_u, target_v }, uv0, uv1, uv2);
                    if (barycentric.x >= -1e-4f && barycentric.y >= -1e-4f && barycentric.z >= -1e-4f) {
                        inside = true;
                        world_pos = Math::vec3_add(Math::vec3_muls(p0, barycentric.x), Math::vec3_add(Math::vec3_muls(p1, barycentric.y), Math::vec3_muls(p2, barycentric.z)));
                        Vec3 tri_normal = Math::vec3_cross(Math::vec3_sub(p1, p0), Math::vec3_sub(p2, p0));
                        Math::vec3_normalize(&tri_normal);
                        point_normal = tri_normal;
                        break;
                    }
                }

                Vec3 direct_sun_light = { 0,0,0 };
                if (inside) {
                    direct_sun_light = calculate_direct_sun_light_only(world_pos, point_normal);
                }

                Vec3 final_light = Math::vec3_add(direct_light, indirect_light);
                final_light = Math::vec3_add(final_light, direct_sun_light);
                final_hdr_lightmap_data[idx] = final_light.x;
                final_hdr_lightmap_data[idx + 1] = final_light.y;
                final_hdr_lightmap_data[idx + 2] = final_light.z;
            }
        }

        vector<Float> filtered_direction_data;
        apply_guided_filter(filtered_direction_data, direction_float_data, final_hdr_lightmap_data, lightmap_width, lightmap_height, 4, 0.01f);

        for (Int i = 0; i < lightmap_width * lightmap_height; ++i) {
            Int idx3 = i * 3;
            Vec3 dir = { filtered_direction_data[idx3], filtered_direction_data[idx3 + 1], filtered_direction_data[idx3 + 2] };
            if (Math::vec3_length_sq(dir) > BLACK_THRESHOLD) {
                Math::vec3_normalize(&dir);
            }
            else {
                dir = { 0,0,0 };
            }

            Int idx4 = i * 4;
            dir_lightmap_data[idx4 + 0] = static_cast<Uchar>((dir.x * 0.5f + 0.5f) * 255.0f);
            dir_lightmap_data[idx4 + 1] = static_cast<Uchar>((dir.y * 0.5f + 0.5f) * 255.0f);
            dir_lightmap_data[idx4 + 2] = static_cast<Uchar>((dir.z * 0.5f + 0.5f) * 255.0f);
            dir_lightmap_data[idx4 + 3] = 255;
        }

        apply_gaussian_blur(final_hdr_lightmap_data, lightmap_width, lightmap_height, 3);

        Int padded_width = lightmap_width + Common::LIGHTMAPPADDING * 2;
        Int padded_height = lightmap_height + Common::LIGHTMAPPADDING * 2;

        vector<Float> padded_hdr_data(padded_width * padded_height * 3);
        for (Int y = 0; y < padded_height; ++y) {
            for (Int x = 0; x < padded_width; ++x) {
                Int src_x = clamp(x - Common::LIGHTMAPPADDING, 0, lightmap_width - 1);
                Int src_y = clamp(y - Common::LIGHTMAPPADDING, 0, lightmap_height - 1);
                for (Int c = 0; c < 3; ++c) {
                    padded_hdr_data[(y * padded_width + x) * 3 + c] = final_hdr_lightmap_data[(src_y * lightmap_width + src_x) * 3 + c];
                }
            }
        }

        vector<Uchar> padded_dir_data(padded_width * padded_height * 4);
        for (Int y = 0; y < padded_height; ++y) {
            for (Int x = 0; x < padded_width; ++x) {
                Int src_x = clamp(x - Common::LIGHTMAPPADDING, 0, lightmap_width - 1);
                Int src_y = clamp(y - Common::LIGHTMAPPADDING, 0, lightmap_height - 1);
                for (Int c = 0; c < 4; ++c) {
                    padded_dir_data[(y * padded_width + x) * 4 + c] = dir_lightmap_data[(src_y * lightmap_width + src_x) * 4 + c];
                }
            }
        }

        fs::path color_path = data.output_dir / ("face_" + to_string(data.face_index) + "_color.hdr");
        stbi_write_hdr(color_path.string().c_str(), padded_width, padded_height, 3, padded_hdr_data.data());
        fs::path dir_path = data.output_dir / ("face_" + to_string(data.face_index) + "_dir.png");
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
        Float scale = (decal.lightmap_scale > 0.0f) ? decal.lightmap_scale : 1.0f;
        Int lightmap_res = clamp(static_cast<Int>(m_resolution / scale), 4, 4096);

        Mat4 transform = Math::create_trs_matrix(decal.pos, decal.rot, decal.size);
        Vec3 x_axis = { transform.m[0], transform.m[1], transform.m[2] };
        Vec3 y_axis = { transform.m[4], transform.m[5], transform.m[6] };
        Vec3 z_axis = { transform.m[8], transform.m[9], transform.m[10] };
        Vec3 normal = { transform.m[8], transform.m[9], transform.m[10] };
        Math::vec3_normalize(&normal);

        vector<Float> direct_lightmap_data(lightmap_res * lightmap_res * 3);
        vector<Float> indirect_lightmap_data(lightmap_res * lightmap_res * 3);
        vector<Float> albedo_lightmap_data(lightmap_res * lightmap_res * 3);
        vector<Float> normal_lightmap_data(lightmap_res * lightmap_res * 3);
        vector<Float> direction_float_data(lightmap_res * lightmap_res * 3);

        Vec4 decal_reflectivity = { 0.5f, 0.5f, 0.5f, 1.0f };
        if (decal.material) {
            auto it = m_material_reflectivity.find(decal.material);
            if (it != m_material_reflectivity.end()) {
                decal_reflectivity = it->second;
            }
        }

        for (Int y = 0; y < lightmap_res; ++y) {
            for (Int x = 0; x < lightmap_res; ++x) {
                Float u = (static_cast<Float>(x) + 0.5f) / lightmap_res;
                Float v = (static_cast<Float>(y) + 0.5f) / lightmap_res;

                Float local_x = u - 0.5f;
                Float local_y = 0.5f - v;

                Vec3 local_pos_on_quad = Math::vec3_add(Math::vec3_muls(x_axis, local_x), Math::vec3_muls(y_axis, local_y));
                local_pos_on_quad = Math::vec3_add(local_pos_on_quad, Math::vec3_muls(z_axis, -0.5f));
                Vec3 world_pos = Math::vec3_add(decal.pos, local_pos_on_quad);

                Vec3 sampling_pos = Math::vec3_add(world_pos, Math::vec3_muls(normal, SHADOW_BIAS));

                mt19937 rng(generate_seed_from_pos(world_pos));
                Vec3 dominant_dir = { 0,0,0 }, indirect_dir = { 0,0,0 };
                Vec3 direct_light = calculate_direct_light(sampling_pos, normal, dominant_dir, BAKE_STATIC_DIRECT_ONLY);
                Vec3 indirect_light = calculate_indirect_light(sampling_pos, normal, rng, indirect_dir, INDIRECT_SAMPLES_PER_POINT_DECALS);

                Int idx = (y * lightmap_res + x) * 3;

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

                Vec3 total_dir = Math::vec3_add(dominant_dir, indirect_dir);
                if (Math::vec3_length_sq(total_dir) > 0.0001f) Math::vec3_normalize(&total_dir);
                direction_float_data[idx + 0] = total_dir.x;
                direction_float_data[idx + 1] = total_dir.y;
                direction_float_data[idx + 2] = total_dir.z;
            }
        }

        vector<Float> denoised_indirect_data(lightmap_res * lightmap_res * 3);
        Float indirect_sum = 0.0f; for (Float val : indirect_lightmap_data) indirect_sum += val;
        if (indirect_sum > BLACK_THRESHOLD) {
            OIDNFilter filter = oidnNewFilter(m_oidn_device, "RTLightmap");
            oidnSetSharedFilterImage(filter, "color", indirect_lightmap_data.data(), OIDN_FORMAT_FLOAT3, lightmap_res, lightmap_res, 0, sizeof(Float) * 3, sizeof(Float) * 3 * lightmap_res);
            oidnSetSharedFilterImage(filter, "albedo", albedo_lightmap_data.data(), OIDN_FORMAT_FLOAT3, lightmap_res, lightmap_res, 0, sizeof(Float) * 3, sizeof(Float) * 3 * lightmap_res);
            oidnSetSharedFilterImage(filter, "normal", normal_lightmap_data.data(), OIDN_FORMAT_FLOAT3, lightmap_res, lightmap_res, 0, sizeof(Float) * 3, sizeof(Float) * 3 * lightmap_res);
            oidnSetSharedFilterImage(filter, "output", denoised_indirect_data.data(), OIDN_FORMAT_FLOAT3, lightmap_res, lightmap_res, 0, sizeof(Float) * 3, sizeof(Float) * 3 * lightmap_res);
            oidnSetFilterBool(filter, "hdr", true);
            oidnSetFilterBool(filter, "cleanAux", true);
            oidnCommitFilter(filter);
            oidnExecuteFilter(filter);
            oidnReleaseFilter(filter);
        }
        else {
            fill(denoised_indirect_data.begin(), denoised_indirect_data.end(), 0.0f);
        }

        vector<Float> final_hdr_lightmap_data(lightmap_res * lightmap_res * 3);
        for (Usize i = 0; i < final_hdr_lightmap_data.size() / 3; ++i) {
            Vec3 direct_light = { direct_lightmap_data[i * 3], direct_lightmap_data[i * 3 + 1], direct_lightmap_data[i * 3 + 2] };
            Vec3 indirect_light = { denoised_indirect_data[i * 3], denoised_indirect_data[i * 3 + 1], denoised_indirect_data[i * 3 + 2] };
            Vec3 direct_sun_light = calculate_direct_sun_light_only(decal.pos, normal);
            Vec3 final_light = Math::vec3_add(Math::vec3_add(direct_light, direct_sun_light), indirect_light);
            final_hdr_lightmap_data[i * 3] = final_light.x;
            final_hdr_lightmap_data[i * 3 + 1] = final_light.y;
            final_hdr_lightmap_data[i * 3 + 2] = final_light.z;
        }

        vector<Float> filtered_direction_data;
        apply_guided_filter(filtered_direction_data, direction_float_data, final_hdr_lightmap_data, lightmap_res, lightmap_res, 4, 0.01f);

        vector<Uchar> dir_data_u8(lightmap_res * lightmap_res * 4);
        for (Int i = 0; i < lightmap_res * lightmap_res; ++i) {
            Vec3 dir = { filtered_direction_data[i * 3], filtered_direction_data[i * 3 + 1], filtered_direction_data[i * 3 + 2] };
            if (Math::vec3_length_sq(dir) > BLACK_THRESHOLD) Math::vec3_normalize(&dir); else dir = { 0,0,0 };
            dir_data_u8[i * 4 + 0] = static_cast<Uchar>((dir.x * 0.5f + 0.5f) * 255.0f);
            dir_data_u8[i * 4 + 1] = static_cast<Uchar>((dir.y * 0.5f + 0.5f) * 255.0f);
            dir_data_u8[i * 4 + 2] = static_cast<Uchar>((dir.z * 0.5f + 0.5f) * 255.0f);
            dir_data_u8[i * 4 + 3] = 255;
        }

        apply_gaussian_blur(final_hdr_lightmap_data, lightmap_res, lightmap_res, 3);

        fs::path color_path = data.output_dir / "lightmap_color.hdr";
        stbi_write_hdr(color_path.string().c_str(), lightmap_res, lightmap_res, 3, final_hdr_lightmap_data.data());

        fs::path dir_path = data.output_dir / "lightmap_dir.png";
        stbi_write_png(dir_path.string().c_str(), lightmap_res, lightmap_res, 4, dir_data_u8.data(), lightmap_res * 4);
    }

    void Lightmapper::process_model_vertex(const ModelVertexJobData& data)
    {
        const SceneObject& obj = m_scene->objects[data.model_index];
        Uint count = obj.model->totalVertexCount;
        vector<Vec4> colors(count);
        vector<Vec4> directions(count);

        for (Uint v_idx = 0; v_idx < count; ++v_idx) {
            Vec3 local_pos = { obj.model->combinedVertexData[v_idx * 3 + 0], obj.model->combinedVertexData[v_idx * 3 + 1], obj.model->combinedVertexData[v_idx * 3 + 2] };
            Vec3 local_normal = { obj.model->combinedNormalData[v_idx * 3 + 0], obj.model->combinedNormalData[v_idx * 3 + 1], obj.model->combinedNormalData[v_idx * 3 + 2] };

            Vec3 world_pos = Math::mat4_mul_vec3(&obj.modelMatrix, local_pos);
            Vec3 world_normal = Math::mat4_mul_vec3_dir(&obj.modelMatrix, local_normal);
            Math::vec3_normalize(&world_normal);

            mt19937 rng(generate_seed_from_pos(world_pos));
            Vec3 dir_acc = { 0,0,0 }, ind_dir = { 0,0,0 };
            Vec3 direct = calculate_direct_light(world_pos, world_normal, dir_acc, BAKE_STATIC_DIRECT_ONLY);
            Vec3 indirect = calculate_indirect_light(world_pos, world_normal, rng, ind_dir, INDIRECT_SAMPLES_PER_POINT_MODELS);
            Vec3 sun = calculate_direct_sun_light_only(world_pos, world_normal);

            Vec3 final_c = Math::vec3_add(Math::vec3_add(direct, sun), indirect);
            colors[v_idx] = { final_c.x, final_c.y, final_c.z, 1.0f };

            dir_acc = Math::vec3_add(dir_acc, ind_dir);
            if (Math::vec3_length_sq(dir_acc) > BLACK_THRESHOLD) Math::vec3_normalize(&dir_acc);
            directions[v_idx] = { dir_acc.x, dir_acc.y, dir_acc.z, 1.0f };
        }

        ofstream vlm_file(data.output_dir / "vertex_colors.vlm", ios::binary);
        if (vlm_file) {
            vlm_file.write("VLM1", 4);
            vlm_file.write(reinterpret_cast<const Char*>(&count), sizeof(Uint));
            vlm_file.write(reinterpret_cast<const Char*>(colors.data()), sizeof(Vec4) * count);
        }

        ofstream vld_file(data.output_dir / "vertex_directions.vld", ios::binary);
        if (vld_file) {
            vld_file.write("VLD1", 4);
            vld_file.write(reinterpret_cast<const Char*>(&count), sizeof(Uint));
            vld_file.write(reinterpret_cast<const Char*>(directions.data()), sizeof(Vec4) * count);
        }
    }

    void Lightmapper::process_model_lightmap(const ModelLightmapJobData& data)
    {
        const SceneObject& scene_obj = m_scene->objects[data.model_index];
        if (!scene_obj.model) return;

        LoadedModel* clean_model = Model_Load(scene_obj.modelPath);
        if (!clean_model) {
            Console::Printf_Error("[Lightmapper] Failed to load clean model for baking: %s", scene_obj.modelPath);
            return;
        }

        xatlas::Atlas* atlas = xatlas::Create();
        Int num_meshes = clean_model->meshCount;

        for (Int m = 0; m < num_meshes; ++m) {
            const Mesh& mesh = clean_model->meshes[m];

            xatlas::MeshDecl meshDecl;
            meshDecl.vertexCount = mesh.vertexCount;

            meshDecl.vertexPositionData = &mesh.final_vbo_data[0];
            meshDecl.vertexPositionStride = 24 * sizeof(Float);

            meshDecl.vertexNormalData = &mesh.final_vbo_data[3];
            meshDecl.vertexNormalStride = 24 * sizeof(Float);

            meshDecl.vertexUvData = &mesh.final_vbo_data[6];
            meshDecl.vertexUvStride = 24 * sizeof(Float);

            meshDecl.indexCount = mesh.indexCount;
            meshDecl.indexData = mesh.indexData;
            meshDecl.indexFormat = xatlas::IndexFormat::UInt32;

            xatlas::AddMeshError err = xatlas::AddMesh(atlas, meshDecl, 1);
            if (err != xatlas::AddMeshError::Success) {
                Console::Printf_Error("[Lightmapper] xatlas error adding mesh %d: %s", m, xatlas::StringForEnum(err));
            }
        }

        Float bounds_size = Math::vec3_length(Math::vec3_sub(scene_obj.model->aabb_max, scene_obj.model->aabb_min));
        Float world_scale = fmaxf(scene_obj.scale.x, fmaxf(scene_obj.scale.y, scene_obj.scale.z));
        Int resolution = clamp((Int)(bounds_size * world_scale * scene_obj.lightmapScale * 32.0f), 32, 2048);
        resolution = pow(2, ceil(log(resolution) / log(2)));

        xatlas::Generate(atlas);

        fs::path lmuv_path = data.output_dir / "model.lmuv";
        FILE* f_lmuv = fopen(lmuv_path.string().c_str(), "wb");
        if (f_lmuv) {
            const Char magic[] = "LMUV";
            fwrite(magic, 1, 4, f_lmuv);
            uint32_t count = atlas->meshCount;
            fwrite(&count, sizeof(uint32_t), 1, f_lmuv);

            for (uint32_t i = 0; i < atlas->meshCount; ++i) {
                const xatlas::Mesh& mesh = atlas->meshes[i];
                fwrite(&mesh.vertexCount, sizeof(uint32_t), 1, f_lmuv);
                fwrite(&mesh.indexCount, sizeof(uint32_t), 1, f_lmuv);
                fwrite(mesh.indexArray, sizeof(uint32_t), mesh.indexCount, f_lmuv);

                for (uint32_t v = 0; v < mesh.vertexCount; ++v) {
                    const xatlas::Vertex& vert = mesh.vertexArray[v];
                    fwrite(&vert.xref, sizeof(uint32_t), 1, f_lmuv);
                    Float u = vert.uv[0] / (Float)atlas->width;
                    Float v_coord = vert.uv[1] / (Float)atlas->height;
                    fwrite(&u, sizeof(Float), 1, f_lmuv);
                    fwrite(&v_coord, sizeof(Float), 1, f_lmuv);
                }
            }
            fclose(f_lmuv);
        }

        vector<Float> direct_data(resolution * resolution * 3, 0.0f);
        vector<Float> indirect_data(resolution * resolution * 3, 0.0f);
        vector<Float> albedo_data(resolution * resolution * 3, 0.5f);
        vector<Float> normal_data(resolution * resolution * 3, 0.0f);
        vector<Float> dir_accum_data(resolution * resolution * 3, 0.0f);

        for (uint32_t m = 0; m < atlas->meshCount; ++m) {
            const xatlas::Mesh& xmesh = atlas->meshes[m];
            const Mesh& original_mesh = clean_model->meshes[m];

            Uint num_tris = xmesh.indexCount / 3;

            for (Uint t = 0; t < num_tris; ++t) {
                Uint i0 = xmesh.indexArray[t * 3 + 0];
                Uint i1 = xmesh.indexArray[t * 3 + 1];
                Uint i2 = xmesh.indexArray[t * 3 + 2];

                const xatlas::Vertex& xv0 = xmesh.vertexArray[i0];
                const xatlas::Vertex& xv1 = xmesh.vertexArray[i1];
                const xatlas::Vertex& xv2 = xmesh.vertexArray[i2];

                Vec2 uv0 = { xv0.uv[0] / (Float)atlas->width, xv0.uv[1] / (Float)atlas->height };
                Vec2 uv1 = { xv1.uv[0] / (Float)atlas->width, xv1.uv[1] / (Float)atlas->height };
                Vec2 uv2 = { xv2.uv[0] / (Float)atlas->width, xv2.uv[1] / (Float)atlas->height };

                auto get_vec3_pos = [&](uint32_t idx) -> Vec3 {
                    return *(Vec3*)&original_mesh.final_vbo_data[idx * 24 + 0];
                    };
                auto get_vec3_norm = [&](uint32_t idx) -> Vec3 {
                    return *(Vec3*)&original_mesh.final_vbo_data[idx * 24 + 3];
                    };

                Vec3 v0_local = get_vec3_pos(xv0.xref);
                Vec3 v1_local = get_vec3_pos(xv1.xref);
                Vec3 v2_local = get_vec3_pos(xv2.xref);

                Vec3 n0_local = get_vec3_norm(xv0.xref);
                Vec3 n1_local = get_vec3_norm(xv1.xref);
                Vec3 n2_local = get_vec3_norm(xv2.xref);

                Float min_u = fminf(uv0.x, fminf(uv1.x, uv2.x));
                Float min_v = fminf(uv0.y, fminf(uv1.y, uv2.y));
                Float max_u = fmaxf(uv0.x, fmaxf(uv1.x, uv2.x));
                Float max_v = fmaxf(uv0.y, fmaxf(uv1.y, uv2.y));

                Int min_x = clamp((Int)(min_u * resolution), 0, resolution - 1);
                Int min_y = clamp((Int)(min_v * resolution), 0, resolution - 1);
                Int max_x = clamp((Int)(max_u * resolution), 0, resolution - 1);
                Int max_y = clamp((Int)(max_v * resolution), 0, resolution - 1);

                for (Int y = min_y; y <= max_y; ++y) {
                    for (Int x = min_x; x <= max_x; ++x) {
                        Vec2 p = { (x + 0.5f) / resolution, (y + 0.5f) / resolution };
                        Vec3 bary = Math::barycentric_coords(p, uv0, uv1, uv2);

                        if (bary.x >= 0 && bary.y >= 0 && bary.z >= 0) {
                            Vec3 pos_local = Math::vec3_add(Math::vec3_muls(v0_local, bary.x), Math::vec3_add(Math::vec3_muls(v1_local, bary.y), Math::vec3_muls(v2_local, bary.z)));
                            Vec3 norm_local = Math::vec3_add(Math::vec3_muls(n0_local, bary.x), Math::vec3_add(Math::vec3_muls(n1_local, bary.y), Math::vec3_muls(n2_local, bary.z)));

                            Vec3 pos_world = Math::mat4_mul_vec3(&scene_obj.modelMatrix, pos_local);
                            Vec3 norm_world = Math::mat4_mul_vec3_dir(&scene_obj.modelMatrix, norm_local);
                            Math::vec3_normalize(&norm_world);

                            Int idx = (y * resolution + x) * 3;

                            mt19937 rng(generate_seed_from_pos(pos_world));
                            Vec3 dom_dir, ind_dir;

                            Vec3 direct = calculate_direct_light(pos_world, norm_world, dom_dir, BAKE_STATIC_DIRECT_ONLY);
                            Vec3 indirect = calculate_indirect_light(pos_world, norm_world, rng, ind_dir, INDIRECT_SAMPLES_PER_POINT_BRUSHES);
                            Vec3 sun_direct = calculate_direct_sun_light_only(pos_world, norm_world);

                            direct_data[idx] = direct.x + sun_direct.x;
                            direct_data[idx + 1] = direct.y + sun_direct.y;
                            direct_data[idx + 2] = direct.z + sun_direct.z;

                            indirect_data[idx] = indirect.x;
                            indirect_data[idx + 1] = indirect.y;
                            indirect_data[idx + 2] = indirect.z;

                            Vec3 total_dir = Math::vec3_add(dom_dir, ind_dir);
                            if (Math::vec3_length_sq(total_dir) > 0) Math::vec3_normalize(&total_dir);

                            dir_accum_data[idx] = total_dir.x;
                            dir_accum_data[idx + 1] = total_dir.y;
                            dir_accum_data[idx + 2] = total_dir.z;

                            normal_data[idx] = norm_world.x; normal_data[idx + 1] = norm_world.y; normal_data[idx + 2] = norm_world.z;
                        }
                    }
                }
            }
        }

        xatlas::Destroy(atlas);
        Model_Free(clean_model);

        vector<Float> denoised_indirect(indirect_data.size());
        {
            OIDNFilter filter = oidnNewFilter(m_oidn_device, "RTLightmap");
            oidnSetSharedFilterImage(filter, "color", indirect_data.data(), OIDN_FORMAT_FLOAT3, resolution, resolution, 0, 0, 0);
            oidnSetSharedFilterImage(filter, "albedo", albedo_data.data(), OIDN_FORMAT_FLOAT3, resolution, resolution, 0, 0, 0);
            oidnSetSharedFilterImage(filter, "normal", normal_data.data(), OIDN_FORMAT_FLOAT3, resolution, resolution, 0, 0, 0);
            oidnSetSharedFilterImage(filter, "output", denoised_indirect.data(), OIDN_FORMAT_FLOAT3, resolution, resolution, 0, 0, 0);
            oidnSetFilterBool(filter, "hdr", true);
            oidnSetFilterBool(filter, "cleanAux", true);
            oidnCommitFilter(filter);
            oidnExecuteFilter(filter);
            oidnReleaseFilter(filter);
        }

        apply_gaussian_blur(denoised_indirect, resolution, resolution, 3);
        apply_gaussian_blur(denoised_indirect, resolution, resolution, 3);

        vector<Float> final_hdr_lightmap_data(resolution * resolution * 3);
        for (Usize i = 0; i < final_hdr_lightmap_data.size() / 3; ++i) {
            final_hdr_lightmap_data[i * 3 + 0] = direct_data[i * 3 + 0] + denoised_indirect[i * 3 + 0];
            final_hdr_lightmap_data[i * 3 + 1] = direct_data[i * 3 + 1] + denoised_indirect[i * 3 + 1];
            final_hdr_lightmap_data[i * 3 + 2] = direct_data[i * 3 + 2] + denoised_indirect[i * 3 + 2];
        }

        vector<Float> filtered_direction_data;
        apply_guided_filter(filtered_direction_data, dir_accum_data, final_hdr_lightmap_data, resolution, resolution, 4, 0.01f);

        vector<Uchar> dir_data(resolution * resolution * 4, 0);

        for (Int i = 0; i < resolution * resolution; ++i) {
            Int idx = i * 3;

            Vec3 d = { filtered_direction_data[idx], filtered_direction_data[idx + 1], filtered_direction_data[idx + 2] };
            if (Math::vec3_length_sq(d) > BLACK_THRESHOLD) Math::vec3_normalize(&d);
            else d = { 0,0,0 };

            dir_data[i * 4 + 0] = (Uchar)((d.x * 0.5f + 0.5f) * 255.0f);
            dir_data[i * 4 + 1] = (Uchar)((d.y * 0.5f + 0.5f) * 255.0f);
            dir_data[i * 4 + 2] = (Uchar)((d.z * 0.5f + 0.5f) * 255.0f);
            dir_data[i * 4 + 3] = 255;
        }

        fs::path color_path = data.output_dir / "lightmap_color.hdr";
        stbi_write_hdr(color_path.string().c_str(), resolution, resolution, 3, final_hdr_lightmap_data.data());
        fs::path dir_path = data.output_dir / "lightmap_dir.png";
        stbi_write_png(dir_path.string().c_str(), resolution, resolution, 4, dir_data.data(), resolution * 4);
    }

    void Lightmapper::process_job(const JobPayload& job)
    {
        visit([this](auto&& arg) {
            using T = decay_t<decltype(arg)>;
            if constexpr (is_same_v<T, BrushFaceJobData>)
                process_brush_face(arg);
            else if constexpr (is_same_v<T, DecalJobData>)
                process_decal(arg);
            else if constexpr (is_same_v<T, ModelVertexJobData>)
                process_model_vertex(arg);
            else if constexpr (is_same_v<T, ModelLightmapJobData>)
                process_model_lightmap(arg);
            }, job);
    }

    void Lightmapper::worker_main()
    {
        while (true)
        {
            Usize job_index = m_next_job_index.fetch_add(1);
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

        Usize total_brush_faces = 0;
        for (Int i = 0; i < m_scene->numBrushes; ++i)
        {
            const Brush& b = m_scene->brushes[i];
            if (!IsBrushBakeable(b)) continue;
            {
                total_brush_faces += b.numFaces;
            }
        }

        Usize total_model_vertices = 0;
        for (Int i = 0; i < m_scene->numObjects; ++i)
        {
            if (m_scene->objects[i].model)
            {
                total_model_vertices += m_scene->objects[i].model->totalVertexCount;
            }
        }

        if (total_brush_faces + total_model_vertices == 0)
        {
            Console::Printf("[Lightmapper] No bakeable geometry found.");
            return;
        }

        m_jobs.reserve(total_brush_faces + total_model_vertices);
        m_model_color_buffers.resize(m_scene->numObjects);
        m_model_direction_buffers.resize(m_scene->numObjects);

        m_brush_color_buffers.resize(m_scene->numBrushes);
        m_brush_direction_buffers.resize(m_scene->numBrushes);

        for (Int i = 0; i < m_scene->numObjects; ++i)
        {
            const SceneObject& obj = m_scene->objects[i];
            if (obj.model && obj.mass <= 0.0f && !obj.useLightmap)
            {
                m_model_color_buffers[i] = make_unique<Vec4[]>(obj.model->totalVertexCount);
                m_model_direction_buffers[i] = make_unique<Vec4[]>(obj.model->totalVertexCount);
            }
        }

        for (Int i = 0; i < m_scene->numBrushes; ++i)
        {
            const Brush& b = m_scene->brushes[i];
            if (!IsBrushBakeable(b)) continue;
            string brush_name_str = (strlen(b.targetname) > 0) ? b.targetname : "Brush_" + to_string(i);
            fs::path brush_dir = m_output_path / sanitize_filename(brush_name_str);
            fs::create_directories(brush_dir);
            for (Int j = 0; j < b.numFaces; ++j)
            {
                if (b.faces[j].material == &g_NodrawMaterial) {
                    continue;
                }
                m_jobs.emplace_back(BrushFaceJobData{ i, j, brush_dir });
            }
        }

        for (Int i = 0; i < m_scene->numDecals; ++i)
        {
            fs::path decal_dir = m_output_path / ("decal_" + to_string(i));
            fs::create_directories(decal_dir);
            m_jobs.emplace_back(DecalJobData{ i, decal_dir });
        }

        for (Int i = 0; i < m_scene->numObjects; ++i)
        {
            const SceneObject& obj = m_scene->objects[i];
            if (obj.mass > 0.0f) continue;
            if (obj.model)
            {
                if (obj.useLightmap) {
                    string model_name_str = (strlen(obj.targetname) > 0) ? obj.targetname : "Model_" + to_string(i);
                    fs::path model_dir = m_output_path / sanitize_filename(model_name_str);
                    fs::create_directories(model_dir);
                    m_jobs.emplace_back(ModelLightmapJobData{ i, model_dir });
                }
                else {
                    string model_name_str = (strlen(obj.targetname) > 0) ? obj.targetname : "Model_" + to_string(i);
                    fs::path model_dir = m_output_path / sanitize_filename(model_name_str);
                    fs::create_directories(model_dir);
                    m_jobs.emplace_back(ModelVertexJobData{ i, model_dir });
                }
            }
        }
        Console::Printf("[Lightmapper] Baking %zu faces, %zu vertices, and %d decals.", total_brush_faces, total_model_vertices, m_scene->numDecals);
    }

    void Lightmapper::precalculate_material_reflectivity()
    {
        Console::Printf("[Lightmapper] Pre-calculating surface reflectivity...");

        set<const Material*> unique_materials;
        for (Int i = 0; i < m_scene->numBrushes; ++i) {
            const Brush& b = m_scene->brushes[i];
            for (Int j = 0; j < b.numFaces; ++j) {
                const BrushFace& face = b.faces[j];
                if (face.material && face.material != &g_NodrawMaterial) unique_materials.insert(face.material);
                if (face.material2) unique_materials.insert(face.material2);
                if (face.material3) unique_materials.insert(face.material3);
                if (face.material4) unique_materials.insert(face.material4);
            }
        }
        for (Int i = 0; i < m_scene->numDecals; ++i) {
            const Decal& d = m_scene->decals[i];
            if (d.material) {
                unique_materials.insert(d.material);
            }
        }
        for (Int i = 0; i < m_scene->numObjects; ++i) {
            const SceneObject& obj = m_scene->objects[i];
            if (obj.model) {
                for (Int j = 0; j < obj.model->meshCount; ++j) {
                    const Mesh& mesh = obj.model->meshes[j];
                    if (mesh.material) {
                        unique_materials.insert(mesh.material);
                    }
                }
            }
        }

        Console::Printf("[Lightmapper] Found %zu unique materials to analyze.", unique_materials.size());

        for (const Material* mat : unique_materials) {
            if (!mat || mat->diffusePath[0] == '\0') {
                m_material_reflectivity[mat] = { 0.5f, 0.5f, 0.5f, 1.0f };
                continue;
            }

            string full_path = "textures/" + string(mat->diffusePath);
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

            LongLong total_r = 0, total_g = 0, total_b = 0, total_a = 0;
            Int texels = surface->w * surface->h;
            Uint8* pixels = (Uint8*)surface->pixels;
            for (Int i = 0; i < texels; ++i) {
                total_r += pixels[i * 4 + 0];
                total_g += pixels[i * 4 + 1];
                total_b += pixels[i * 4 + 2];
                total_a += pixels[i * 4 + 3];
            }
            SDL_FreeSurface(surface);

            m_material_reflectivity[mat] = {
                static_cast<Float>(total_r) / texels / 255.0f,
                static_cast<Float>(total_g) / texels / 255.0f,
                static_cast<Float>(total_b) / texels / 255.0f,
                static_cast<Float>(total_a) / texels / 255.0f
            };
        }

        for (Int i = 0; i < m_scene->numBrushes; ++i) {
            const Brush& b = m_scene->brushes[i];
            for (Int j = 0; j < b.numFaces; ++j) {
                const BrushFace& face = b.faces[j];
                if (face.numVertexIndices == 0 || !face.material || face.material == &g_NodrawMaterial) {
                    continue;
                }

                Vec4 avg_color = { 0.0f, 0.0f, 0.0f, 0.0f };
                for (Int k = 0; k < face.numVertexIndices; ++k) {
                    Int vert_idx = face.vertexIndices[k];
                    avg_color = Math::vec4_add(avg_color, b.vertices[vert_idx].color);
                }
                avg_color = Math::vec4_muls(avg_color, 1.0f / face.numVertexIndices);

                Vec4 mat1_refl = m_material_reflectivity.count(face.material) ? m_material_reflectivity[face.material] : Vec4{ 0.5f, 0.5f, 0.5f, 1.0f };
                Vec4 mat2_refl = face.material2 && m_material_reflectivity.count(face.material2) ? m_material_reflectivity[face.material2] : Vec4{ 0.0f, 0.0f, 0.0f, 1.0f };
                Vec4 mat3_refl = face.material3 && m_material_reflectivity.count(face.material3) ? m_material_reflectivity[face.material3] : Vec4{ 0.0f, 0.0f, 0.0f, 1.0f };
                Vec4 mat4_refl = face.material4 && m_material_reflectivity.count(face.material4) ? m_material_reflectivity[face.material4] : Vec4{ 0.0f, 0.0f, 0.0f, 1.0f };

                Float weightR = avg_color.x;
                Float weightG = avg_color.y;
                Float weightB = avg_color.z;
                Float totalWeight = max(weightR + weightG + weightB, 0.0001f);
                if (totalWeight > 1.0f) {
                    weightR /= totalWeight;
                    weightG /= totalWeight;
                    weightB /= totalWeight;
                }
                Float weightBase = 1.0f - (weightR + weightG + weightB);

                Vec4 blended_refl;
                blended_refl.x = mat1_refl.x * weightBase + mat2_refl.x * weightR + mat3_refl.x * weightG + mat4_refl.x * weightB;
                blended_refl.y = mat1_refl.y * weightBase + mat2_refl.y * weightR + mat3_refl.y * weightG + mat4_refl.y * weightB;
                blended_refl.z = mat1_refl.z * weightBase + mat2_refl.z * weightR + mat3_refl.z * weightG + mat4_refl.z * weightB;
                blended_refl.w = mat1_refl.w * weightBase + mat2_refl.w * weightR + mat3_refl.w * weightG + mat4_refl.w * weightB;

                m_face_reflectivity[&face] = blended_refl;
            }
        }
        Console::Printf("[Lightmapper] Reflectivity calculation complete.");
    }

    Vec3 Lightmapper::calculate_direct_sun_light_only(const Vec3& pos, const Vec3& normal) const
    {
        if (m_scene->sun.enabled) {
            Vec3 point_to_light_check = Math::vec3_add(pos, Math::vec3_muls(normal, SHADOW_BIAS));
            Vec3 light_dir = Math::vec3_muls(m_scene->sun.direction, -1.0f);
            Float NdotL = max(0.0f, Math::vec3_dot(normal, light_dir));
            if (NdotL > 0.0f) {
                if (!is_in_shadow(point_to_light_check, Math::vec3_add(point_to_light_check, Math::vec3_muls(light_dir, 10000.0f)))) {
                    Vec3 light_color = Math::vec3_muls(m_scene->sun.color, m_scene->sun.intensity);
                    return Math::vec3_muls(light_color, NdotL);
                }
            }
        }
        return { 0,0,0 };
    }

    Vec3 Lightmapper::calculate_direct_light(const Vec3& pos, const Vec3& normal, Vec3& out_dominant_dir, DirectLightMode mode) const
    {
        Vec3 direct_light = { 0,0,0 };
        out_dominant_dir = { 0,0,0 };
        Vec3 point_to_light_check = Math::vec3_add(pos, Math::vec3_muls(normal, SHADOW_BIAS));

        if (m_scene->sun.enabled) {
            Vec3 light_dir = Math::vec3_muls(m_scene->sun.direction, -1.0f);
            Float NdotL = max(0.0f, Math::vec3_dot(normal, light_dir));
            if (NdotL > 0.0f) {
                if (!is_in_shadow(point_to_light_check, Math::vec3_add(point_to_light_check, Math::vec3_muls(light_dir, 10000.0f)))) {
                    Vec3 light_color = Math::vec3_muls(m_scene->sun.color, m_scene->sun.intensity);
                    Vec3 light_contribution = Math::vec3_muls(light_color, NdotL);
                    direct_light = Math::vec3_add(direct_light, light_contribution);

                    Float contribution_magnitude = Math::vec3_length(light_contribution);
                    out_dominant_dir = Math::vec3_add(out_dominant_dir, Math::vec3_muls(light_dir, contribution_magnitude));
                }
            }
        }

        uniform_real_distribution<Float> random_dist(-0.5f, 0.5f);
        mt19937 rng(generate_seed_from_pos(pos));

        for (Int k = 0; k < m_scene->numActiveLights; ++k)
        {
            const Light& light = m_scene->lights[k];
            if (mode == BAKE_ALL_FOR_BOUNCES && light.is_static == 0) continue;
            if (mode == BAKE_STATIC_DIRECT_ONLY && light.is_static != 1) continue;

            if (light.type == LIGHT_AREA) {
                if (light.width <= 0 || light.height <= 0) continue;

                Mat4 light_transform = Math::create_trs_matrix({ 0,0,0 }, light.rot, { 1,1,1 });
                Vec3 light_right = Math::mat4_mul_vec3_dir(&light_transform, { 1, 0, 0 });
                Vec3 light_up = Math::mat4_mul_vec3_dir(&light_transform, { 0, 1, 0 });
                Vec3 light_forward = Math::mat4_mul_vec3_dir(&light_transform, { 0, 0, -1 });

                if (Math::vec3_dot(normal, light_forward) >= 0) {
                    continue;
                }

                Vec3 accumulated_light = { 0,0,0 };
                Int samples_that_hit = 0;

                Int grid_size = static_cast<Int>(sqrt(NUM_AREA_LIGHT_SAMPLES));
                uniform_real_distribution<Float> jitter_dist(0.0f, 1.0f);

                for (Int y = 0; y < grid_size; ++y) {
                    for (Int x = 0; x < grid_size; ++x) {
                        Float u = ((Float)x + jitter_dist(rng)) / (Float)grid_size - 0.5f;
                        Float v = ((Float)y + jitter_dist(rng)) / (Float)grid_size - 0.5f;

                        Vec3 sample_offset = Math::vec3_add(Math::vec3_muls(light_right, u * light.width), Math::vec3_muls(light_up, v * light.height));
                        Vec3 sample_pos = Math::vec3_add(light.pos, sample_offset);

                        Vec3 light_dir = Math::vec3_sub(sample_pos, pos);
                        Float dist_sq = Math::vec3_length_sq(light_dir);
                        if (dist_sq > light.radius * light.radius) continue;

                        Math::vec3_normalize(&light_dir);
                        Float NdotL = max(0.0f, Math::vec3_dot(normal, light_dir));
                        if (NdotL <= 0.0f) continue;

                        if (is_in_shadow(point_to_light_check, sample_pos)) continue;

                        samples_that_hit++;
                        Float dist = sqrtf(dist_sq);
                        Float attenuation = powf(max(0.0f, 1.0f - dist / light.radius), 2.0f);
                        attenuation /= (dist * dist + 1.0f);

                        Vec3 light_color = Math::vec3_muls(light.color, light.intensity);
                        accumulated_light = Math::vec3_add(accumulated_light, Math::vec3_muls(light_color, attenuation * NdotL));
                    }
                }

                if (samples_that_hit > 0) {
                    Vec3 light_contribution = Math::vec3_muls(accumulated_light, 1.0f / (Float)(grid_size * grid_size));
                    direct_light = Math::vec3_add(direct_light, light_contribution);

                    Vec3 avg_light_dir = Math::vec3_muls(light_forward, -1.0f);
                    Math::vec3_normalize(&avg_light_dir);
                    out_dominant_dir = Math::vec3_add(out_dominant_dir, Math::vec3_muls(avg_light_dir, Math::vec3_length(light_contribution)));
                }
                continue;
            }

            Vec3 light_dir = Math::vec3_sub(light.pos, pos);
            Float dist = Math::vec3_length(light_dir);
            Math::vec3_normalize(&light_dir);
            if (dist > light.radius) continue;

            Float NdotL = max(0.0f, Math::vec3_dot(normal, light_dir));
            if (NdotL <= 0.0f) continue;

            if (is_in_shadow(point_to_light_check, light.pos)) continue;

            Float spotFactor = 1.0f;
            if (light.type == LIGHT_SPOT)
            {
                Vec3 light_forward_vector = Math::vec3_muls(light.direction, -1.0f);
                Float theta = Math::vec3_dot(light_dir, light_forward_vector);
                Float inner_cone_cos = light.cutOff;
                Float outer_cone_cos = light.outerCutOff;

                if (theta < outer_cone_cos) {
                    spotFactor = 0.0f;
                }
                else {
                    Float delta = inner_cone_cos - outer_cone_cos;
                    if (delta > 0.0001f) {
                        Float t = clamp((theta - outer_cone_cos) / delta, 0.0f, 1.0f);
                        spotFactor = t * t * (3.0f - 2.0f * t);
                    }
                    else {
                        spotFactor = (theta >= inner_cone_cos) ? 1.0f : 0.0f;
                    }
                }
            }

            Float attenuation = powf(max(0.0f, 1.0f - dist / light.radius), 2.0f);
            attenuation /= (dist * dist + 1.0f);
            attenuation *= spotFactor;
            Vec3 light_color = Math::vec3_muls(light.color, light.intensity);
            Vec3 light_contribution = Math::vec3_muls(light_color, NdotL * attenuation);
            direct_light = Math::vec3_add(direct_light, light_contribution);

            Float contribution_magnitude = Math::vec3_length(light_contribution);
            out_dominant_dir = Math::vec3_add(out_dominant_dir, Math::vec3_muls(light_dir, contribution_magnitude));
        }
        return direct_light;
    }

    Vec3 Lightmapper::cosine_weighted_direction_in_hemisphere(const Vec3& normal, mt19937& gen)
    {
        uniform_real_distribution<Float> dist(0.0f, 1.0f);
        Float u1 = dist(gen);
        Float u2 = dist(gen);

        Float r = sqrtf(u1);
        Float theta = 2.0f * (Float)Common::PI * u2;

        Float x = r * cosf(theta);
        Float y = r * sinf(theta);
        Float z = sqrtf(max(0.0f, 1.0f - u1));

        Vec3 up = fabsf(normal.z) < 0.999f ? Vec3{ 0,0,1 } : Vec3{ 1,0,0 };
        Vec3 tangent = Math::vec3_cross(up, normal);
        Math::vec3_normalize(&tangent);
        Vec3 bitangent = Math::vec3_cross(normal, tangent);

        return Math::vec3_add(Math::vec3_muls(tangent, x), Math::vec3_add(Math::vec3_muls(bitangent, y), Math::vec3_muls(normal, z)));
    }

    Vec4 Lightmapper::get_reflectivity_at_hit(Uint primID) const
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

    Vec3 Lightmapper::calculate_indirect_light(const Vec3& origin, const Vec3& normal, mt19937& rng, Vec3& out_indirect_dir, Int num_samples)
    {
        Vec3 accumulated_color = { 0, 0, 0 };
        out_indirect_dir = { 0, 0, 0 };

        if (m_bounces <= 0 || num_samples <= 0)
        {
            return accumulated_color;
        }

        uniform_real_distribution<Float> roulette_dist(0.0f, 1.0f);
        constexpr Int BATCH_SIZE = 16;
        Int num_batches = (num_samples + BATCH_SIZE - 1) / BATCH_SIZE;

        RTCIntersectArguments args;
        rtcInitIntersectArguments(&args);
        args.feature_mask = RTC_FEATURE_FLAG_NONE;

        for (Int i = 0; i < num_batches; ++i)
        {
            RTCRayHit16 rayhit16;
            Int valid[BATCH_SIZE];
            Vec3 first_bounce_dirs[BATCH_SIZE];
            Int current_batch_size = ((i == num_batches - 1) && (num_samples % BATCH_SIZE != 0)) ? (num_samples % BATCH_SIZE) : BATCH_SIZE;

            for (Int k = 0; k < current_batch_size; ++k)
            {
                valid[k] = -1;
                Vec3 bounce_dir = cosine_weighted_direction_in_hemisphere(normal, rng);
                first_bounce_dirs[k] = bounce_dir;
                Vec3 trace_origin = Math::vec3_add(origin, Math::vec3_muls(normal, SHADOW_BIAS));

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

            for (Int k = 0; k < current_batch_size; ++k)
            {
                if (rayhit16.hit.geomID[k] == RTC_INVALID_GEOMETRY_ID)
                {
                    continue;
                }

                Vec3 path_radiance = { 0, 0, 0 };
                Vec3 throughput = { 1.0f, 1.0f, 1.0f };

                Vec3 current_pos = {
                    rayhit16.ray.org_x[k] + rayhit16.ray.tfar[k] * rayhit16.ray.dir_x[k],
                    rayhit16.ray.org_y[k] + rayhit16.ray.tfar[k] * rayhit16.ray.dir_y[k],
                    rayhit16.ray.org_z[k] + rayhit16.ray.tfar[k] * rayhit16.ray.dir_z[k]
                };
                Vec3 current_normal = { rayhit16.hit.Ng_x[k], rayhit16.hit.Ng_y[k], rayhit16.hit.Ng_z[k] };
                Math::vec3_normalize(&current_normal);

                Uint current_primID = rayhit16.hit.primID[k];

                if (Math::vec3_dot(first_bounce_dirs[k], current_normal) > 0.0f) {
                    continue;
                }

                for (Int bounce = 1; bounce <= m_bounces; ++bounce)
                {
                    Vec4 reflectivity = get_reflectivity_at_hit(current_primID);
                    Vec3 albedo = { reflectivity.x, reflectivity.y, reflectivity.z };
                    Vec3 dummy_dir;
                    Vec3 direct_light = calculate_direct_light(current_pos, current_normal, dummy_dir, BAKE_ALL_FOR_BOUNCES);
                    path_radiance = Math::vec3_add(path_radiance, Math::vec3_mul(Math::vec3_mul(direct_light, albedo), throughput));

                    throughput = Math::vec3_mul(throughput, albedo);
                    Float p = max({ throughput.x, throughput.y, throughput.z });
                    if (bounce == m_bounces || p < 0.001f || roulette_dist(rng) > p) {
                        break;
                    }
                    throughput = Math::vec3_muls(throughput, 1.0f / p);

                    Vec3 bounce_dir = cosine_weighted_direction_in_hemisphere(current_normal, rng);
                    Vec3 trace_origin = Math::vec3_add(current_pos, Math::vec3_muls(current_normal, SHADOW_BIAS));

                    RTCRayHit rayhit;
                    rayhit.ray.org_x = trace_origin.x; rayhit.ray.org_y = trace_origin.y; rayhit.ray.org_z = trace_origin.z;
                    rayhit.ray.dir_x = bounce_dir.x; rayhit.ray.dir_y = bounce_dir.y; rayhit.ray.dir_z = bounce_dir.z;
                    rayhit.ray.tnear = 0.0f; rayhit.ray.tfar = FLT_MAX;
                    rayhit.ray.mask = -1; rayhit.ray.flags = 0;
                    rayhit.hit.geomID = RTC_INVALID_GEOMETRY_ID;

                    rtcIntersect1(m_rtc_scene, &rayhit, &args);

                    if (rayhit.hit.geomID == RTC_INVALID_GEOMETRY_ID) {
                        break;
                    }

                    current_pos = { trace_origin.x + rayhit.ray.tfar * bounce_dir.x, trace_origin.y + rayhit.ray.tfar * bounce_dir.y, trace_origin.z + rayhit.ray.tfar * bounce_dir.z };
                    current_normal = { rayhit.hit.Ng_x, rayhit.hit.Ng_y, rayhit.hit.Ng_z };
                    Math::vec3_normalize(&current_normal);

                    if (Math::vec3_dot(bounce_dir, current_normal) > 0.0f) {
                        break;
                    }
                    current_primID = rayhit.hit.primID;
                }

                accumulated_color = Math::vec3_add(accumulated_color, path_radiance);
                out_indirect_dir = Math::vec3_add(out_indirect_dir, Math::vec3_muls(first_bounce_dirs[k], Math::vec3_length(path_radiance)));
            }
        }

        if (Math::vec3_length_sq(out_indirect_dir) > BLACK_THRESHOLD)
        {
            Math::vec3_normalize(&out_indirect_dir);
        }

        return Math::vec3_muls(accumulated_color, 1.0f / (Float)num_samples);
    }

    void Lightmapper::generate()
    {
        Console::Printf("[Lightmapper] Starting lightmap generation...");
        auto start_time = chrono::high_resolution_clock::now();
        m_scene->lightmapResolution = m_resolution;

        precalculate_material_reflectivity();
        prepare_jobs();
        if (m_jobs.empty()) return;

        Uint num_threads = thread::hardware_concurrency();
        vector<thread> threads;
        Console::Printf("[Lightmapper] Using %u threads for final gather.", num_threads);

        for (Uint i = 0; i < num_threads; ++i)
        {
            threads.emplace_back(&Lightmapper::worker_main, this);
        }
        for (auto& t : threads)
        {
            t.join();
        }

        generate_ambient_probes();

        auto end_time = chrono::high_resolution_clock::now();
        chrono::duration<Float> duration = end_time - start_time;
        Console::Printf("[Lightmapper] Finished in %.2f seconds.", duration.count());
    }
}

void Lightmapper_Generate(Scene* scene, Engine* engine, Int resolution, Int bounces)
{
    try
    {
        Lightmapper mapper(scene, resolution, bounces);
        mapper.generate();
    }
    catch (const exception& e)
    {
        Console::Printf_Error("[Lightmapper] C++ Exception: %s", e.what());
    }
    catch (...)
    {
        Console::Printf_Error("[Lightmapper] Unknown C++ exception occurred.");
    }
}
#else
#include "lightmapper.h"
#include "gl_console.h"

void Lightmapper_Generate(Scene* scene, Engine* engine, Int resolution, Int bounces)
{
    Console::Printf_Error("[Lightmapper] Not available on x86 builds.");
}
#endif