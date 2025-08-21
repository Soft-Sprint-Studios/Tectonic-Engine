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
#include "gl_geometry.h"
#include "gl_misc.h"
#include "cvar.h"
#include <float.h>
#include "io_system.h"
#include "gl_decals.h"
#include "gl_beams.h"
#include "gl_cables.h"
#include "gl_glow.h"

static int FindReflectionProbeForPoint(Scene* scene, Vec3 p) {
    for (int i = 0; i < scene->numBrushes; ++i) {
        Brush* b = &scene->brushes[i];
        if (strcmp(b->classname, "env_reflectionprobe") != 0) {
            continue;
        }
        if (b->numVertices == 0 || b->vertices == NULL) continue;

        Vec3 min_aabb_world = { FLT_MAX, FLT_MAX, FLT_MAX };
        Vec3 max_aabb_world = { -FLT_MAX, -FLT_MAX, -FLT_MAX };

        for (int j = 0; j < b->numVertices; ++j) {
            Vec3 world_v = mat4_mul_vec3(&b->modelMatrix, b->vertices[j].pos);
            min_aabb_world.x = fminf(min_aabb_world.x, world_v.x);
            min_aabb_world.y = fminf(min_aabb_world.y, world_v.y);
            min_aabb_world.z = fminf(min_aabb_world.z, world_v.z);
            max_aabb_world.x = fmaxf(max_aabb_world.x, world_v.x);
            max_aabb_world.y = fmaxf(max_aabb_world.y, world_v.y);
            max_aabb_world.z = fmaxf(max_aabb_world.z, world_v.z);
        }

        if (p.x >= min_aabb_world.x && p.x <= max_aabb_world.x &&
            p.y >= min_aabb_world.y && p.y <= max_aabb_world.y &&
            p.z >= min_aabb_world.z && p.z <= max_aabb_world.z)
        {
            return i;
        }
    }
    return -1;
}

void render_object(Renderer* renderer, Scene* scene, GLuint shader, SceneObject* obj, bool is_baking_pass, const Frustum* frustum) {
    bool envMapEnabled = false;

    if (!is_baking_pass && shader == renderer->mainShader && Cvar_GetInt("r_cubemaps")) {
        int reflection_brush_idx = FindReflectionProbeForPoint(scene, obj->pos);
        if (reflection_brush_idx != -1) {
            Brush* reflection_brush = &scene->brushes[reflection_brush_idx];
            if (reflection_brush->cubemapTexture != 0) {
                glActiveTexture(GL_TEXTURE10);
                glBindTexture(GL_TEXTURE_CUBE_MAP, reflection_brush->cubemapTexture);
                glUniform1i(glGetUniformLocation(shader, "environmentMap"), 10);
                glUniform1i(glGetUniformLocation(shader, "useParallaxCorrection"), 1);

                Vec3 min_aabb = { FLT_MAX, FLT_MAX, FLT_MAX };
                Vec3 max_aabb = { -FLT_MAX, -FLT_MAX, -FLT_MAX };
                for (int i = 0; i < reflection_brush->numVertices; ++i) {
                    Vec3 world_v = mat4_mul_vec3(&reflection_brush->modelMatrix, reflection_brush->vertices[i].pos);
                    min_aabb.x = fminf(min_aabb.x, world_v.x); min_aabb.y = fminf(min_aabb.y, world_v.y); min_aabb.z = fminf(min_aabb.z, world_v.z);
                    max_aabb.x = fmaxf(max_aabb.x, world_v.x); max_aabb.y = fmaxf(max_aabb.y, world_v.y); max_aabb.z = fmaxf(max_aabb.z, world_v.z);
                }
                glUniform3fv(glGetUniformLocation(shader, "probeBoxMin"), 1, &min_aabb.x);
                glUniform3fv(glGetUniformLocation(shader, "probeBoxMax"), 1, &max_aabb.x);
                glUniform3fv(glGetUniformLocation(shader, "probePosition"), 1, &reflection_brush->pos.x);
                envMapEnabled = true;
            }
        }
    }

    glUniform1i(glGetUniformLocation(shader, "useEnvironmentMap"), envMapEnabled);

    if (shader == renderer->mainShader) {
        bool is_skinnable = obj->model && obj->model->num_skins > 0;
        glUniform1i(glGetUniformLocation(shader, "u_hasAnimation"), is_skinnable);
        if (is_skinnable && obj->bone_matrices) {
            glUniformMatrix4fv(glGetUniformLocation(shader, "u_boneMatrices"), obj->model->skins[0].num_joints, GL_FALSE, (const GLfloat*)obj->bone_matrices);
        }
    }

    glUniform1f(glGetUniformLocation(shader, "u_fadeStartDist"), obj->fadeStartDist);
    glUniform1f(glGetUniformLocation(shader, "u_fadeEndDist"), obj->fadeEndDist);

    Mat4 finalModelMatrix = obj->modelMatrix;
    if (obj->model && obj->model->num_animations > 0 && obj->model->num_skins == 0) {
        mat4_multiply(&finalModelMatrix, &obj->modelMatrix, &obj->animated_local_transform);
    }
    glUniformMatrix4fv(glGetUniformLocation(shader, "model"), 1, GL_FALSE, finalModelMatrix.m);

    glUniform1i(glGetUniformLocation(shader, "u_swayEnabled"), obj->swayEnabled);
    if (obj->model) {
        if (obj->bakedVertexColors || obj->bakedVertexDirections) {
            unsigned int vertex_offset = 0;
            const int stride_floats = 24;
            for (int i = 0; i < obj->model->meshCount; ++i) {
                Mesh* mesh = &obj->model->meshes[i];
                for (unsigned int v = 0; v < mesh->vertexCount; ++v) {
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
                free(obj->bakedVertexColors);
                obj->bakedVertexColors = NULL;
            }
            if (obj->bakedVertexDirections) {
                free(obj->bakedVertexDirections);
                obj->bakedVertexDirections = NULL;
            }
        }
        for (int i = 0; i < obj->model->meshCount; ++i) {
            Mesh* mesh = &obj->model->meshes[i];
            Material* material = mesh->material;
            if (shader == renderer->mainShader) {
                bool isTesselationEnabled = material->useTesselation;
                glUniform1i(glGetUniformLocation(shader, "u_useTesselation"), isTesselationEnabled);

                bool parallaxEnabledForThisMesh = !isTesselationEnabled && Cvar_GetInt("r_relief_mapping") && material->heightScale > 0.0f;
                glUniform1i(glGetUniformLocation(shader, "u_isParallaxEnabled"), parallaxEnabledForThisMesh);
                glUniform1f(glGetUniformLocation(shader, "heightScale"), material->heightScale);
                glUniform1f(glGetUniformLocation(shader, "u_roughness_override"), material->roughness);
                glUniform1f(glGetUniformLocation(shader, "u_metalness_override"), material->metalness);
                glActiveTexture(GL_TEXTURE0); glBindTexture(GL_TEXTURE_2D, material->diffuseMap);
                glActiveTexture(GL_TEXTURE1); glBindTexture(GL_TEXTURE_2D, material->normalMap);
                glActiveTexture(GL_TEXTURE2); glBindTexture(GL_TEXTURE_2D, material->rmaMap);
                glActiveTexture(GL_TEXTURE3); glBindTexture(GL_TEXTURE_2D, material->heightMap);
                glUniform1f(glGetUniformLocation(shader, "detailScale"), material->detailScale);
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

void render_brush(Renderer* renderer, Scene* scene, GLuint shader, Brush* b, bool is_baking_pass, const Frustum* frustum) {
    if (strcmp(b->classname, "func_clip") == 0) return;
    if (b->totalRenderVertexCount == 0) return;
    if (!Brush_IsSolid(b) && strcmp(b->classname, "func_illusionary") != 0 && strcmp(b->classname, "func_lod") != 0) return;

    glUniform1i(glGetUniformLocation(shader, "u_swayEnabled"), 0);

    if (strcmp(b->classname, "func_lod") == 0) {
        glUniform1f(glGetUniformLocation(shader, "u_fadeStartDist"), atof(Brush_GetProperty(b, "DisappearMinDist", "500")));
        glUniform1f(glGetUniformLocation(shader, "u_fadeEndDist"), atof(Brush_GetProperty(b, "DisappearMaxDist", "1000")));
    }
    else {
        glUniform1f(glGetUniformLocation(shader, "u_fadeStartDist"), 0.0f);
        glUniform1f(glGetUniformLocation(shader, "u_fadeEndDist"), 0.0f);
    }

    bool envMapEnabled = false;
    if (!is_baking_pass && shader == renderer->mainShader && Cvar_GetInt("r_cubemaps")) {
        int reflection_brush_idx = FindReflectionProbeForPoint(scene, b->pos);
        if (reflection_brush_idx != -1) {
            Brush* reflection_brush = &scene->brushes[reflection_brush_idx];
            if (reflection_brush->cubemapTexture != 0) {
                glActiveTexture(GL_TEXTURE10);
                glBindTexture(GL_TEXTURE_CUBE_MAP, reflection_brush->cubemapTexture);
                glUniform1i(glGetUniformLocation(shader, "environmentMap"), 10);
                glUniform1i(glGetUniformLocation(shader, "useParallaxCorrection"), 1);
                Vec3 min_aabb = { FLT_MAX, FLT_MAX, FLT_MAX };
                Vec3 max_aabb = { -FLT_MAX, -FLT_MAX, -FLT_MAX };
                for (int i = 0; i < reflection_brush->numVertices; ++i) {
                    Vec3 world_v = mat4_mul_vec3(&reflection_brush->modelMatrix, reflection_brush->vertices[i].pos);
                    min_aabb.x = fminf(min_aabb.x, world_v.x); min_aabb.y = fminf(min_aabb.y, world_v.y); min_aabb.z = fminf(min_aabb.z, world_v.z);
                    max_aabb.x = fmaxf(max_aabb.x, world_v.x); max_aabb.y = fmaxf(max_aabb.y, world_v.y); max_aabb.z = fmaxf(max_aabb.z, world_v.z);
                }
                glUniform3fv(glGetUniformLocation(shader, "probeBoxMin"), 1, &min_aabb.x);
                glUniform3fv(glGetUniformLocation(shader, "probeBoxMax"), 1, &max_aabb.x);
                glUniform3fv(glGetUniformLocation(shader, "probePosition"), 1, &reflection_brush->pos.x);
                envMapEnabled = true;
            }
        }
    }
    glUniform1i(glGetUniformLocation(shader, "useEnvironmentMap"), envMapEnabled);

    glUniformMatrix4fv(glGetUniformLocation(shader, "model"), 1, GL_FALSE, b->modelMatrix.m);
    glBindVertexArray(b->vao);

    if (b->lightmapAtlas != 0) {
        glUniform1i(glGetUniformLocation(shader, "useLightmap"), 1);
        glActiveTexture(GL_TEXTURE5);
        glBindTexture(GL_TEXTURE_2D, b->lightmapAtlas);
        glUniform1i(glGetUniformLocation(shader, "lightmap"), 5);
    }
    else {
        glUniform1i(glGetUniformLocation(shader, "useLightmap"), 0);
    }

    if (b->directionalLightmapAtlas != 0) {
        glUniform1i(glGetUniformLocation(shader, "useDirectionalLightmap"), 1);
        glActiveTexture(GL_TEXTURE6);
        glBindTexture(GL_TEXTURE_2D, b->directionalLightmapAtlas);
        glUniform1i(glGetUniformLocation(shader, "directionalLightmap"), 6);
    }
    else {
        glUniform1i(glGetUniformLocation(shader, "useDirectionalLightmap"), 0);
    }

    if (shader == renderer->mainShader) {
        int vbo_offset = 0;
        int face_idx = 0;
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

            int batch_start_vbo_offset = vbo_offset;
            int batch_vertex_count = 0;

            int current_face_in_batch_idx = face_idx;
            while (current_face_in_batch_idx < b->numFaces &&
                b->faces[current_face_in_batch_idx].material == batch_material &&
                b->faces[current_face_in_batch_idx].material2 == batch_material2 &&
                b->faces[current_face_in_batch_idx].material3 == batch_material3 &&
                b->faces[current_face_in_batch_idx].material4 == batch_material4) {

                if (strlen(b->faces[current_face_in_batch_idx].blendMapPath) > 0) {
                    if (b->faces[current_face_in_batch_idx].blendMapTexture == 0) {
                        b->faces[current_face_in_batch_idx].blendMapTexture = loadTexture(b->faces[current_face_in_batch_idx].blendMapPath, false, TEXTURE_LOAD_CONTEXT_WORLD);
                    }
                }

                if (b->faces[current_face_in_batch_idx].numVertexIndices >= 3) {
                    int num_face_verts = (b->faces[current_face_in_batch_idx].numVertexIndices - 2) * 3;
                    batch_vertex_count += num_face_verts;
                }
                current_face_in_batch_idx++;
            }

            bool isTesselationEnabledForBatch = (batch_material && batch_material->useTesselation) ||
                (batch_material2 && batch_material2->useTesselation) ||
                (batch_material3 && batch_material3->useTesselation) ||
                (batch_material4 && batch_material4->useTesselation);

            glUniform1i(glGetUniformLocation(shader, "u_useTesselation"), isTesselationEnabledForBatch);

            bool parallaxEnabled = Cvar_GetInt("r_relief_mapping");
            bool isParallaxEnabledForBatch = !isTesselationEnabledForBatch && parallaxEnabled && (
                (batch_material && batch_material->heightScale > 0.0f) ||
                (batch_material2 && batch_material2->heightScale > 0.0f) ||
                (batch_material3 && batch_material3->heightScale > 0.0f) ||
                (batch_material4 && batch_material4->heightScale > 0.0f)
                );
            glUniform1i(glGetUniformLocation(shader, "u_isParallaxEnabled"), isParallaxEnabledForBatch);

            glUniform1f(glGetUniformLocation(shader, "heightScale"), batch_material ? batch_material->heightScale : 0.0f);
            glUniform1f(glGetUniformLocation(shader, "u_roughness_override"), batch_material ? batch_material->roughness : -1.0f);
            glUniform1f(glGetUniformLocation(shader, "u_metalness_override"), batch_material ? batch_material->metalness : -1.0f);
            glActiveTexture(GL_TEXTURE0); glBindTexture(GL_TEXTURE_2D, batch_material ? batch_material->diffuseMap : missingTextureID);
            glActiveTexture(GL_TEXTURE1); glBindTexture(GL_TEXTURE_2D, batch_material ? batch_material->normalMap : defaultNormalMapID);
            glActiveTexture(GL_TEXTURE2); glBindTexture(GL_TEXTURE_2D, batch_material ? batch_material->rmaMap : defaultRmaMapID);
            glActiveTexture(GL_TEXTURE3); glBindTexture(GL_TEXTURE_2D, batch_material ? batch_material->heightMap : 0);
            if (first_face_in_batch->blendMapTexture != 0) {
                glUniform1i(glGetUniformLocation(shader, "useBlendMap"), 1);
                glActiveTexture(GL_TEXTURE9);
                glBindTexture(GL_TEXTURE_2D, first_face_in_batch->blendMapTexture);
                glUniform1i(glGetUniformLocation(shader, "blendMap"), 9);
            }
            else {
                glUniform1i(glGetUniformLocation(shader, "useBlendMap"), 0);
            }
            glUniform1f(glGetUniformLocation(shader, "detailScale"), batch_material ? batch_material->detailScale : 1.0f);
            glActiveTexture(GL_TEXTURE7); glBindTexture(GL_TEXTURE_2D, batch_material ? batch_material->detailDiffuseMap : 0);

#define BIND_MATERIAL_SLOT(slot, material) \
            if (material) { \
                glUniform1i(glGetUniformLocation(shader, "diffuseMap" #slot), 12 + (slot-2)*5); \
                glUniform1f(glGetUniformLocation(shader, "heightScale" #slot), parallaxEnabled ? material->heightScale : 0.0f); \
                glActiveTexture(GL_TEXTURE12 + (slot-2)*5); glBindTexture(GL_TEXTURE_2D, material->diffuseMap); \
                glActiveTexture(GL_TEXTURE13 + (slot-2)*5); glBindTexture(GL_TEXTURE_2D, material->normalMap); \
                glActiveTexture(GL_TEXTURE14 + (slot-2)*5); glBindTexture(GL_TEXTURE_2D, material->rmaMap); \
                glActiveTexture(GL_TEXTURE15 + (slot-2)*5); glBindTexture(GL_TEXTURE_2D, material->heightMap); \
            } else { \
                glUniform1f(glGetUniformLocation(shader, "heightScale" #slot), 0.0f); \
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

void Geometry_RenderPass(Renderer* renderer, Scene* scene, Engine* engine, Mat4* view, Mat4* projection, const Mat4* sunLightSpaceMatrix, Vec3 cameraPos, bool unlit) {
    Frustum frustum;
    Mat4 view_proj;
    mat4_multiply(&view_proj, projection, view);
    extract_frustum_planes(&view_proj, &frustum, true);

    glBindFramebuffer(GL_FRAMEBUFFER, renderer->gBufferFBO);
    glViewport(0, 0, engine->width / GEOMETRY_PASS_DOWNSAMPLE_FACTOR, engine->height / GEOMETRY_PASS_DOWNSAMPLE_FACTOR);

    if (Cvar_GetInt("r_zprepass")) {
        Zprepass_Render(renderer, scene, engine, view, projection);
    }
    else {
        glDepthMask(GL_TRUE);
        glDepthFunc(GL_LESS);
    }

    glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
    if (!Cvar_GetInt("r_zprepass")) {
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    }
    else {
        glClear(GL_COLOR_BUFFER_BIT);
    }

    GLuint attachments[7] = { GL_COLOR_ATTACHMENT0, GL_COLOR_ATTACHMENT1, GL_COLOR_ATTACHMENT2, GL_COLOR_ATTACHMENT3, GL_COLOR_ATTACHMENT4, GL_COLOR_ATTACHMENT5, GL_COLOR_ATTACHMENT6 };
    glDrawBuffers(7, attachments);

    if (Cvar_GetInt("r_faceculling")) {
        glEnable(GL_CULL_FACE);
        glCullFace(GL_BACK);
    }
    else {
        glDisable(GL_CULL_FACE);
    }

    glUseProgram(renderer->mainShader);
    glPatchParameteri(GL_PATCH_VERTICES, 3);
    glUniformMatrix4fv(glGetUniformLocation(renderer->mainShader, "view"), 1, GL_FALSE, view->m);
    glUniformMatrix4fv(glGetUniformLocation(renderer->mainShader, "projection"), 1, GL_FALSE, projection->m);
    glUniform2f(glGetUniformLocation(renderer->mainShader, "viewportSize"), (float)(engine->width / GEOMETRY_PASS_DOWNSAMPLE_FACTOR), (float)(engine->height / GEOMETRY_PASS_DOWNSAMPLE_FACTOR));
    glUniformMatrix4fv(glGetUniformLocation(renderer->mainShader, "prevViewProjection"), 1, GL_FALSE, renderer->prevViewProjection.m);
    glUniform3fv(glGetUniformLocation(renderer->mainShader, "viewPos"), 1, &cameraPos.x);
    glUniform1f(glGetUniformLocation(renderer->mainShader, "u_time"), engine->lastFrame);
    glUniform3fv(glGetUniformLocation(renderer->mainShader, "u_windDirection"), 1, &scene->sun.windDirection.x);
    glUniform1f(glGetUniformLocation(renderer->mainShader, "u_windStrength"), scene->sun.windStrength);
    glUniform1i(glGetUniformLocation(renderer->mainShader, "sun.enabled"), scene->sun.enabled);
    glUniform3fv(glGetUniformLocation(renderer->mainShader, "sun.direction"), 1, &scene->sun.direction.x);
    glUniform3fv(glGetUniformLocation(renderer->mainShader, "sun.color"), 1, &scene->sun.color.x);
    glUniform1f(glGetUniformLocation(renderer->mainShader, "sun.intensity"), scene->sun.intensity);
    glUniformMatrix4fv(glGetUniformLocation(renderer->mainShader, "sunLightSpaceMatrix"), 1, GL_FALSE, sunLightSpaceMatrix->m);
    glActiveTexture(GL_TEXTURE11);
    glBindTexture(GL_TEXTURE_2D, renderer->sunShadowMap);
    glUniform1i(glGetUniformLocation(renderer->mainShader, "sunShadowMap"), 11);
    glUniform1i(glGetUniformLocation(renderer->mainShader, "r_debug_lightmaps"), Cvar_GetInt("r_debug_lightmaps"));
    glUniform1i(glGetUniformLocation(renderer->mainShader, "r_debug_lightmaps_directional"), Cvar_GetInt("r_debug_lightmaps_directional"));
    glUniform1i(glGetUniformLocation(renderer->mainShader, "r_debug_vertex_light"), Cvar_GetInt("r_debug_vertex_light"));
    glUniform1i(glGetUniformLocation(renderer->mainShader, "r_debug_vertex_light_directional"), Cvar_GetInt("r_debug_vertex_light_directional"));
    glUniform1i(glGetUniformLocation(renderer->mainShader, "r_lightmaps_bicubic"), Cvar_GetInt("r_lightmaps_bicubic"));
    glUniform1i(glGetUniformLocation(renderer->mainShader, "is_unlit"), 0);
    glActiveTexture(GL_TEXTURE16);
    glBindTexture(GL_TEXTURE_2D, renderer->brdfLUTTexture);
    glUniform1i(glGetUniformLocation(renderer->mainShader, "is_unlit"), unlit);
    glUniform1i(glGetUniformLocation(renderer->mainShader, "u_numAmbientProbes"), scene->num_ambient_probes);
    glUniform1i(glGetUniformLocation(renderer->mainShader, "numActiveLights"), scene->numActiveLights);

    ShaderLight dynamic_lights[MAX_LIGHTS];
    int num_dynamic_lights = 0;

    for (int i = 0; i < scene->numActiveLights; ++i) {
        Light* light = &scene->lights[i];
        if (light->is_static) continue;
        if (light->intensity <= 0.0f) continue;
        ShaderLight* shader_light = &dynamic_lights[num_dynamic_lights];
        shader_light->position.x = light->position.x;
        shader_light->position.y = light->position.y;
        shader_light->position.z = light->position.z;
        shader_light->position.w = (float)light->type;
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
        shader_light->params2.z = light->volumetricIntensity / 100.0f;
        shader_light->shadowMapHandle[0] = (unsigned int)(light->shadowMapHandle & 0xFFFFFFFF);
        shader_light->shadowMapHandle[1] = (unsigned int)(light->shadowMapHandle >> 32);
        if (light->cookieMapHandle != 0) {
            shader_light->cookieMapHandle[0] = (unsigned int)(light->cookieMapHandle & 0xFFFFFFFF);
            shader_light->cookieMapHandle[1] = (unsigned int)(light->cookieMapHandle >> 32);
        } else {
            shader_light->cookieMapHandle[0] = 0;
            shader_light->cookieMapHandle[1] = 0;
        }
        num_dynamic_lights++;
    }

    glUniform1i(glGetUniformLocation(renderer->mainShader, "numActiveLights"), num_dynamic_lights);
    if (num_dynamic_lights > 0) {
        glBindBuffer(GL_SHADER_STORAGE_BUFFER, renderer->lightSSBO);
        glBufferSubData(GL_SHADER_STORAGE_BUFFER, 0, num_dynamic_lights * sizeof(ShaderLight), dynamic_lights);
        glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0);
    }

    glUniform1i(glGetUniformLocation(renderer->mainShader, "flashlight.enabled"), engine->flashlight_on);
    if (engine->flashlight_on) {
        Vec3 forward = { cosf(engine->camera.pitch) * sinf(engine->camera.yaw), sinf(engine->camera.pitch), -cosf(engine->camera.pitch) * cosf(engine->camera.yaw) };
        vec3_normalize(&forward);
        glUniform3fv(glGetUniformLocation(renderer->mainShader, "flashlight.position"), 1, &engine->camera.position.x);
        glUniform3fv(glGetUniformLocation(renderer->mainShader, "flashlight.direction"), 1, &forward.x);
    }
    for (int i = 0; i < scene->numObjects; i++) {
        SceneObject* obj = &scene->objects[i];
        glUniform1i(glGetUniformLocation(renderer->mainShader, "isBrush"), 0);
        if (obj->model) {
            if (obj->mass > 0.0f && scene->num_ambient_probes > 0) {
                AmbientProbe* nearest_probes[8] = { NULL };
                float distances[8];
                for (int k = 0; k < 8; ++k) distances[k] = FLT_MAX;
                for (int p_idx = 0; p_idx < scene->num_ambient_probes; ++p_idx) {
                    float d = vec3_length_sq(vec3_sub(obj->pos, scene->ambient_probes[p_idx].position));
                    for (int k = 0; k < 8; ++k) {
                        if (d < distances[k]) {
                            for (int l = 7; l > k; --l) {
                                distances[l] = distances[l - 1];
                                nearest_probes[l] = nearest_probes[l - 1];
                            }
                            distances[k] = d;
                            nearest_probes[k] = &scene->ambient_probes[p_idx];
                            break;
                        }
                    }
                }
                for (int k = 0; k < 8; ++k) {
                    char buf[64];
                    if (nearest_probes[k]) {
                        sprintf(buf, "u_probes[%d].position", k);
                        glUniform3fv(glGetUniformLocation(renderer->mainShader, buf), 1, &nearest_probes[k]->position.x);
                        for (int f = 0; f < 6; ++f) {
                            sprintf(buf, "u_probes[%d].colors[%d]", k, f);
                            glUniform3fv(glGetUniformLocation(renderer->mainShader, buf), 1, &nearest_probes[k]->colors[f].x);
                        }
                        sprintf(buf, "u_probes[%d].dominant_direction", k);
                        glUniform3fv(glGetUniformLocation(renderer->mainShader, buf), 1, &nearest_probes[k]->dominant_direction.x);
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
            for (int j = 0; j < 8; ++j) {
                Vec3 transformed_corner = mat4_mul_vec3(&obj->modelMatrix, local_corners[j]);
                world_aabb_min.x = fminf(world_aabb_min.x, transformed_corner.x);
                world_aabb_min.y = fminf(world_aabb_min.y, transformed_corner.y);
                world_aabb_min.z = fminf(world_aabb_min.z, transformed_corner.z);
                world_aabb_max.x = fmaxf(world_aabb_max.x, transformed_corner.x);
                world_aabb_max.y = fmaxf(world_aabb_max.y, transformed_corner.y);
                world_aabb_max.z = fmaxf(world_aabb_max.z, transformed_corner.z);
            }
            if (!frustum_check_aabb(&frustum, world_aabb_min, world_aabb_max)) {
                continue;
            }
        }
        render_object(renderer, scene, renderer->mainShader, &scene->objects[i], false, &frustum);
    }
    for (int i = 0; i < scene->numBrushes; i++) {
        Brush* b = &scene->brushes[i];
        if (strcmp(b->classname, "func_wall_toggle") == 0 && !b->runtime_is_visible) continue;
        glUniform1i(glGetUniformLocation(renderer->mainShader, "isBrush"), 1);
        if (strcmp(b->classname, "func_water") == 0) continue;
        if (strcmp(b->classname, "env_glass") == 0) continue;
        if (b->numVertices > 0) {
            Vec3 min_v = { FLT_MAX, FLT_MAX, FLT_MAX };
            Vec3 max_v = { -FLT_MAX, -FLT_MAX, -FLT_MAX };
            for (int j = 0; j < b->numVertices; ++j) {
                Vec3 p = mat4_mul_vec3(&b->modelMatrix, b->vertices[j].pos);
                min_v.x = fminf(min_v.x, p.x); min_v.y = fminf(min_v.y, p.y); min_v.z = fminf(min_v.z, p.z);
                max_v.x = fmaxf(max_v.x, p.x); max_v.y = fmaxf(max_v.y, p.y); max_v.z = fmaxf(max_v.z, p.z);
            }
            if (!frustum_check_aabb(&frustum, min_v, max_v)) {
                continue;
            }
        }
        render_brush(renderer, scene, renderer->mainShader, &scene->brushes[i], false, &frustum);
    }
    MiscRender_ParallaxRooms(renderer, scene, engine, view, projection);
    Decals_Render(scene, renderer, renderer->mainShader);

    if (Cvar_GetInt("r_physics_shadows")) {
        glUseProgram(renderer->modelShadowShader);
        glUniformMatrix4fv(glGetUniformLocation(renderer->modelShadowShader, "view"), 1, GL_FALSE, view->m);
        glUniformMatrix4fv(glGetUniformLocation(renderer->modelShadowShader, "projection"), 1, GL_FALSE, projection->m);
        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
        glDepthMask(GL_FALSE);
        for (int i = 0; i < scene->numObjects; i++) {
            SceneObject* obj = &scene->objects[i];
            if (obj->mass > 0.0f && obj->model) {
                glUniformMatrix4fv(glGetUniformLocation(renderer->modelShadowShader, "model"), 1, GL_FALSE, obj->modelMatrix.m);
                for (int meshIdx = 0; meshIdx < obj->model->meshCount; ++meshIdx) {
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
        glDepthMask(GL_TRUE);
        glDisable(GL_BLEND);
    }

    if (Cvar_GetInt("r_faceculling")) {
        glDisable(GL_CULL_FACE);
    }
    if (Cvar_GetInt("r_zprepass")) {
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
        glUniformMatrix4fv(glGetUniformLocation(renderer->wireframeShader, "view"), 1, GL_FALSE, view->m);
        glUniformMatrix4fv(glGetUniformLocation(renderer->wireframeShader, "projection"), 1, GL_FALSE, projection->m);
        glUniform4f(glGetUniformLocation(renderer->wireframeShader, "wireframeColor"), 0.0f, 0.5f, 1.0f, 1.0f);
        glDisable(GL_DEPTH_TEST);
        for (int i = 0; i < scene->numObjects; i++) {
            SceneObject* obj = &scene->objects[i];
            glUniformMatrix4fv(glGetUniformLocation(renderer->wireframeShader, "model"), 1, GL_FALSE, obj->modelMatrix.m);
            if (obj->model) {
                for (int meshIdx = 0; meshIdx < obj->model->meshCount; ++meshIdx) {
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
        for (int i = 0; i < scene->numBrushes; i++) {
            Brush* b = &scene->brushes[i];
            if (!Brush_IsSolid(b)) continue;
            glUniformMatrix4fv(glGetUniformLocation(renderer->wireframeShader, "model"), 1, GL_FALSE, b->modelMatrix.m);
            glBindVertexArray(b->vao);
            glDrawArrays(GL_TRIANGLES, 0, b->totalRenderVertexCount);
        }
        glEnable(GL_DEPTH_TEST);
    }

    glBindFramebuffer(GL_FRAMEBUFFER, 0);
}