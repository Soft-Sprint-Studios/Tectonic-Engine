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
#include "gl_geometry.h"
#include "gl_misc.h"
#include "gl_zprepass.h"
#include "gl_render_misc.h"
#include "gl_sprites.h"
#include "gl_video_player.h"
#include "cvar.h"
#include <float.h>
#include "io_system.h"
#include "gl_decals.h"
#include "gl_beams.h"
#include "gl_cables.h"
#include "gl_glow.h"
#include "gl_monitor.h"
#include "map_misc.h"

Int FindReflectionProbeForPoint(Scene* scene, Vec3 p) {
    for (Int i = 0; i < scene->numBrushes; ++i) {
        Brush* b = &scene->brushes[i];
        if (strcmp(b->classname, "env_reflectionprobe") != 0) {
            continue;
        }

        Vec3 min_aabb_world, max_aabb_world;
        Brush_GetWorldAABB(b, &min_aabb_world, &max_aabb_world);

        if (p.x >= min_aabb_world.x && p.x <= max_aabb_world.x &&
            p.y >= min_aabb_world.y && p.y <= max_aabb_world.y &&
            p.z >= min_aabb_world.z && p.z <= max_aabb_world.z)
        {
            return i;
        }
    }
    return -1;
}

void render_object(Renderer* renderer, Scene* scene, GLuint shader, SceneObject* obj, Bool is_baking_pass, const Frustum* frustum) {
    Bool envMapEnabled = false;

    if (!is_baking_pass && shader == renderer->mainShader && Cvar_GetInt("r_cubemaps")) {
        Int reflection_brush_idx = FindReflectionProbeForPoint(scene, obj->pos);
        if (reflection_brush_idx != -1) {
            Brush* reflection_brush = &scene->brushes[reflection_brush_idx];
            if (reflection_brush->cubemapTexture != 0) {
                glActiveTexture(GL_TEXTURE10);
                glBindTexture(GL_TEXTURE_CUBE_MAP, reflection_brush->cubemapTexture);

                Shader_Set(shader, "environmentMap", 10);
                Shader_Set(shader, "useParallaxCorrection", 1);

                Vec3 min_aabb, max_aabb;
                Brush_GetWorldAABB(reflection_brush, &min_aabb, &max_aabb);

                Shader_Set(shader, "probeBoxMin", min_aabb);
                Shader_Set(shader, "probeBoxMax", max_aabb);
                Shader_Set(shader, "probePosition", reflection_brush->pos);

                envMapEnabled = true;
            }
        }
    }

    Shader_Set(shader, "useEnvironmentMap", (Int)envMapEnabled);

    if (shader == renderer->mainShader) {
        Bool is_skinnable = obj->model && obj->model->num_skins > 0;
        Shader_Set(shader, "u_hasAnimation", (Int)is_skinnable);

        if (is_skinnable && obj->bone_matrices) {
            glUniformMatrix4fv(Shader_GetUniformLocation(shader, "u_boneMatrices"), obj->model->skins[0].num_joints, GL_FALSE, (const GLfloat*)obj->bone_matrices);
        }
    }

    Shader_Set(shader, "u_fadeStartDist", obj->fadeStartDist);
    Shader_Set(shader, "u_fadeEndDist", obj->fadeEndDist);

    Mat4 finalModelMatrix = obj->modelMatrix;
    if (obj->model && obj->model->num_animations > 0 && obj->model->num_skins == 0) {
        Math::mat4_multiply(&finalModelMatrix, &obj->modelMatrix, &obj->animated_local_transform);
    }

    Shader_Set(shader, "model", &finalModelMatrix);
    Shader_Set(shader, "u_swayEnabled", (Int)obj->swayEnabled);

    if (shader == renderer->mainShader) {
        if (obj->useLightmap && obj->lightmapHandle) {
            Shader_Set(shader, "useLightmap", 1);
            Shader_Set(shader, "lightmap", obj->lightmapHandle);

            if (obj->lightmapWidth > 0 && obj->lightmapHeight > 0) {
                Shader_Set(shader, "u_lightmap_sampler_size", Vec2{ (Float)obj->lightmapWidth, (Float)obj->lightmapHeight });
            }

            if (obj->dirLightmapHandle) {
                Shader_Set(shader, "useDirectionalLightmap", 1);
                Shader_Set(shader, "directionalLightmap", obj->dirLightmapHandle);
            }
            else {
                Shader_Set(shader, "useDirectionalLightmap", 0);
            }
        }
        else {
            Shader_Set(shader, "useLightmap", 0);
            Shader_Set(shader, "useDirectionalLightmap", 0);
        }
    }

    if (obj->model) {
        if (obj->bakedVertexColors || obj->bakedVertexDirections) {
            Uint vertex_offset = 0;
            constexpr Int stride_floats = 24;
            for (Int i = 0; i < obj->model->meshCount; ++i) {
                Mesh* mesh = &obj->model->meshes[i];
                for (Uint v = 0; v < mesh->vertexCount; ++v) {
                    if (obj->bakedVertexColors) {
                        memcpy(&mesh->final_vbo_data[(v * stride_floats) + 12], &obj->bakedVertexColors[vertex_offset + v], sizeof(Vec4));
                    }
                    if (obj->bakedVertexDirections) {
                        memcpy(&mesh->final_vbo_data[(v * stride_floats) + 16], &obj->bakedVertexDirections[vertex_offset + v], sizeof(Vec4));
                    }
                }
                glBindBuffer(GL_ARRAY_BUFFER, mesh->VBO);
                glBufferSubData(GL_ARRAY_BUFFER, 0, mesh->final_vbo_data_size, mesh->final_vbo_data);
                vertex_offset += mesh->vertexCount;
            }
            glBindBuffer(GL_ARRAY_BUFFER, 0);
            if (obj->bakedVertexColors) {
                delete[] obj->bakedVertexColors;
                obj->bakedVertexColors = nullptr;
            }
            if (obj->bakedVertexDirections) {
                delete[] obj->bakedVertexDirections;
                obj->bakedVertexDirections = nullptr;
            }
        }

        for (Int i = 0; i < obj->model->meshCount; ++i) {
            Mesh* mesh = &obj->model->meshes[i];
            Material* material = mesh->material;
            if (shader == renderer->mainShader) {
                Bool isTesselationEnabled = material->useTesselation;
                Shader_Set(shader, "u_useTesselation", (Int)isTesselationEnabled);
                Shader_Set(shader, "u_useAlphaTest", (Int)material->alpha);

                Bool parallaxEnabledForThisMesh = !isTesselationEnabled && Cvar_GetInt("r_relief_mapping") && material->heightScale > 0.0f;
                Shader_Set(shader, "u_isParallaxEnabled", (Int)parallaxEnabledForThisMesh);
                Shader_Set(shader, "heightScale", material->heightScale);
                Shader_Set(shader, "u_roughness_override", material->roughness);
                Shader_Set(shader, "u_metalness_override", material->metalness);

                glActiveTexture(GL_TEXTURE0); glBindTexture(GL_TEXTURE_2D, material->diffuseMap);
                glActiveTexture(GL_TEXTURE1); glBindTexture(GL_TEXTURE_2D, material->normalMap);
                glActiveTexture(GL_TEXTURE2); glBindTexture(GL_TEXTURE_2D, material->rmaMap);
                glActiveTexture(GL_TEXTURE3); glBindTexture(GL_TEXTURE_2D, material->heightMap);

                Shader_Set(shader, "detailScale", material->detailScale);
                glActiveTexture(GL_TEXTURE7); glBindTexture(GL_TEXTURE_2D, material->detailDiffuseMap);
            }

            glBindVertexArray(mesh->VAO);
            if (shader == renderer->mainShader) {
                if (mesh->useEBO) { glDrawElements(GL_PATCHES, mesh->indexCount, GL_UNSIGNED_INT, 0); }
                else { glDrawArrays(GL_PATCHES, 0, mesh->indexCount); }
            }
            else {
                if (mesh->useEBO) { glDrawElements(GL_TRIANGLES, mesh->indexCount, GL_UNSIGNED_INT, 0); }
                else { glDrawArrays(GL_TRIANGLES, 0, mesh->indexCount); }
            }
        }
    }
}

void render_brush(Renderer* renderer, Scene* scene, GLuint shader, Brush* b, Bool is_baking_pass, const Frustum* frustum) {
    if (strcmp(b->classname, "func_clip") == 0) return;
    if (b->totalRenderVertexCount == 0) return;
    if (!Brush_IsSolid(b) && strcmp(b->classname, "func_illusionary") != 0 && strcmp(b->classname, "func_lod") != 0) return;

    Shader_Set(shader, "u_swayEnabled", 0);

    if (strcmp(b->classname, "func_lod") == 0) {
        Shader_Set(shader, "u_fadeStartDist", (Float)atof(Brush_GetProperty(b, "DisappearMinDist", "500")));
        Shader_Set(shader, "u_fadeEndDist", (Float)atof(Brush_GetProperty(b, "DisappearMaxDist", "1000")));
    }
    else {
        Shader_Set(shader, "u_fadeStartDist", 0.0f);
        Shader_Set(shader, "u_fadeEndDist", 0.0f);
    }

    Bool envMapEnabled = false;
    if (!is_baking_pass && shader == renderer->mainShader && Cvar_GetInt("r_cubemaps")) {
        Int reflection_brush_idx = FindReflectionProbeForPoint(scene, b->pos);
        if (reflection_brush_idx != -1) {
            Brush* reflection_brush = &scene->brushes[reflection_brush_idx];
            if (reflection_brush->cubemapTexture != 0) {
                glActiveTexture(GL_TEXTURE10);
                glBindTexture(GL_TEXTURE_CUBE_MAP, reflection_brush->cubemapTexture);

                Shader_Set(shader, "environmentMap", 10);
                Shader_Set(shader, "useParallaxCorrection", 1);

                Vec3 min_aabb, max_aabb;
                Brush_GetWorldAABB(reflection_brush, &min_aabb, &max_aabb);

                Shader_Set(shader, "probeBoxMin", min_aabb);
                Shader_Set(shader, "probeBoxMax", max_aabb);
                Shader_Set(shader, "probePosition", reflection_brush->pos);

                envMapEnabled = true;
            }
        }
    }

    Shader_Set(shader, "useEnvironmentMap", (Int)envMapEnabled);
    Shader_Set(shader, "useVertexLighting", (Int)b->useVertexLighting);
    Shader_Set(shader, "model", &b->modelMatrix);

    glBindVertexArray(b->vao);

    if (b->lightmapAtlasHandle != 0) {
        Shader_Set(shader, "useLightmap", 1);
        Shader_Set(shader, "lightmap", b->lightmapAtlasHandle);
    }
    else {
        Shader_Set(shader, "useLightmap", 0);
    }

    if (b->directionalLightmapAtlasHandle != 0) {
        Shader_Set(shader, "useDirectionalLightmap", 1);
        Shader_Set(shader, "directionalLightmap", b->directionalLightmapAtlasHandle);
    }
    else {
        Shader_Set(shader, "useDirectionalLightmap", 0);
    }

    if (shader == renderer->mainShader) {
        Int vbo_offset = 0;
        Int face_idx = 0;
        while (face_idx < b->numFaces) {
            BrushFace* first_face_in_batch = &b->faces[face_idx];
            if (first_face_in_batch->material == &g_NodrawMaterial) {
                vbo_offset += (first_face_in_batch->numVertexIndices - 2) * 3;
                face_idx++;
                continue;
            }

            Material* batch_material = first_face_in_batch->material;
            Material* batch_material2 = first_face_in_batch->material2;
            Material* batch_material3 = first_face_in_batch->material3;
            Material* batch_material4 = first_face_in_batch->material4;

            Int batch_start_vbo_offset = vbo_offset;
            Int batch_vertex_count = 0;

            Int current_face_in_batch_idx = face_idx;
            while (current_face_in_batch_idx < b->numFaces &&
                b->faces[current_face_in_batch_idx].material == batch_material &&
                b->faces[current_face_in_batch_idx].material2 == batch_material2 &&
                b->faces[current_face_in_batch_idx].material3 == batch_material3 &&
                b->faces[current_face_in_batch_idx].material4 == batch_material4) {

                if (b->faces[current_face_in_batch_idx].numVertexIndices >= 3) {
                    Int num_face_verts = (b->faces[current_face_in_batch_idx].numVertexIndices - 2) * 3;
                    batch_vertex_count += num_face_verts;
                }
                current_face_in_batch_idx++;
            }

            Bool isTesselationEnabledForBatch = (batch_material && batch_material->useTesselation) ||
                (batch_material2 && batch_material2->useTesselation) ||
                (batch_material3 && batch_material3->useTesselation) ||
                (batch_material4 && batch_material4->useTesselation);

            Shader_Set(shader, "u_useTesselation", (Int)isTesselationEnabledForBatch);

            Bool use_alpha_test_for_batch = (batch_material && batch_material->alpha) ||
                (batch_material2 && batch_material2->alpha) ||
                (batch_material3 && batch_material3->alpha) ||
                (batch_material4 && batch_material4->alpha);

            Shader_Set(shader, "u_useAlphaTest", (Int)use_alpha_test_for_batch);

            Bool parallaxEnabled = Cvar_GetInt("r_relief_mapping");
            Bool isParallaxEnabledForBatch = !isTesselationEnabledForBatch && parallaxEnabled && (
                (batch_material && batch_material->heightScale > 0.0f) ||
                (batch_material2 && batch_material2->heightScale > 0.0f) ||
                (batch_material3 && batch_material3->heightScale > 0.0f) ||
                (batch_material4 && batch_material4->heightScale > 0.0f)
                );
            Shader_Set(shader, "u_isParallaxEnabled", (Int)isParallaxEnabledForBatch);

            Shader_Set(shader, "heightScale", batch_material ? batch_material->heightScale : 0.0f);
            Shader_Set(shader, "u_roughness_override", batch_material ? batch_material->roughness : -1.0f);
            Shader_Set(shader, "u_metalness_override", batch_material ? batch_material->metalness : -1.0f);

            glActiveTexture(GL_TEXTURE0); glBindTexture(GL_TEXTURE_2D, batch_material ? batch_material->diffuseMap : missingTextureID);
            glActiveTexture(GL_TEXTURE1); glBindTexture(GL_TEXTURE_2D, batch_material ? batch_material->normalMap : defaultNormalMapID);
            glActiveTexture(GL_TEXTURE2); glBindTexture(GL_TEXTURE_2D, batch_material ? batch_material->rmaMap : defaultRmaMapID);
            glActiveTexture(GL_TEXTURE3); glBindTexture(GL_TEXTURE_2D, batch_material ? batch_material->heightMap : 0);

            Shader_Set(shader, "detailScale", batch_material ? batch_material->detailScale : 1.0f);
            glActiveTexture(GL_TEXTURE7); glBindTexture(GL_TEXTURE_2D, batch_material ? batch_material->detailDiffuseMap : 0);

#define BIND_MATERIAL_SLOT(slot, material) \
            if (material) { \
                Shader_Set(shader, "diffuseMap" #slot, 12 + (slot-2)*5); \
                Shader_Set(shader, "heightScale" #slot, parallaxEnabled ? material->heightScale : 0.0f); \
                glActiveTexture(GL_TEXTURE12 + (slot-2)*5); glBindTexture(GL_TEXTURE_2D, material->diffuseMap); \
                glActiveTexture(GL_TEXTURE13 + (slot-2)*5); glBindTexture(GL_TEXTURE_2D, material->normalMap); \
                glActiveTexture(GL_TEXTURE14 + (slot-2)*5); glBindTexture(GL_TEXTURE_2D, material->rmaMap); \
                glActiveTexture(GL_TEXTURE15 + (slot-2)*5); glBindTexture(GL_TEXTURE_2D, material->heightMap); \
            } else { \
                Shader_Set(shader, "heightScale" #slot, 0.0f); \
            }
            BIND_MATERIAL_SLOT(2, batch_material2);
            BIND_MATERIAL_SLOT(3, batch_material3);
            BIND_MATERIAL_SLOT(4, batch_material4);

            if (batch_vertex_count > 0) {
                if (shader == renderer->mainShader) {
                    glDrawArrays(GL_PATCHES, batch_start_vbo_offset, batch_vertex_count);
                }
                else {
                    glDrawArrays(GL_TRIANGLES, batch_start_vbo_offset, batch_vertex_count);
                }
            }

            vbo_offset += batch_vertex_count;
            face_idx = current_face_in_batch_idx;
        }
    }
    else {
        if (b->totalRenderVertexCount > 0) {
            glDrawArrays(GL_TRIANGLES, 0, b->totalRenderVertexCount);
        }
    }
}

void Geometry_RenderPass(Renderer* renderer, Scene* scene, Engine* engine, Mat4* view, Mat4* projection, const Mat4* sunLightSpaceMatrix, Vec3 cameraPos, Bool unlit, Bool is_reflection_pass) {
    Frustum frustum;
    Mat4 view_proj;
    Math::mat4_multiply(&view_proj, projection, view);
    Math::extract_frustum_planes(&view_proj, &frustum, true);

    glBindFramebuffer(GL_FRAMEBUFFER, renderer->gBufferFBO);
    glViewport(0, 0, engine->width / Cvar_GetFloat("r_geometry_downsample"), engine->height / Cvar_GetFloat("r_geometry_downsample"));

    if (Cvar_GetInt("r_zprepass") && !is_reflection_pass) {
        Zprepass_Render(renderer, scene, engine, view, projection);
        glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT);
    }
    else {
        glDepthMask(GL_TRUE);
        glDepthFunc(GL_LESS);
        glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    }

    GLuint attachments[6] = { GL_COLOR_ATTACHMENT0, GL_COLOR_ATTACHMENT1, GL_COLOR_ATTACHMENT2, GL_COLOR_ATTACHMENT3, GL_COLOR_ATTACHMENT4, GL_COLOR_ATTACHMENT5 };
    glDrawBuffers(6, attachments);

    if (Cvar_GetInt("r_faceculling")) {
        glEnable(GL_CULL_FACE);
        glCullFace(GL_BACK);
    }
    else {
        glDisable(GL_CULL_FACE);
    }

    glUseProgram(renderer->mainShader);
    glPatchParameteri(GL_PATCH_VERTICES, 3);

    Shader_Set(renderer->mainShader, "view", view);
    Shader_Set(renderer->mainShader, "projection", projection);
    Shader_Set(renderer->mainShader, "viewportSize", Vec2{ (Float)(engine->width / Cvar_GetFloat("r_geometry_downsample")), (Float)(engine->height / Cvar_GetFloat("r_geometry_downsample")) });
    Shader_Set(renderer->mainShader, "prevViewProjection", &renderer->prevViewProjection);
    Shader_Set(renderer->mainShader, "viewPos", cameraPos);
    Shader_Set(renderer->mainShader, "u_time", engine->lastFrame);
    Shader_Set(renderer->mainShader, "u_windDirection", scene->sun.windDirection);
    Shader_Set(renderer->mainShader, "u_windStrength", scene->sun.windStrength);
    Shader_Set(renderer->mainShader, "sun.enabled", (Int)scene->sun.enabled);
    Shader_Set(renderer->mainShader, "sun.direction", scene->sun.direction);
    Shader_Set(renderer->mainShader, "sun.color", scene->sun.color);
    Shader_Set(renderer->mainShader, "sun.intensity", scene->sun.intensity);
    Shader_Set(renderer->mainShader, "sunLightSpaceMatrix", sunLightSpaceMatrix);

    glActiveTexture(GL_TEXTURE11);
    glBindTexture(GL_TEXTURE_2D, renderer->sunShadowMap);
    Shader_Set(renderer->mainShader, "sunShadowMap", 11);

    Shader_Set(renderer->mainShader, "r_debug_lightmaps", Cvar_GetInt("r_debug_lightmaps"));
    Shader_Set(renderer->mainShader, "r_debug_lightmaps_directional", Cvar_GetInt("r_debug_lightmaps_directional"));
    Shader_Set(renderer->mainShader, "r_debug_vertex_light", Cvar_GetInt("r_debug_vertex_light"));
    Shader_Set(renderer->mainShader, "r_debug_vertex_light_directional", Cvar_GetInt("r_debug_vertex_light_directional"));
    Shader_Set(renderer->mainShader, "r_lightmaps_bicubic", Cvar_GetInt("r_lightmaps_bicubic"));

    glActiveTexture(GL_TEXTURE16);
    glBindTexture(GL_TEXTURE_2D, renderer->brdfLUTTexture);

    Shader_Set(renderer->mainShader, "is_unlit", Cvar_GetInt("r_fullbright"));
    Shader_Set(renderer->mainShader, "u_numAmbientProbes", scene->num_ambient_probes);
    Shader_Set(renderer->mainShader, "numActiveLights", scene->numActiveLights);
    Shader_Set(renderer->mainShader, "u_relief_max_steps", Cvar_GetFloat("r_relief_max_steps"));
    Shader_Set(renderer->mainShader, "u_relief_min_steps", Cvar_GetFloat("r_relief_min_steps"));
    Shader_Set(renderer->mainShader, "u_relief_refine_steps", Cvar_GetInt("r_relief_refine_steps"));

    ShaderLight dynamic_lights[Common::MAX_LIGHTS];
    Int num_dynamic_lights = 0;

    for (Int i = 0; i < scene->numActiveLights; ++i) {
        Light* light = &scene->lights[i];
        if (light->is_static == 1) continue;
        if (light->intensity <= 0.0f) continue;
        ShaderLight* shader_light = &dynamic_lights[num_dynamic_lights];
        shader_light->position.x = light->pos.x;
        shader_light->position.y = light->pos.y;
        shader_light->position.z = light->pos.z;
        shader_light->position.w = (Float)light->type;
        shader_light->direction.x = light->direction.x;
        shader_light->direction.y = light->direction.y;
        shader_light->direction.z = light->direction.z;
        shader_light->color.x = light->color.x;
        shader_light->color.y = light->color.y;
        shader_light->color.z = light->color.z;
        shader_light->color.w = light->intensity;
        shader_light->params1.x = light->radius;
        shader_light->params1.y = light->cutOff;
        shader_light->params1.z = light->outerCutOff;
        shader_light->params2.x = light->shadowFarPlane;
        shader_light->params2.y = light->shadowBias;
        shader_light->params2.z = light->volumetricIntensity;
        shader_light->shadowMapHandle[0] = (Uint)(light->shadowMapHandle & 0xFFFFFFFF);
        shader_light->shadowMapHandle[1] = (Uint)(light->shadowMapHandle >> 32);
        if (light->cookieMapHandle != 0) {
            shader_light->cookieMapHandle[0] = (Uint)(light->cookieMapHandle & 0xFFFFFFFF);
            shader_light->cookieMapHandle[1] = (Uint)(light->cookieMapHandle >> 32);
        }
        else {
            shader_light->cookieMapHandle[0] = 0;
            shader_light->cookieMapHandle[1] = 0;
        }
        num_dynamic_lights++;
    }

    Shader_Set(renderer->mainShader, "numActiveLights", num_dynamic_lights);
    if (num_dynamic_lights > 0) {
        glBindBuffer(GL_SHADER_STORAGE_BUFFER, renderer->lightSSBO);
        glBufferSubData(GL_SHADER_STORAGE_BUFFER, 0, num_dynamic_lights * sizeof(ShaderLight), dynamic_lights);
        glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0);
    }

    Shader_Set(renderer->mainShader, "flashlight.enabled", (Int)engine->flashlight_on);
    if (engine->flashlight_on) {
        Vec3 forward = { cosf(engine->camera.pitch) * sinf(engine->camera.yaw), sinf(engine->camera.pitch), -cosf(engine->camera.pitch) * cosf(engine->camera.yaw) };
        Math::vec3_normalize(&forward);
        Shader_Set(renderer->mainShader, "flashlight.position", engine->camera.position);
        Shader_Set(renderer->mainShader, "flashlight.direction", forward);
    }

    for (Int i = 0; i < scene->numObjects; i++) {
        SceneObject* obj = &scene->objects[i];
        Shader_Set(renderer->mainShader, "isBrush", 0);
        if (obj->model) {
            if (obj->mass > 0.0f && scene->num_ambient_probes > 0) {
                AmbientProbe* nearest_probes[8] = { nullptr };
                Float distances[8];
                for (Int k = 0; k < 8; ++k) distances[k] = FLT_MAX;
                for (Int p_idx = 0; p_idx < scene->num_ambient_probes; ++p_idx) {
                    Float d = Math::vec3_length_sq(Math::vec3_sub(obj->pos, scene->ambient_probes[p_idx].position));
                    for (Int k = 0; k < 8; ++k) {
                        if (d < distances[k]) {
                            for (Int l = 7; l > k; --l) {
                                distances[l] = distances[l - 1];
                                nearest_probes[l] = nearest_probes[l - 1];
                            }
                            distances[k] = d;
                            nearest_probes[k] = &scene->ambient_probes[p_idx];
                            break;
                        }
                    }
                }
                for (Int k = 0; k < 8; ++k) {
                    Char buf[64];
                    if (nearest_probes[k]) {
                        sprintf(buf, "u_probes[%d].position", k);
                        Shader_Set(renderer->mainShader, buf, nearest_probes[k]->position);
                        for (Int f = 0; f < 6; ++f) {
                            sprintf(buf, "u_probes[%d].colors[%d]", k, f);
                            Shader_Set(renderer->mainShader, buf, nearest_probes[k]->colors[f]);
                        }
                        sprintf(buf, "u_probes[%d].dominant_direction", k);
                        Shader_Set(renderer->mainShader, buf, nearest_probes[k]->dominant_direction);
                    }
                }
            }
            Vec3 local_corners[8] = {
                {obj->model->aabb_min.x, obj->model->aabb_min.y, obj->model->aabb_min.z}, {obj->model->aabb_max.x, obj->model->aabb_min.y, obj->model->aabb_min.z},
                {obj->model->aabb_min.x, obj->model->aabb_max.y, obj->model->aabb_min.z}, {obj->model->aabb_max.x, obj->model->aabb_max.y, obj->model->aabb_min.z},
                {obj->model->aabb_min.x, obj->model->aabb_min.y, obj->model->aabb_max.z}, {obj->model->aabb_max.x, obj->model->aabb_min.y, obj->model->aabb_max.z},
                {obj->model->aabb_min.x, obj->model->aabb_max.y, obj->model->aabb_max.z}, {obj->model->aabb_max.x, obj->model->aabb_max.y, obj->model->aabb_max.z}
            };
            Vec3 world_aabb_min = { FLT_MAX, FLT_MAX, FLT_MAX };
            Vec3 world_aabb_max = { -FLT_MAX, -FLT_MAX, -FLT_MAX };
            for (Int j = 0; j < 8; ++j) {
                Vec3 transformed_corner = Math::mat4_mul_vec3(&obj->modelMatrix, local_corners[j]);
                world_aabb_min.x = fminf(world_aabb_min.x, transformed_corner.x);
                world_aabb_min.y = fminf(world_aabb_min.y, transformed_corner.y);
                world_aabb_min.z = fminf(world_aabb_min.z, transformed_corner.z);
                world_aabb_max.x = fmaxf(world_aabb_max.x, transformed_corner.x);
                world_aabb_max.y = fmaxf(world_aabb_max.y, transformed_corner.y);
                world_aabb_max.z = fmaxf(world_aabb_max.z, transformed_corner.z);
            }
            if (!Math::frustum_check_aabb(&frustum, world_aabb_min, world_aabb_max)) {
                continue;
            }
        }
        render_object(renderer, scene, renderer->mainShader, &scene->objects[i], false, &frustum);
    }

    for (Int i = 0; i < scene->numBrushes; i++) {
        Brush* b = &scene->brushes[i];
        if (strcmp(b->classname, "func_wall_toggle") == 0 && !b->runtime_is_visible) continue;
        Shader_Set(renderer->mainShader, "isBrush", 1);
        if (strcmp(b->classname, "func_water") == 0) continue;
        if (strcmp(b->classname, "env_glass") == 0) continue;
        if (b->numVertices > 0) {
            Vec3 min_v = { FLT_MAX, FLT_MAX, FLT_MAX };
            Vec3 max_v = { -FLT_MAX, -FLT_MAX, -FLT_MAX };
            for (Int j = 0; j < b->numVertices; ++j) {
                Vec3 p = Math::mat4_mul_vec3(&b->modelMatrix, b->vertices[j].pos);
                min_v.x = fminf(min_v.x, p.x); min_v.y = fminf(min_v.y, p.y); min_v.z = fminf(min_v.z, p.z);
                max_v.x = fmaxf(max_v.x, p.x); max_v.y = fmaxf(max_v.y, p.y); max_v.z = fmaxf(max_v.z, p.z);
            }
            if (!Math::frustum_check_aabb(&frustum, min_v, max_v)) {
                continue;
            }
        }
        render_brush(renderer, scene, renderer->mainShader, &scene->brushes[i], false, &frustum);
    }

    MiscRender_ParallaxRooms(renderer, scene, engine, view, projection);
    Decals_Render(scene, renderer, renderer->mainShader);

    for (Int i = 0; i < scene->numVideoPlayers; ++i) {
        VideoPlayer_Render(&scene->videoPlayers[i], view, projection);
    }

    glEnable(GL_BLEND);
    glDepthMask(GL_FALSE);

    if (Cvar_GetInt("r_particles")) {
        glDrawBuffer(GL_COLOR_ATTACHMENT0);
        Float scrW = (Float)(engine->width / Cvar_GetFloat("r_geometry_downsample"));
        Float scrH = (Float)(engine->height / Cvar_GetFloat("r_geometry_downsample"));

        for (Int i = 0; i < scene->numParticleEmitters; ++i) {
            ParticleEmitter_Render(&scene->particleEmitters[i], (void*)scene, (void*)engine, *view, *projection, renderer->gPosition, scrW, scrH);
        }
        glDrawBuffers(6, attachments);
    }

    if (Cvar_GetInt("r_sprites")) {
        Sprites_Render(renderer, scene, view, projection);
    }

    glDepthMask(GL_TRUE);
    glDisable(GL_BLEND);

    if (Cvar_GetInt("r_faceculling")) {
        glDisable(GL_CULL_FACE);
    }
    if (Cvar_GetInt("r_zprepass") && !is_reflection_pass) {
        glDepthFunc(GL_LESS);
    }
    glDepthMask(GL_TRUE);
    glDisable(GL_BLEND);
    glBindVertexArray(0);

    Beams_Render(scene, *view, *projection, cameraPos, engine->scaledTime);
    Cable_Render(scene, *view, *projection, cameraPos, engine->scaledTime);
    Glow_Render(scene, *view, *projection);

    if (Cvar_GetInt("r_wireframe")) {
        glUseProgram(renderer->wireframeShader);
        Shader_Set(renderer->wireframeShader, "view", view);
        Shader_Set(renderer->wireframeShader, "projection", projection);
        Shader_Set(renderer->wireframeShader, "wireframeColor", Vec4{ 0.0f, 0.5f, 1.0f, 1.0f });

        glDisable(GL_DEPTH_TEST);
        for (Int i = 0; i < scene->numObjects; i++) {
            SceneObject* obj = &scene->objects[i];
            Shader_Set(renderer->wireframeShader, "model", &obj->modelMatrix);
            if (obj->model) {
                for (Int meshIdx = 0; meshIdx < obj->model->meshCount; ++meshIdx) {
                    Mesh* mesh = &obj->model->meshes[meshIdx];
                    glBindVertexArray(mesh->VAO);
                    if (mesh->useEBO) {
                        glDrawElements(GL_TRIANGLES, mesh->indexCount, GL_UNSIGNED_INT, 0);
                    }
                    else {
                        glDrawArrays(GL_TRIANGLES, 0, mesh->indexCount);
                    }
                }
            }
        }
        for (Int i = 0; i < scene->numBrushes; i++) {
            Brush* b = &scene->brushes[i];
            if (!Brush_IsSolid(b)) continue;
            Shader_Set(renderer->wireframeShader, "model", &b->modelMatrix);
            glBindVertexArray(b->vao);
            glDrawArrays(GL_TRIANGLES, 0, b->totalRenderVertexCount);
        }
        glEnable(GL_DEPTH_TEST);
    }

    glBindFramebuffer(GL_FRAMEBUFFER, 0);
}