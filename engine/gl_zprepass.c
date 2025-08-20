#include "gl_zprepass.h"
#include "gl_misc.h"

void Zprepass_Init(Renderer* renderer) {
    renderer->zPrepassShader = createShaderProgram("shaders/zprepass.vert", "shaders/zprepass.frag");
    renderer->zPrepassTessShader = createShaderProgramTess("shaders/zprepass_tess.vert", "shaders/zprepass_tess.tcs", "shaders/zprepass_tess.tes", "shaders/zprepass_tess.frag");
}

void Zprepass_Shutdown(Renderer* renderer) {
    glDeleteProgram(renderer->zPrepassShader);
    glDeleteProgram(renderer->zPrepassTessShader);
}

void Zprepass_Render(Renderer* renderer, Scene* scene, Engine* engine, const Mat4* view, const Mat4* projection) {
    glBindFramebuffer(GL_FRAMEBUFFER, renderer->gBufferFBO);
    glViewport(0, 0, engine->width / GEOMETRY_PASS_DOWNSAMPLE_FACTOR, engine->height / GEOMETRY_PASS_DOWNSAMPLE_FACTOR);
    glClear(GL_DEPTH_BUFFER_BIT);

    glEnable(GL_DEPTH_TEST);
    glDepthMask(GL_TRUE);
    glDepthFunc(GL_LESS);
    glColorMask(GL_FALSE, GL_FALSE, GL_FALSE, GL_FALSE);

    if (Cvar_GetInt("r_faceculling")) {
        glEnable(GL_CULL_FACE);
        glCullFace(GL_BACK);
    }
    else {
        glDisable(GL_CULL_FACE);
    }

    glEnable(GL_POLYGON_OFFSET_FILL);
    glPolygonOffset(1.0f, 1.0f);

    for (int i = 0; i < scene->numObjects; i++) {
        SceneObject* obj = &scene->objects[i];
        if (!obj->model) continue;

        bool hasTessellatedMesh = false;
        for (int meshIdx = 0; meshIdx < obj->model->meshCount; ++meshIdx) {
            if (obj->model->meshes[meshIdx].material && obj->model->meshes[meshIdx].material->useTesselation) {
                hasTessellatedMesh = true;
                break;
            }
        }

        GLuint shader = hasTessellatedMesh ? renderer->zPrepassTessShader : renderer->zPrepassShader;
        glUseProgram(shader);
        glUniformMatrix4fv(glGetUniformLocation(shader, "view"), 1, GL_FALSE, view->m);
        glUniformMatrix4fv(glGetUniformLocation(shader, "projection"), 1, GL_FALSE, projection->m);
        glUniformMatrix4fv(glGetUniformLocation(shader, "model"), 1, GL_FALSE, obj->modelMatrix.m);

        if (hasTessellatedMesh) {
            glPatchParameteri(GL_PATCH_VERTICES, 3);
            for (int meshIdx = 0; meshIdx < obj->model->meshCount; ++meshIdx) {
                Mesh* mesh = &obj->model->meshes[meshIdx];
                Material* mat = mesh->material;

                if (mat && mat->useTesselation) {
                    glUniform1i(glGetUniformLocation(shader, "useBlendMap"), 0);
                    glUniform1f(glGetUniformLocation(shader, "heightScale"), mat->heightScale);
                    glActiveTexture(GL_TEXTURE0); glBindTexture(GL_TEXTURE_2D, mat->heightMap);
                    glUniform1i(glGetUniformLocation(shader, "heightMap"), 0);
                }

                glBindVertexArray(mesh->VAO);
                if (mat && mat->useTesselation) {
                    glDrawElements(GL_PATCHES, mesh->indexCount, GL_UNSIGNED_INT, 0);
                }
                else {
                    glDrawElements(GL_TRIANGLES, mesh->indexCount, GL_UNSIGNED_INT, 0);
                }
            }
        }
        else {
            bool is_skinnable = obj->model && obj->model->num_skins > 0;
            glUniform1i(glGetUniformLocation(shader, "u_hasAnimation"), is_skinnable);
            if (is_skinnable && obj->bone_matrices) {
                glUniformMatrix4fv(glGetUniformLocation(shader, "u_boneMatrices"), obj->model->skins[0].num_joints, GL_FALSE, (const GLfloat*)obj->bone_matrices);
            }
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
        if (strcmp(b->classname, "func_wall_toggle") == 0 && !b->runtime_is_visible) continue;
        if (strcmp(b->classname, "func_clip") == 0) continue;
        if (strcmp(b->classname, "env_glass") == 0) continue;
        if (!Brush_IsSolid(b) && strcmp(b->classname, "func_illusionary") != 0 && strcmp(b->classname, "func_lod") != 0) continue;

        bool hasTessellatedFace = false;
        for (int faceIdx = 0; faceIdx < b->numFaces; ++faceIdx) {
            Material* mat = b->faces[faceIdx].material;
            if (mat && mat->useTesselation) {
                hasTessellatedFace = true;
                break;
            }
        }

        if (hasTessellatedFace) {
            glUseProgram(renderer->zPrepassTessShader);
            glPatchParameteri(GL_PATCH_VERTICES, 3);
            glUniformMatrix4fv(glGetUniformLocation(renderer->zPrepassTessShader, "view"), 1, GL_FALSE, view->m);
            glUniformMatrix4fv(glGetUniformLocation(renderer->zPrepassTessShader, "projection"), 1, GL_FALSE, projection->m);
            glUniformMatrix4fv(glGetUniformLocation(renderer->zPrepassTessShader, "model"), 1, GL_FALSE, b->modelMatrix.m);

            glBindVertexArray(b->vao);
            int vbo_offset = 0;
            for (int face_idx = 0; face_idx < b->numFaces; ++face_idx) {
                BrushFace* face = &b->faces[face_idx];
                int num_face_verts = (face->numVertexIndices - 2) * 3;
                if (face->material && face->material->useTesselation) {
                    glUniform1f(glGetUniformLocation(renderer->zPrepassTessShader, "heightScale"), face->material->heightScale);
                    glActiveTexture(GL_TEXTURE0); glBindTexture(GL_TEXTURE_2D, face->material->heightMap); glUniform1i(glGetUniformLocation(renderer->zPrepassTessShader, "heightMap"), 0);

                    bool useBlend = face->material2 || face->material3 || face->material4;
                    glUniform1i(glGetUniformLocation(renderer->zPrepassTessShader, "useBlendMap"), useBlend);
                    if (useBlend) {
                        glActiveTexture(GL_TEXTURE1); glBindTexture(GL_TEXTURE_2D, face->material2 ? face->material2->heightMap : 0); glUniform1i(glGetUniformLocation(renderer->zPrepassTessShader, "heightMap2"), 1);
                        glActiveTexture(GL_TEXTURE2); glBindTexture(GL_TEXTURE_2D, face->material3 ? face->material3->heightMap : 0); glUniform1i(glGetUniformLocation(renderer->zPrepassTessShader, "heightMap3"), 2);
                        glActiveTexture(GL_TEXTURE3); glBindTexture(GL_TEXTURE_2D, face->material4 ? face->material4->heightMap : 0); glUniform1i(glGetUniformLocation(renderer->zPrepassTessShader, "heightMap4"), 3);
                        glActiveTexture(GL_TEXTURE4); glBindTexture(GL_TEXTURE_2D, face->blendMapTexture); glUniform1i(glGetUniformLocation(renderer->zPrepassTessShader, "blendMap"), 4);
                    }
                    glDrawArrays(GL_PATCHES, vbo_offset, num_face_verts);
                }
                vbo_offset += num_face_verts;
            }
        }
        else {
            glUseProgram(renderer->zPrepassShader);
            glUniformMatrix4fv(glGetUniformLocation(renderer->zPrepassShader, "view"), 1, GL_FALSE, view->m);
            glUniformMatrix4fv(glGetUniformLocation(renderer->zPrepassShader, "projection"), 1, GL_FALSE, projection->m);
            glUniformMatrix4fv(glGetUniformLocation(renderer->zPrepassShader, "model"), 1, GL_FALSE, b->modelMatrix.m);
            glBindVertexArray(b->vao);
            glDrawArrays(GL_TRIANGLES, 0, b->totalRenderVertexCount);
        }
    }

    glDisable(GL_POLYGON_OFFSET_FILL);
    glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
    glDepthMask(GL_FALSE);
    glDepthFunc(GL_LEQUAL);
}