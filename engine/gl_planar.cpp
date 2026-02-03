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
#include "gl_planar.h"
#include "gl_misc.h"
#include "cvar.h"
#include <float.h>
#include "gl_geometry.h"
#include "gl_skybox.h"
#include "water_manager.h"
#include "io_system.h"

#ifdef BRANCH_NOCTURNE
// hack alert
static bool NoSkyReflectionMap(const char* map) {
    return _stricmp(map, "level1.map") == 0 ||
        _stricmp(map, "level2.map") == 0 ||
        _stricmp(map, "level3.map") == 0 ||
        _stricmp(map, "level4.map") == 0;
}
#endif

void Planar_RenderReflections(Renderer* renderer, Scene* scene, Engine* engine, Mat4* view, Mat4* projection, const Mat4* sunLightSpaceMatrix, Camera* camera) {
    static int frameCounter = 0;
    frameCounter++;

    int updateEveryNFrames = 2;
    if ((frameCounter % updateEveryNFrames) != 0) {
        return;
    }

    if (!Cvar_GetInt("r_planar")) return;

    float reflection_plane_height = 0.0;
    bool reflection_plane_found = false;
    for (int i = 0; i < scene->numBrushes; ++i) {
        Brush* b = &scene->brushes[i];
        if (strcmp(b->classname, "func_water") == 0 || strcmp(b->classname, "func_reflective_glass") == 0) {
            float max_y = -FLT_MAX;
            for (int v = 0; v < b->numVertices; ++v) {
                Vec3 world_v = mat4_mul_vec3(&b->modelMatrix, b->vertices[v].pos);
                if (world_v.y > max_y) {
                    max_y = world_v.y;
                }
            }
            reflection_plane_height = max_y;
            reflection_plane_found = true;
            break;
        }
    }

    if (!reflection_plane_found) {
        return;
    }

    int downsample = Cvar_GetInt("r_planar_downsample");
    if (downsample < 1) downsample = 1;
    int reflection_width = engine->width / downsample;
    int reflection_height = engine->height / downsample;

    glEnable(GL_CLIP_DISTANCE0);

    Camera reflection_camera = *camera;

    float distance = 2 * (reflection_camera.position.y - reflection_plane_height);
    reflection_camera.position.y -= distance;
    reflection_camera.pitch = -reflection_camera.pitch;

    Vec3 f_refl = { cosf(reflection_camera.pitch) * sinf(reflection_camera.yaw), sinf(reflection_camera.pitch), -cosf(reflection_camera.pitch) * cosf(reflection_camera.yaw) };
    vec3_normalize(&f_refl);
    Vec3 t_refl = vec3_add(reflection_camera.position, f_refl);
    Mat4 reflection_view = mat4_lookAt(reflection_camera.position, t_refl, Vec3{ 0, 1, 0 });

    glUseProgram(renderer->mainShader);
    glUniform4f(glGetUniformLocation(renderer->mainShader, "clipPlane"), 0, 1, 0, -reflection_plane_height + 0.1f);

    glViewport(0, 0, reflection_width, reflection_height);
    Geometry_RenderPass(renderer, scene, engine, &reflection_view, projection, sunLightSpaceMatrix, reflection_camera.position, false, true);

    glBindFramebuffer(GL_READ_FRAMEBUFFER, renderer->gBufferFBO);
    glBindFramebuffer(GL_DRAW_FRAMEBUFFER, renderer->reflectionFBO);
    glBlitFramebuffer(0, 0, engine->width / Cvar_GetFloat("r_geometry_downsample"), engine->height / Cvar_GetFloat("r_geometry_downsample"), 0, 0, reflection_width, reflection_height, GL_COLOR_BUFFER_BIT, GL_LINEAR);
    glBlitFramebuffer(0, 0, engine->width / Cvar_GetFloat("r_geometry_downsample"), engine->height / Cvar_GetFloat("r_geometry_downsample"), 0, 0, reflection_width, reflection_height, GL_DEPTH_BUFFER_BIT, GL_NEAREST);

    glBindFramebuffer(GL_FRAMEBUFFER, renderer->reflectionFBO);
    glViewport(0, 0, reflection_width, reflection_height);

#ifdef BRANCH_NOCTURNE
    // hacky fix to prevent glitches in level1,level2,level3,level4.map
    if (!NoSkyReflectionMap(scene->mapPath)) {
        Skybox_Render(renderer, scene, engine, &reflection_view, projection);
    }
#else
    Skybox_Render(renderer, scene, engine, &reflection_view, projection);
#endif

    glDisable(GL_CLIP_DISTANCE0);
    glUseProgram(renderer->mainShader);
    glUniform4f(glGetUniformLocation(renderer->mainShader, "clipPlane"), 0, 0, 0, 0);
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    glViewport(0, 0, engine->width, engine->height);
}

void Planar_RenderWater(Renderer* renderer, Scene* scene, Engine* engine, Mat4* view, Mat4* projection, const Mat4* sunLightSpaceMatrix) {
    glBindFramebuffer(GL_FRAMEBUFFER, renderer->gBufferFBO);

    GLuint attachments[7] = { GL_COLOR_ATTACHMENT0, GL_COLOR_ATTACHMENT1, GL_COLOR_ATTACHMENT2, GL_COLOR_ATTACHMENT3, GL_COLOR_ATTACHMENT4, GL_COLOR_ATTACHMENT5, GL_COLOR_ATTACHMENT6 };
    glDrawBuffers(7, attachments);

    glEnable(GL_DEPTH_TEST);
    glDepthMask(GL_TRUE);
    glDisable(GL_BLEND);

    glUseProgram(renderer->waterShader);

    glUniformMatrix4fv(glGetUniformLocation(renderer->waterShader, "view"), 1, GL_FALSE, view->m);
    glUniformMatrix4fv(glGetUniformLocation(renderer->waterShader, "projection"), 1, GL_FALSE, projection->m);
    glUniform3fv(glGetUniformLocation(renderer->waterShader, "viewPos"), 1, &engine->camera.position.x);
    glUniform1i(glGetUniformLocation(renderer->waterShader, "u_debug_reflection"), Cvar_GetInt("r_debug_water_reflection"));

    glUniform1i(glGetUniformLocation(renderer->waterShader, "sun.enabled"), scene->sun.enabled);
    glUniform3fv(glGetUniformLocation(renderer->waterShader, "sun.direction"), 1, &scene->sun.direction.x);
    glUniform3fv(glGetUniformLocation(renderer->waterShader, "sun.color"), 1, &scene->sun.color.x);
    glUniform1f(glGetUniformLocation(renderer->waterShader, "sun.intensity"), scene->sun.intensity);

    glUniformMatrix4fv(glGetUniformLocation(renderer->waterShader, "sunLightSpaceMatrix"), 1, GL_FALSE, sunLightSpaceMatrix->m);
    glUniform1i(glGetUniformLocation(renderer->waterShader, "numActiveLights"), scene->numActiveLights);
    glUniform1i(glGetUniformLocation(renderer->waterShader, "r_lightmaps_bicubic"), Cvar_GetInt("r_lightmaps_bicubic"));
    glUniform1i(glGetUniformLocation(renderer->waterShader, "r_debug_lightmaps"), Cvar_GetInt("r_debug_lightmaps"));
    glUniform1i(glGetUniformLocation(renderer->waterShader, "r_debug_lightmaps_directional"), Cvar_GetInt("r_debug_lightmaps_directional"));

    glUniform1i(glGetUniformLocation(renderer->waterShader, "flashlight.enabled"), engine->flashlight_on);
    if (engine->flashlight_on) {
        Vec3 forward = { cosf(engine->camera.pitch) * sinf(engine->camera.yaw), sinf(engine->camera.pitch), -cosf(engine->camera.pitch) * cosf(engine->camera.yaw) };
        vec3_normalize(&forward);
        glUniform3fv(glGetUniformLocation(renderer->waterShader, "flashlight.position"), 1, &engine->camera.position.x);
        glUniform3fv(glGetUniformLocation(renderer->waterShader, "flashlight.direction"), 1, &forward.x);
    }

    glUniform3fv(glGetUniformLocation(renderer->waterShader, "cameraPosition"), 1, &engine->camera.position.x);
    glUniform1f(glGetUniformLocation(renderer->waterShader, "time"), engine->scaledTime);

    glActiveTexture(GL_TEXTURE11);
    glBindTexture(GL_TEXTURE_2D, renderer->sunShadowMap);
    glUniform1i(glGetUniformLocation(renderer->waterShader, "sunShadowMap"), 11);

    glActiveTexture(GL_TEXTURE2);
    glBindTexture(GL_TEXTURE_2D, renderer->reflectionTexture);
    glUniform1i(glGetUniformLocation(renderer->waterShader, "reflectionTexture"), 2);

    for (int i = 0; i < scene->numBrushes; ++i) {
        Brush* b = &scene->brushes[i];
        if (strcmp(b->classname, "func_water") != 0) continue;
        const char* water_def_name = Brush_GetProperty(b, "water_def", "default_water");
        WaterDef* water_def = WaterManager_FindWaterDef(water_def_name);
        if (!water_def) continue;

        float uv_scale = atof(Brush_GetProperty(b, "uv_scale", "0.0"));
        glUniform1f(glGetUniformLocation(renderer->waterShader, "u_uv_scale"), uv_scale);

        Vec3 world_min = { FLT_MAX, FLT_MAX, FLT_MAX };
        Vec3 world_max = { -FLT_MAX, -FLT_MAX, -FLT_MAX };
        if (b->numVertices > 0) {
            for (int v_idx = 0; v_idx < b->numVertices; ++v_idx) {
                Vec3 world_v = mat4_mul_vec3(&b->modelMatrix, b->vertices[v_idx].pos);
                world_min.x = fminf(world_min.x, world_v.x);
                world_min.y = fminf(world_min.y, world_v.y);
                world_min.z = fminf(world_min.z, world_v.z);
                world_max.x = fmaxf(world_max.x, world_v.x);
                world_max.y = fmaxf(world_max.y, world_v.y);
                world_max.z = fmaxf(world_max.z, world_v.z);
            }
            glUniform3fv(glGetUniformLocation(renderer->waterShader, "u_waterAabbMin"), 1, &world_min.x);
            glUniform3fv(glGetUniformLocation(renderer->waterShader, "u_waterAabbMax"), 1, &world_max.x);
        }

        glActiveTexture(GL_TEXTURE0); glBindTexture(GL_TEXTURE_2D, water_def->dudvMap);
        glActiveTexture(GL_TEXTURE1); glBindTexture(GL_TEXTURE_2D, water_def->normalMap);

        if (b->lightmapAtlas != 0) {
            glUniform1i(glGetUniformLocation(renderer->waterShader, "useLightmap"), 1);
            glActiveTexture(GL_TEXTURE12);
            glBindTexture(GL_TEXTURE_2D, b->lightmapAtlas);
            glUniform1i(glGetUniformLocation(renderer->waterShader, "lightmap"), 12);
        }
        else {
            glUniform1i(glGetUniformLocation(renderer->waterShader, "useLightmap"), 0);
        }

        if (b->directionalLightmapAtlas != 0) {
            glUniform1i(glGetUniformLocation(renderer->waterShader, "useDirectionalLightmap"), 1);
            glActiveTexture(GL_TEXTURE13);
            glBindTexture(GL_TEXTURE_2D, b->directionalLightmapAtlas);
            glUniform1i(glGetUniformLocation(renderer->waterShader, "directionalLightmap"), 13);
        }
        else {
            glUniform1i(glGetUniformLocation(renderer->waterShader, "useDirectionalLightmap"), 0);
        }

        if (water_def->flowMap != 0) {
            glActiveTexture(GL_TEXTURE3);
            glBindTexture(GL_TEXTURE_2D, water_def->flowMap);
            glUniform1i(glGetUniformLocation(renderer->waterShader, "flowMap"), 3);
            glUniform1f(glGetUniformLocation(renderer->waterShader, "flowSpeed"), water_def->flowSpeed);
            glUniform1i(glGetUniformLocation(renderer->waterShader, "useFlowMap"), 1);
        }
        else {
            glUniform1i(glGetUniformLocation(renderer->waterShader, "useFlowMap"), 0);
        }

        glUniformMatrix4fv(glGetUniformLocation(renderer->waterShader, "model"), 1, GL_FALSE, b->modelMatrix.m);
        glBindVertexArray(b->vao);
        glDrawArrays(GL_TRIANGLES, 0, b->totalRenderVertexCount);
    }
    glBindVertexArray(0);
    glDepthMask(GL_TRUE);
    glDrawBuffer(GL_COLOR_ATTACHMENT0);
}

void Planar_RenderReflectiveGlass(Renderer* renderer, Scene* scene, Engine* engine, Mat4* view, Mat4* projection) {
    if (!Cvar_GetInt("r_planar")) return;

    glUseProgram(renderer->reflectiveGlassShader);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glDepthMask(GL_FALSE);

    glUniformMatrix4fv(glGetUniformLocation(renderer->reflectiveGlassShader, "view"), 1, GL_FALSE, view->m);
    glUniformMatrix4fv(glGetUniformLocation(renderer->reflectiveGlassShader, "projection"), 1, GL_FALSE, projection->m);
    glUniform3fv(glGetUniformLocation(renderer->reflectiveGlassShader, "viewPos"), 1, &engine->camera.position.x);

    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, renderer->reflectionTexture);
    glUniform1i(glGetUniformLocation(renderer->reflectiveGlassShader, "reflectionTexture"), 0);

    glActiveTexture(GL_TEXTURE2);
    glUniform1i(glGetUniformLocation(renderer->reflectiveGlassShader, "normalMap"), 2);

    for (int i = 0; i < scene->numBrushes; i++) {
        Brush* b = &scene->brushes[i];
        if (strcmp(b->classname, "func_reflective_glass") != 0) continue;

        const char* normal_map_name = Brush_GetProperty(b, "normal_map", "NULL");
        Material* normal_mat = TextureManager_FindMaterial(normal_map_name);
        glBindTexture(GL_TEXTURE_2D, (normal_mat && normal_mat != &g_MissingMaterial) ? normal_mat->normalMap : defaultNormalMapID);

        glUniform1f(glGetUniformLocation(renderer->reflectiveGlassShader, "refractionStrength"), atof(Brush_GetProperty(b, "refraction_strength", "0.01")));

        glUniformMatrix4fv(glGetUniformLocation(renderer->reflectiveGlassShader, "model"), 1, GL_FALSE, b->modelMatrix.m);
        glBindVertexArray(b->vao);
        glDrawArrays(GL_TRIANGLES, 0, b->totalRenderVertexCount);
    }

    glDepthMask(GL_TRUE);
    glDisable(GL_BLEND);
    glBindVertexArray(0);
}