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
#include <float.h>
#include "gl_decals.h"
#include "gl_geometry.h"
#include "cvar.h"
#include "texturemanager.h"
#include "map_misc.h"

static Float decalQuadVertices[] = {
    -0.5f, -0.5f, -0.5f,  -1.0f,  0.0f,  0.0f,   0.0f, 1.0f,      0.0f, -1.0f,  0.0f, 1.0f,    0.0f, 0.0f, 0.0f, 1.0f,   0.0f, 0.0f,    0.0f, 0.0f,    0.0f, 0.0f,
     0.5f, -0.5f, -0.5f,  -1.0f,  0.0f,  0.0f,   1.0f, 1.0f,      0.0f, -1.0f,  0.0f, 1.0f,    0.0f, 0.0f, 0.0f, 1.0f,   0.0f, 0.0f,    0.0f, 0.0f,    0.0f, 0.0f,
     0.5f,  0.5f, -0.5f,  -1.0f,  0.0f,  0.0f,   1.0f, 0.0f,      0.0f, -1.0f,  0.0f, 1.0f,    0.0f, 0.0f, 0.0f, 1.0f,   0.0f, 0.0f,    0.0f, 0.0f,    0.0f, 0.0f,
     0.5f,  0.5f, -0.5f,  -1.0f,  0.0f,  0.0f,   1.0f, 0.0f,      0.0f, -1.0f,  0.0f, 1.0f,    0.0f, 0.0f, 0.0f, 1.0f,   0.0f, 0.0f,    0.0f, 0.0f,    0.0f, 0.0f,
    -0.5f,  0.5f, -0.5f,  -1.0f,  0.0f,  0.0f,   0.0f, 0.0f,      0.0f, -1.0f,  0.0f, 1.0f,    0.0f, 0.0f, 0.0f, 1.0f,   0.0f, 0.0f,    0.0f, 0.0f,    0.0f, 0.0f,
    -0.5f, -0.5f, -0.5f,  -1.0f,  0.0f,  0.0f,   0.0f, 1.0f,      0.0f, -1.0f,  0.0f, 1.0f,    0.0f, 0.0f, 0.0f, 1.0f,   0.0f, 0.0f,    0.0f, 0.0f,    0.0f, 0.0f
};

void Decals_Init(Renderer* renderer) {
    glGenVertexArrays(1, &renderer->decalVAO);
    glGenBuffers(1, &renderer->decalVBO);
    glBindVertexArray(renderer->decalVAO);
    glBindBuffer(GL_ARRAY_BUFFER, renderer->decalVBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(decalQuadVertices), &decalQuadVertices, GL_STATIC_DRAW);
    Usize stride = 22 * sizeof(Float);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, stride, (void*)0);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, stride, (void*)(3 * sizeof(Float)));
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, stride, (void*)(6 * sizeof(Float)));
    glEnableVertexAttribArray(2);
    glVertexAttribPointer(3, 4, GL_FLOAT, GL_FALSE, stride, (void*)(8 * sizeof(Float)));
    glEnableVertexAttribArray(3);
    glVertexAttribPointer(4, 4, GL_FLOAT, GL_FALSE, stride, (void*)(12 * sizeof(Float)));
    glEnableVertexAttribArray(4);
    glVertexAttribPointer(5, 2, GL_FLOAT, GL_FALSE, stride, (void*)(16 * sizeof(Float)));
    glEnableVertexAttribArray(5);
    glVertexAttribPointer(6, 2, GL_FLOAT, GL_FALSE, stride, (void*)(18 * sizeof(Float)));
    glEnableVertexAttribArray(6);
    glVertexAttribPointer(7, 2, GL_FLOAT, GL_FALSE, stride, (void*)(20 * sizeof(Float)));
    glEnableVertexAttribArray(7);
    glVertexAttribPointer(8, 2, GL_FLOAT, GL_FALSE, stride, (void*)(6 * sizeof(Float)));
    glEnableVertexAttribArray(8);
    glBindVertexArray(0);
}

void Decals_Shutdown(Renderer* renderer) {
    if (renderer->decalVAO) glDeleteVertexArrays(1, &renderer->decalVAO);
    if (renderer->decalVBO) glDeleteBuffers(1, &renderer->decalVBO);
}

void Decals_Render(Scene* scene, Renderer* renderer, GLuint shader_program) {
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glDepthMask(GL_FALSE);

    glUseProgram(shader_program);
    
    glUniform1i(glGetUniformLocation(shader_program, "isBrush"), 1);
    glUniform1i(glGetUniformLocation(shader_program, "isDecal"), 1);
    GLuint useEnvLoc = glGetUniformLocation(shader_program, "useEnvironmentMap");
    GLuint probeMinLoc = glGetUniformLocation(shader_program, "probeBoxMin");
    GLuint probeMaxLoc = glGetUniformLocation(shader_program, "probeBoxMax");
    GLuint probePosLoc = glGetUniformLocation(shader_program, "probePosition");
    glPatchParameteri(GL_PATCH_VERTICES, 3);

    for (Int i = 0; i < scene->numDecals; ++i) {
        Decal* d = &scene->decals[i];

        glUniformMatrix4fv(glGetUniformLocation(shader_program, "model"), 1, GL_FALSE, d->modelMatrix.m);
        Int probeIdx = FindReflectionProbeForPoint(scene, d->pos);
        if (probeIdx != -1 && Cvar_GetInt("r_cubemaps")) {
            Brush* probe = &scene->brushes[probeIdx];
            glUniform1i(useEnvLoc, 1);
            glActiveTexture(GL_TEXTURE10);
            glBindTexture(GL_TEXTURE_CUBE_MAP, probe->cubemapTexture);

            Vec3 min_aabb, max_aabb;
            Brush_GetWorldAABB(probe, &min_aabb, &max_aabb);

            glUniform3fv(probeMinLoc, 1, &min_aabb.x);
            glUniform3fv(probeMaxLoc, 1, &max_aabb.x);
            glUniform3fv(probePosLoc, 1, &probe->pos.x);
        }
        else {
            glUniform1i(useEnvLoc, 0);
        }
        glUniform1f(glGetUniformLocation(shader_program, "heightScale"), 0.0f);

        glUniform2f(glGetUniformLocation(shader_program, "u_uvScale"), d->uv_scale.x, d->uv_scale.y);
        glUniform2f(glGetUniformLocation(shader_program, "u_uvOffset"), d->uv_offset.x, d->uv_offset.y);
        glUniform1f(glGetUniformLocation(shader_program, "u_uvRotation"), d->uv_rotation * ((Float)M_PI / 180.0f));

        glActiveTexture(GL_TEXTURE0); glBindTexture(GL_TEXTURE_2D, d->material->diffuseMap);
        glActiveTexture(GL_TEXTURE1); glBindTexture(GL_TEXTURE_2D, d->material->normalMap);
        glActiveTexture(GL_TEXTURE2); glBindTexture(GL_TEXTURE_2D, d->material->rmaMap);
        glActiveTexture(GL_TEXTURE3); glBindTexture(GL_TEXTURE_2D, 0);
        glActiveTexture(GL_TEXTURE7); glBindTexture(GL_TEXTURE_2D, 0);

        Bool has_lightmap = d->lightmapAtlas != 0 && d->lightmapAtlas != missingTextureID;
        glUniform1i(glGetUniformLocation(shader_program, "useLightmap"), has_lightmap);
        if (has_lightmap) {
            glActiveTexture(GL_TEXTURE5);
            glBindTexture(GL_TEXTURE_2D, d->lightmapAtlas);
            glUniform1i(glGetUniformLocation(shader_program, "lightmap"), 5);
        }

        Bool has_dir_lightmap = d->directionalLightmapAtlas != 0 && d->directionalLightmapAtlas != missingTextureID;
        glUniform1i(glGetUniformLocation(shader_program, "useDirectionalLightmap"), has_dir_lightmap);
        if (has_dir_lightmap) {
            glActiveTexture(GL_TEXTURE6);
            glBindTexture(GL_TEXTURE_2D, d->directionalLightmapAtlas);
            glUniform1i(glGetUniformLocation(shader_program, "directionalLightmap"), 6);
        }

        glBindVertexArray(renderer->decalVAO);
        glDrawArrays(GL_PATCHES, 0, 6);
    }

    glUniform1i(glGetUniformLocation(shader_program, "isBrush"), 0);
    glUniform1i(glGetUniformLocation(shader_program, "isDecal"), 0);
    glUniform1i(glGetUniformLocation(shader_program, "useLightmap"), 0);
    glUniform1i(glGetUniformLocation(shader_program, "useDirectionalLightmap"), 0);

    glDepthMask(GL_TRUE);
    glDisable(GL_BLEND);
    glBindVertexArray(0);
}