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
#include "gl_console.h"
#include "map_lighting.h"
#include <SDL_image.h>
#include "stb_image.h"

static void sanitize_filename_map(const Char* input, Char* output, Usize max_len) {
    Usize i = 0;
    while (i < max_len - 1 && input[i] != '\0') {
        if (isalnum((Uchar)input[i]) || input[i] == '_' || input[i] == '-') {
            output[i] = input[i];
        }
        else {
            output[i] = '_';
        }
        i++;
    }
    output[i] = '\0';
}

void SceneObject_LoadVertexLighting(SceneObject* obj, Int index, const Char* mapPath) {
    if (!obj->model || obj->model->totalVertexCount == 0) return;

    Char map_name_sanitized[128];
    const Char* last_slash = strrchr(mapPath, '/');
    const Char* last_bslash = strrchr(mapPath, '\\');
    const Char* map_filename_start = (last_slash > last_bslash) ? last_slash + 1 : (last_bslash ? last_bslash + 1 : mapPath);
    const Char* dot = strrchr(map_filename_start, '.');
    if (dot) {
        Usize len = dot - map_filename_start;
        strncpy(map_name_sanitized, map_filename_start, len);
        map_name_sanitized[len] = '\0';
    }
    else {
        strcpy(map_name_sanitized, map_filename_start);
    }

    Char model_name_sanitized[128];
    if (strlen(obj->targetname) > 0) {
        sanitize_filename_map(obj->targetname, model_name_sanitized, sizeof(model_name_sanitized));
    }
    else {
        sprintf(model_name_sanitized, "Model_%d", index);
    }

    Char vlm_path[512];
    snprintf(vlm_path, sizeof(vlm_path), "lightmaps/%s/%s/vertex_colors.vlm", map_name_sanitized, model_name_sanitized);

    FILE* file = fopen(vlm_path, "rb");
    if (file) {
        Char header[4];
        Uint vertex_count;
        fread(header, 1, 4, file);
        fread(&vertex_count, sizeof(Uint), 1, file);

        if (strncmp(header, "VLM1", 4) == 0 && vertex_count == obj->model->totalVertexCount) {
            obj->bakedVertexColors = new Vec4[vertex_count];
            if (obj->bakedVertexColors) {
                fread(obj->bakedVertexColors, sizeof(Vec4), vertex_count, file);
            }
        }
        else {
            Console::Printf_Warning("VLM file '%s' is invalid or vertex count mismatch.", vlm_path);
        }
        fclose(file);
    }
}

void SceneObject_LoadVertexDirectionalLighting(SceneObject* obj, Int index, const Char* mapPath) {
    if (!obj->model || obj->model->totalVertexCount == 0) return;

    Char map_name_sanitized[128];
    const Char* last_slash = strrchr(mapPath, '/');
    const Char* last_bslash = strrchr(mapPath, '\\');
    const Char* map_filename_start = (last_slash > last_bslash) ? last_slash + 1 : (last_bslash ? last_bslash + 1 : mapPath);
    const Char* dot = strrchr(map_filename_start, '.');
    if (dot) {
        Usize len = dot - map_filename_start;
        strncpy(map_name_sanitized, map_filename_start, len);
        map_name_sanitized[len] = '\0';
    }
    else {
        strcpy(map_name_sanitized, map_filename_start);
    }

    Char model_name_sanitized[128];
    if (strlen(obj->targetname) > 0) {
        sanitize_filename_map(obj->targetname, model_name_sanitized, sizeof(model_name_sanitized));
    }
    else {
        sprintf(model_name_sanitized, "Model_%d", index);
    }

    Char vld_path[512];
    snprintf(vld_path, sizeof(vld_path), "lightmaps/%s/%s/vertex_directions.vld", map_name_sanitized, model_name_sanitized);

    FILE* file = fopen(vld_path, "rb");
    if (file) {
        Char header[4];
        Uint vertex_count;
        fread(header, 1, 4, file);
        fread(&vertex_count, sizeof(Uint), 1, file);

        if (strncmp(header, "VLD1", 4) == 0 && vertex_count == obj->model->totalVertexCount) {
            obj->bakedVertexDirections = new Vec4[vertex_count];
            if (obj->bakedVertexDirections) {
                fread(obj->bakedVertexDirections, sizeof(Vec4), vertex_count, file);
            }
        }
        else {
            Console::Printf_Warning("VLD file '%s' is invalid or vertex count mismatch.", vld_path);
        }
        fclose(file);
    }
}

void SceneObject_LoadLightmaps(SceneObject* obj, Int index, const Char* mapPath) {
    Char map_name_sanitized[128];
    const Char* last_slash = strrchr(mapPath, '/');
    const Char* last_bslash = strrchr(mapPath, '\\');
    const Char* map_filename_start = (last_slash > last_bslash) ? last_slash + 1 : (last_bslash ? last_bslash + 1 : mapPath);
    const Char* dot = strrchr(map_filename_start, '.');
    if (dot) {
        Usize len = dot - map_filename_start;
        strncpy(map_name_sanitized, map_filename_start, len);
        map_name_sanitized[len] = '\0';
    }
    else {
        strcpy(map_name_sanitized, map_filename_start);
    }

    Char model_name_sanitized[128];
    if (strlen(obj->targetname) > 0) {
        sanitize_filename_map(obj->targetname, model_name_sanitized, sizeof(model_name_sanitized));
    }
    else {
        sprintf(model_name_sanitized, "Model_%d", index);
    }

    Char dir_path[1024];
    snprintf(dir_path, sizeof(dir_path), "lightmaps/%s/%s", map_name_sanitized, model_name_sanitized);

    Char color_path[1024];
    snprintf(color_path, sizeof(color_path), "%s/lightmap_color.hdr", dir_path);

    Int w, h, c;
    Float* color_data = stbi_loadf(color_path, &w, &h, &c, 3);
    if (color_data) {
        obj->lightmapWidth = w;
        obj->lightmapHeight = h;
        glGenTextures(1, &obj->lightmapTexture);
        glBindTexture(GL_TEXTURE_2D, obj->lightmapTexture);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB16F, w, h, 0, GL_RGB, GL_FLOAT, color_data);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        stbi_image_free(color_data);

        obj->lightmapHandle = glGetTextureHandleARB(obj->lightmapTexture);
        glMakeTextureHandleResidentARB(obj->lightmapHandle);
    }

    Char dir_lmap_path[1024];
    snprintf(dir_lmap_path, sizeof(dir_lmap_path), "%s/lightmap_dir.png", dir_path);
    SDL_Surface* surf = IMG_Load(dir_lmap_path);
    if (surf) {
        SDL_Surface* conv = SDL_ConvertSurfaceFormat(surf, SDL_PIXELFORMAT_RGBA32, 0);
        glGenTextures(1, &obj->dirLightmapTexture);
        glBindTexture(GL_TEXTURE_2D, obj->dirLightmapTexture);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, conv->w, conv->h, 0, GL_RGBA, GL_UNSIGNED_BYTE, conv->pixels);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        SDL_FreeSurface(conv);
        SDL_FreeSurface(surf);

        obj->dirLightmapHandle = glGetTextureHandleARB(obj->dirLightmapTexture);
        glMakeTextureHandleResidentARB(obj->dirLightmapHandle);
    }

    Char lmuv_path[1024];
    snprintf(lmuv_path, sizeof(lmuv_path), "%s/model.lmuv", dir_path);
    if (obj->model) {
        Model_ApplyLMUV(obj->model, lmuv_path);
    }
}

void Decal_LoadLightmaps(Decal* decal, const Char* originalMapPath, Int decal_index) {
    if (!decal || originalMapPath[0] == '\0') return;

    Char map_name_sanitized[128];
    const Char* last_slash = strrchr(originalMapPath, '/');
    const Char* last_bslash = strrchr(originalMapPath, '\\');
    const Char* map_filename_start = (last_slash > last_bslash) ? last_slash + 1 : (last_bslash ? last_bslash + 1 : originalMapPath);
    const Char* dot = strrchr(map_filename_start, '.');
    if (dot) {
        Usize len = dot - map_filename_start;
        strncpy(map_name_sanitized, map_filename_start, len);
        map_name_sanitized[len] = '\0';
    }
    else {
        strcpy(map_name_sanitized, map_filename_start);
    }

    Char decal_name_sanitized[128];
    if (strlen(decal->targetname) > 0) {
        sanitize_filename_map(decal->targetname, decal_name_sanitized, sizeof(decal_name_sanitized));
    }
    else {
        sprintf(decal_name_sanitized, "decal_%d", decal_index);
    }

    Char final_decal_dir[1024];
    snprintf(final_decal_dir, sizeof(final_decal_dir), "lightmaps/%s/%s", map_name_sanitized, decal_name_sanitized);

    Char color_path[2048];
    snprintf(color_path, sizeof(color_path), "%s/lightmap_color.hdr", final_decal_dir);

    Int width, height, channels;
    Float* color_data = stbi_loadf(color_path, &width, &height, &channels, 3);

    if (color_data) {
        glGenTextures(1, &decal->lightmapAtlas);
        glBindTexture(GL_TEXTURE_2D, decal->lightmapAtlas);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB16F, width, height, 0, GL_RGB, GL_FLOAT, color_data);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        stbi_image_free(color_data);
    }
    else {
        decal->lightmapAtlas = 0;
    }

    Char dir_path[2048];
    snprintf(dir_path, sizeof(dir_path), "%s/lightmap_dir.png", final_decal_dir);

    SDL_Surface* dir_surface = IMG_Load(dir_path);
    if (dir_surface) {
        SDL_Surface* dir_converted = SDL_ConvertSurfaceFormat(dir_surface, SDL_PIXELFORMAT_RGBA32, 0);
        if (dir_converted) {
            glGenTextures(1, &decal->directionalLightmapAtlas);
            glBindTexture(GL_TEXTURE_2D, decal->directionalLightmapAtlas);
            glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, dir_converted->w, dir_converted->h, 0, GL_RGBA, GL_UNSIGNED_BYTE, dir_converted->pixels);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
            SDL_FreeSurface(dir_converted);
        }
        SDL_FreeSurface(dir_surface);
    }
    else {
        decal->directionalLightmapAtlas = 0;
    }
}

void Scene_LoadAmbientProbes(Scene* scene) {
    if (scene->ambient_probes) {
        delete[] scene->ambient_probes;
        scene->ambient_probes = nullptr;
    }
    scene->num_ambient_probes = 0;

    if (strlen(scene->mapPath) == 0) {
        return;
    }

    Char map_name_sanitized[128];
    const Char* last_slash = strrchr(scene->mapPath, '/');
    const Char* last_bslash = strrchr(scene->mapPath, '\\');
    const Char* map_filename_start = (last_slash > last_bslash) ? last_slash + 1 : (last_bslash ? last_bslash + 1 : scene->mapPath);

    const Char* dot = strrchr(map_filename_start, '.');
    if (dot) {
        Usize len = dot - map_filename_start;
        strncpy(map_name_sanitized, map_filename_start, len);
        map_name_sanitized[len] = '\0';
    }
    else {
        strcpy(map_name_sanitized, map_filename_start);
    }

    Char probe_path[512];
    snprintf(probe_path, sizeof(probe_path), "lightmaps/%s/ambient_probes.amp", map_name_sanitized);

    FILE* probe_file = fopen(probe_path, "rb");
    if (probe_file) {
        Char header[4];
        if (fread(header, 1, 4, probe_file) == 4 && strncmp(header, "AMBI", 4) == 0) {
            fread(&scene->num_ambient_probes, sizeof(Int), 1, probe_file);
            if (scene->num_ambient_probes > 0) {
                scene->ambient_probes = new AmbientProbe[scene->num_ambient_probes];
                fread(scene->ambient_probes, sizeof(AmbientProbe), scene->num_ambient_probes, probe_file);
            }
        }
        else {
            Console::Printf_Error("Invalid ambient probe file header: %s", probe_path);
        }
        fclose(probe_file);
    }
}

void Brush_GenerateLightmapAtlas(Brush* b, const Char* originalMapPath, Int brush_index, Int resolution) {
    if (originalMapPath[0] == '\0') return;
    if (b->lightmapAtlasHandle) {
        glMakeTextureHandleNonResidentARB(b->lightmapAtlasHandle);
        b->lightmapAtlasHandle = 0;
    }
    if (b->lightmapAtlas != 0) {
        glDeleteTextures(1, &b->lightmapAtlas);
        b->lightmapAtlas = 0;
    }
    if (b->directionalLightmapAtlasHandle) {
        glMakeTextureHandleNonResidentARB(b->directionalLightmapAtlasHandle);
        b->directionalLightmapAtlasHandle = 0;
    }
    if (b->directionalLightmapAtlas != 0) {
        glDeleteTextures(1, &b->directionalLightmapAtlas);
        b->directionalLightmapAtlas = 0;
    }
    if (b->numFaces == 0) return;

    typedef struct {
        Float* color_data;
        SDL_Surface* dir_surface;
        Int width;
        Int height;
        Bool is_valid;
    } FaceLightmapData;

    FaceLightmapData* face_data = new FaceLightmapData[b->numFaces];
    Int valid_faces = 0;
    Int max_width = 0;
    Int max_height = 0;

    Char map_name_sanitized[128];
    const Char* last_slash = strrchr(originalMapPath, '/');
    const Char* last_bslash = strrchr(originalMapPath, '\\');
    const Char* map_filename_start = (last_slash > last_bslash) ? last_slash + 1 : (last_bslash ? last_bslash + 1 : originalMapPath);
    const Char* dot = strrchr(map_filename_start, '.');
    if (dot) {
        Usize len = dot - map_filename_start;
        strncpy(map_name_sanitized, map_filename_start, len);
        map_name_sanitized[len] = '\0';
    }
    else {
        strcpy(map_name_sanitized, map_filename_start);
    }

    Char brush_name_sanitized[128];
    if (strlen(b->targetname) > 0) {
        sanitize_filename_map(b->targetname, brush_name_sanitized, sizeof(brush_name_sanitized));
    }
    else {
        sprintf(brush_name_sanitized, "Brush_%d", brush_index);
    }

    Char final_brush_dir[1024];
    snprintf(final_brush_dir, sizeof(final_brush_dir), "lightmaps/%s/%s", map_name_sanitized, brush_name_sanitized);

    for (Int i = 0; i < b->numFaces; ++i) {
        face_data[i].is_valid = false;
        Char path[512];

        snprintf(path, sizeof(path), "%s/face_%d_color.hdr", final_brush_dir, i);
        face_data[i].color_data = stbi_loadf(path, &face_data[i].width, &face_data[i].height, nullptr, 3);

        snprintf(path, sizeof(path), "%s/face_%d_dir.png", final_brush_dir, i);
        face_data[i].dir_surface = IMG_Load(path);

        if (face_data[i].color_data && face_data[i].dir_surface) {
            valid_faces++;
            face_data[i].is_valid = true;
            if (face_data[i].width > max_width) max_width = face_data[i].width;
            if (face_data[i].height > max_height) max_height = face_data[i].height;
        }
    }

    if (valid_faces == 0) {
        for (Int i = 0; i < b->numFaces; ++i) {
            if (face_data[i].color_data) stbi_image_free(face_data[i].color_data);
            if (face_data[i].dir_surface) SDL_FreeSurface(face_data[i].dir_surface);
        }
        delete[] face_data;
        b->lightmapAtlas = 0;
        b->directionalLightmapAtlas = 0;
        return;
    }

    if (max_width == 0) max_width = 4;
    if (max_height == 0) max_height = 4;
    Int atlas_cols = (Int)ceil(sqrt((Double)valid_faces));
    Int atlas_rows = (Int)ceil((Double)valid_faces / atlas_cols);
    Int atlas_width = atlas_cols * max_width;
    Int atlas_height = atlas_rows * max_height;

    glGenTextures(1, &b->lightmapAtlas);
    glBindTexture(GL_TEXTURE_2D, b->lightmapAtlas);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB16F, atlas_width, atlas_height, 0, GL_RGB, GL_FLOAT, nullptr);

    glGenTextures(1, &b->directionalLightmapAtlas);
    glBindTexture(GL_TEXTURE_2D, b->directionalLightmapAtlas);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, atlas_width, atlas_height, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);

    Int current_face = 0;
    for (Int i = 0; i < b->numFaces; ++i) {
        if (face_data[i].is_valid) {
            Int x_pos = (current_face % atlas_cols) * max_width;
            Int y_pos = (current_face / atlas_cols) * max_height;
            Int w = face_data[i].width;
            Int h = face_data[i].height;

            glBindTexture(GL_TEXTURE_2D, b->lightmapAtlas);
            glTexSubImage2D(GL_TEXTURE_2D, 0, x_pos, y_pos, w, h, GL_RGB, GL_FLOAT, face_data[i].color_data);

            SDL_Surface* dir_converted = SDL_ConvertSurfaceFormat(face_data[i].dir_surface, SDL_PIXELFORMAT_RGBA32, 0);
            if (dir_converted) {
                glBindTexture(GL_TEXTURE_2D, b->directionalLightmapAtlas);
                glTexSubImage2D(GL_TEXTURE_2D, 0, x_pos, y_pos, w, h, GL_RGBA, GL_UNSIGNED_BYTE, dir_converted->pixels);
                SDL_FreeSurface(dir_converted);
            }

            b->faces[i].atlas_coords.x = (Float)x_pos / atlas_width;
            b->faces[i].atlas_coords.y = (Float)y_pos / atlas_height;
            b->faces[i].atlas_coords.z = (Float)w / atlas_width;
            b->faces[i].atlas_coords.w = (Float)h / atlas_height;

            Float pad_x = (Float)Common::LIGHTMAPPADDING / atlas_width;
            Float pad_y = (Float)Common::LIGHTMAPPADDING / atlas_height;
            b->faces[i].atlas_coords.x += pad_x;
            b->faces[i].atlas_coords.y += pad_y;
            b->faces[i].atlas_coords.z -= pad_x * 2.0f;
            b->faces[i].atlas_coords.w -= pad_y * 2.0f;

            current_face++;
        }
    }

    glBindTexture(GL_TEXTURE_2D, b->lightmapAtlas);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

    glBindTexture(GL_TEXTURE_2D, b->directionalLightmapAtlas);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

    glBindTexture(GL_TEXTURE_2D, 0);

    if (b->lightmapAtlas != 0) {
        b->lightmapAtlasHandle = glGetTextureHandleARB(b->lightmapAtlas);
        glMakeTextureHandleResidentARB(b->lightmapAtlasHandle);
    }
    if (b->directionalLightmapAtlas != 0) {
        b->directionalLightmapAtlasHandle = glGetTextureHandleARB(b->directionalLightmapAtlas);
        glMakeTextureHandleResidentARB(b->directionalLightmapAtlasHandle);
    }

    for (Int i = 0; i < b->numFaces; ++i) {
        if (face_data[i].color_data) stbi_image_free(face_data[i].color_data);
        if (face_data[i].dir_surface) SDL_FreeSurface(face_data[i].dir_surface);
    }
    delete[] face_data;
}