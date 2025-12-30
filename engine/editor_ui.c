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
#include "editor_ui.h"
#include "editor_undo.h"
#include "gl_console.h"
#include "texturemanager.h"
#include "sound_system.h"
#include "cvar.h"
#include "lightmapper.h"
#include "io_system.h" 
#include "gl_render_misc.h"
#include "gl_video_player.h"
#include "game_data.h"
#include <SDL_image.h>
#include <math.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <float.h>

#ifdef PLATFORM_WINDOWS
#include <windows.h>
#else
#include <dirent.h>
#include <sys/stat.h>
#endif

static void render_markdown_line(const char* line) {
    if (strncmp(line, "## ", 3) == 0) {
        UI_TextColored((Vec4) { 0.6f, 0.8f, 1.0f, 1.0f }, "%s", line + 3);
        return;
    }
    if (strncmp(line, "# ", 2) == 0) {
        UI_TextColored((Vec4) { 0.8f, 1.0f, 0.8f, 1.0f }, "%s", line + 2);
        return;
    }
    if (strcmp(line, "---") == 0) {
        UI_Separator();
        return;
    }
    if (strncmp(line, "* ", 2) == 0) {
        UI_BulletText("%s", line + 2);
        return;
    }
    if (strncmp(line, "|", 1) == 0) {
        UI_TextWrapped("%s", line);
        return;
    }

    const char* p = line;
    while (*p) {
        const char* bold_start = strstr(p, "**");

        if (!bold_start) {
            UI_TextWrapped("%s", p);
            break;
        }

        if (bold_start > p) {
            char buffer[1024];
            size_t len = bold_start - p;
            if (len > sizeof(buffer) - 1) len = sizeof(buffer) - 1;
            strncpy(buffer, p, len);
            buffer[len] = '\0';
            UI_TextWrapped("%s", buffer);
            UI_SameLine(0, 0);
        }

        const char* bold_end = strstr(bold_start + 2, "**");

        if (!bold_end) {
            UI_TextWrapped("%s", bold_start);
            break;
        }

        char bold_text[1024];
        size_t bold_len = bold_end - (bold_start + 2);
        if (bold_len > sizeof(bold_text) - 1) bold_len = sizeof(bold_text) - 1;
        strncpy(bold_text, bold_start + 2, bold_len);
        bold_text[bold_len] = '\0';

        UI_TextColored((Vec4) { 1.0f, 1.0f, 0.5f, 1.0f }, "%s", bold_text);

        p = bold_end + 2;

        if (*p) {
            UI_SameLine(0, 0);
        }
    }
}

void RenderIOEditor(EntityType type, int index) {
    const TGD_EntityDef* def = NULL;
    if (type == ENTITY_BRUSH) {
        def = GameData_FindEntityDef(g_CurrentScene->brushes[index].classname);
    }
    else if (type == ENTITY_LOGIC) {
        def = GameData_FindEntityDef(g_CurrentScene->logicEntities[index].classname);
    }

    if (!def || def->num_outputs == 0) {
        return;
    }

    UI_Separator();
    UI_Text("Outputs");

    int total_target_names = 0;
    char** all_target_names = NULL;

    for (int i = 0; i < g_CurrentScene->numObjects; i++) if (strlen(g_CurrentScene->objects[i].targetname) > 0) { all_target_names = (char**)realloc(all_target_names, ++total_target_names * sizeof(char*)); all_target_names[total_target_names - 1] = g_CurrentScene->objects[i].targetname; }
    for (int i = 0; i < g_CurrentScene->numBrushes; i++) if (strlen(g_CurrentScene->brushes[i].targetname) > 0) { all_target_names = (char**)realloc(all_target_names, ++total_target_names * sizeof(char*)); all_target_names[total_target_names - 1] = g_CurrentScene->brushes[i].targetname; }
    for (int i = 0; i < g_CurrentScene->numActiveLights; i++) if (strlen(g_CurrentScene->lights[i].targetname) > 0) { all_target_names = (char**)realloc(all_target_names, ++total_target_names * sizeof(char*)); all_target_names[total_target_names - 1] = g_CurrentScene->lights[i].targetname; }
    for (int i = 0; i < g_CurrentScene->numSoundEntities; i++) if (strlen(g_CurrentScene->soundEntities[i].targetname) > 0) { all_target_names = (char**)realloc(all_target_names, ++total_target_names * sizeof(char*)); all_target_names[total_target_names - 1] = g_CurrentScene->soundEntities[i].targetname; }
    for (int i = 0; i < g_CurrentScene->numParticleEmitters; i++) if (strlen(g_CurrentScene->particleEmitters[i].targetname) > 0) { all_target_names = (char**)realloc(all_target_names, ++total_target_names * sizeof(char*)); all_target_names[total_target_names - 1] = g_CurrentScene->particleEmitters[i].targetname; }
    for (int i = 0; i < g_CurrentScene->numVideoPlayers; i++) if (strlen(g_CurrentScene->videoPlayers[i].targetname) > 0) { all_target_names = (char**)realloc(all_target_names, ++total_target_names * sizeof(char*)); all_target_names[total_target_names - 1] = g_CurrentScene->videoPlayers[i].targetname; }
    for (int i = 0; i < g_CurrentScene->numSprites; i++) if (strlen(g_CurrentScene->sprites[i].targetname) > 0) { all_target_names = (char**)realloc(all_target_names, ++total_target_names * sizeof(char*)); all_target_names[total_target_names - 1] = g_CurrentScene->sprites[i].targetname; }
    for (int i = 0; i < g_CurrentScene->numLogicEntities; i++) if (strlen(g_CurrentScene->logicEntities[i].targetname) > 0) { all_target_names = (char**)realloc(all_target_names, ++total_target_names * sizeof(char*)); all_target_names[total_target_names - 1] = g_CurrentScene->logicEntities[i].targetname; }

    for (int i = 0; i < def->num_outputs; ++i) {
        if (UI_CollapsingHeader(def->outputs[i].name, 1)) {
            int conn_to_delete = -1;
            for (int k = 0; k < g_num_io_connections; k++) {
                IOConnection* conn = &g_io_connections[k];
                if (conn->sourceType == type && conn->sourceIndex == index && strcmp(conn->outputName, def->outputs[i].name) == 0) {
                    UI_PushID(k);
                    char header_label[128];
                    sprintf(header_label, "To '%s' -> '%s'", conn->targetName, conn->inputName);
                    if (UI_CollapsingHeader(header_label, 1)) {
                        int current_target_idx = -1;
                        for (int j = 0; j < total_target_names; j++) {
                            if (strcmp(all_target_names[j], conn->targetName) == 0) {
                                current_target_idx = j;
                                break;
                            }
                        }
                        if (UI_Combo("Target", &current_target_idx, (const char* const*)all_target_names, total_target_names, -1)) {
                            if (current_target_idx >= 0) {
                                strncpy(conn->targetName, all_target_names[current_target_idx], sizeof(conn->targetName) - 1);
                                conn->inputName[0] = '\0';
                            }
                        }

                        EntityType target_type;
                        int target_index;
                        if (FindEntityInScene(g_CurrentScene, conn->targetName, &target_type, &target_index)) {
                            const TGD_EntityDef* target_def = NULL;
                            const char* classname = NULL;
                            const char* base_classname = NULL;

                            switch (target_type) {
                            case ENTITY_MODEL: base_classname = "_model_base"; break;
                            case ENTITY_LIGHT: base_classname = "_light_base"; break;
                            case ENTITY_SOUND: base_classname = "_sound_base"; break;
                            case ENTITY_PARTICLE_EMITTER: base_classname = "_particle_base"; break;
                            case ENTITY_VIDEO_PLAYER: base_classname = "_video_base"; break;
                            case ENTITY_SPRITE: base_classname = "_sprite_base"; break;
                            case ENTITY_BRUSH: classname = g_CurrentScene->brushes[target_index].classname; break;
                            case ENTITY_LOGIC: classname = g_CurrentScene->logicEntities[target_index].classname; break;
                            default: break;
                            }

                            if (classname && classname[0] != '\0') {
                                target_def = GameData_FindEntityDef(classname);
                            }
                            else if (base_classname) {
                                target_def = GameData_FindEntityDef(base_classname);
                            }

                            if (target_def && target_def->num_inputs > 0) {
                                const char** valid_inputs = (const char**)malloc(target_def->num_inputs * sizeof(const char*));
                                for (int j = 0; j < target_def->num_inputs; ++j) {
                                    valid_inputs[j] = target_def->inputs[j].name;
                                }

                                int current_input_idx = -1;
                                for (int j = 0; j < target_def->num_inputs; j++) {
                                    if (strcmp(valid_inputs[j], conn->inputName) == 0) {
                                        current_input_idx = j;
                                        break;
                                    }
                                }
                                if (UI_Combo("Input", &current_input_idx, valid_inputs, target_def->num_inputs, -1)) {
                                    if (current_input_idx >= 0) {
                                        strncpy(conn->inputName, valid_inputs[current_input_idx], sizeof(conn->inputName) - 1);
                                    }
                                }
                                free(valid_inputs);
                            }
                            else {
                                UI_InputText("Input", conn->inputName, sizeof(conn->inputName));
                            }
                        }
                        else {
                            UI_InputText("Input (Unknown Target)", conn->inputName, sizeof(conn->inputName));
                        }

                        UI_InputText("Parameter", conn->parameter, 64);
                        UI_DragFloat("Delay", &conn->delay, 0.1f, 0.0f, 300.0f);
                        UI_Selectable("Fire Once", &conn->fireOnce);
                        if (UI_Button("Delete Connection")) {
                            conn_to_delete = k;
                        }
                    }
                    UI_PopID();
                }
            }
            if (conn_to_delete != -1) { IO_RemoveConnection(conn_to_delete); }
            char add_label[64];
            sprintf(add_label, "Add Connection##%s", def->outputs[i].name);
            if (UI_Button(add_label)) { IO_AddConnection(type, index, def->outputs[i].name); }
        }
    }

    if (all_target_names) {
        free(all_target_names);
    }
}

// scan stuff

void FreeModelBrowserEntries() {
    if (g_EditorState.model_browser_entries) {
        for (int i = 0; i < g_EditorState.num_model_files; ++i) {
            free(g_EditorState.model_browser_entries[i].file_path);
            if (g_EditorState.model_browser_entries[i].thumbnail_texture != 0) {
                glDeleteTextures(1, &g_EditorState.model_browser_entries[i].thumbnail_texture);
            }
        }
        free(g_EditorState.model_browser_entries);
        g_EditorState.model_browser_entries = NULL;
        g_EditorState.num_model_files = 0;
    }
}

void ScanModelFiles() {
    FreeModelBrowserEntries();
    const char* dir_path = "models/";
#ifdef PLATFORM_WINDOWS
    char search_path[256];
    sprintf(search_path, "%s*.*", dir_path);
    WIN32_FIND_DATAA find_data;
    HANDLE h_find = FindFirstFileA(search_path, &find_data);
    if (h_find == INVALID_HANDLE_VALUE) return;
    do {
        if (!(find_data.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)) {
            const char* ext = strrchr(find_data.cFileName, '.');
            if (ext && (_stricmp(ext, ".gltf") == 0 || _stricmp(ext, ".glb") == 0)) {
                g_EditorState.model_browser_entries = realloc(g_EditorState.model_browser_entries, (g_EditorState.num_model_files + 1) * sizeof(ModelBrowserEntry));
                g_EditorState.model_browser_entries[g_EditorState.num_model_files].file_path = _strdup(find_data.cFileName);
                g_EditorState.model_browser_entries[g_EditorState.num_model_files].thumbnail_texture = 0;
                g_EditorState.num_model_files++;
            }
        }
    } while (FindNextFileA(h_find, &find_data) != 0);
    FindClose(h_find);
#else
    DIR* d = opendir(dir_path);
    if (!d) return;
    struct dirent* dir;
    while ((dir = readdir(d)) != NULL) {
        const char* ext = strrchr(dir->d_name, '.');
        if (ext && (_stricmp(ext, ".gltf") == 0 || _stricmp(ext, ".glb") == 0)) {
            g_EditorState.model_browser_entries = realloc(g_EditorState.model_browser_entries, (g_EditorState.num_model_files + 1) * sizeof(ModelBrowserEntry));
            g_EditorState.model_browser_entries[g_EditorState.num_model_files].file_path = strdup(dir->d_name);
            g_EditorState.model_browser_entries[g_EditorState.num_model_files].thumbnail_texture = 0;
            g_EditorState.num_model_files++;
        }
    }
    closedir(d);
#endif
}

void FreeDocFileList() {
    if (g_EditorState.doc_files) {
        for (int i = 0; i < g_EditorState.num_doc_files; ++i) {
            free(g_EditorState.doc_files[i]);
        }
        free(g_EditorState.doc_files);
        g_EditorState.doc_files = NULL;
        g_EditorState.num_doc_files = 0;
    }
}

void ScanDocFiles() {
    FreeDocFileList();
    const char* dir_path = "docs/";
#ifdef PLATFORM_WINDOWS
    char search_path[256];
    sprintf(search_path, "%s*.md", dir_path);
    WIN32_FIND_DATAA find_data;
    HANDLE h_find = FindFirstFileA(search_path, &find_data);
    if (h_find == INVALID_HANDLE_VALUE) return;
    do {
        if (!(find_data.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)) {
            g_EditorState.doc_files = realloc(g_EditorState.doc_files, (g_EditorState.num_doc_files + 1) * sizeof(char*));
            g_EditorState.doc_files[g_EditorState.num_doc_files] = _strdup(find_data.cFileName);
            g_EditorState.num_doc_files++;
        }
    } while (FindNextFileA(h_find, &find_data) != 0);
    FindClose(h_find);
#else
    DIR* d = opendir(dir_path);
    if (!d) return;
    struct dirent* dir;
    while ((dir = readdir(d)) != NULL) {
        const char* ext = strrchr(dir->d_name, '.');
        if (ext && (_stricmp(ext, ".md") == 0)) {
            g_EditorState.doc_files = realloc(g_EditorState.doc_files, (g_EditorState.num_doc_files + 1) * sizeof(char*));
            g_EditorState.doc_files[g_EditorState.num_doc_files] = strdup(dir->d_name);
            g_EditorState.num_doc_files++;
        }
    }
    closedir(d);
#endif
}

void FreeSoundFileList() {
    if (g_EditorState.sound_file_list) {
        for (int i = 0; i < g_EditorState.num_sound_files; ++i) {
            free(g_EditorState.sound_file_list[i]);
        }
        free(g_EditorState.sound_file_list);
        g_EditorState.sound_file_list = NULL;
        g_EditorState.num_sound_files = 0;
    }
}

void ScanSoundFiles() {
    FreeSoundFileList();
    const char* dir_path = "sounds/";
#ifdef PLATFORM_WINDOWS
    char search_path[256];
    sprintf(search_path, "%s*.*", dir_path);
    WIN32_FIND_DATAA find_data;
    HANDLE h_find = FindFirstFileA(search_path, &find_data);
    if (h_find == INVALID_HANDLE_VALUE) return;
    do {
        if (!(find_data.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)) {
            const char* ext = strrchr(find_data.cFileName, '.');
            if (ext && (_stricmp(ext, ".wav") == 0 || _stricmp(ext, ".mp3") == 0 || _stricmp(ext, ".ogg") == 0)) {
                g_EditorState.sound_file_list = realloc(g_EditorState.sound_file_list, (g_EditorState.num_sound_files + 1) * sizeof(char*));
                g_EditorState.sound_file_list[g_EditorState.num_sound_files] = _strdup(find_data.cFileName);
                g_EditorState.num_sound_files++;
            }
        }
    } while (FindNextFileA(h_find, &find_data) != 0);
    FindClose(h_find);
#else
    DIR* d = opendir(dir_path);
    if (!d) return;
    struct dirent* dir;
    while ((dir = readdir(d)) != NULL) {
        const char* ext = strrchr(dir->d_name, '.');
        if (ext && (_stricmp(ext, ".wav") == 0 || _stricmp(ext, ".mp3") == 0 || _stricmp(ext, ".ogg") == 0)) {
            g_EditorState.sound_file_list = realloc(g_EditorState.sound_file_list, (g_EditorState.num_sound_files + 1) * sizeof(char*));
            g_EditorState.sound_file_list[g_EditorState.num_sound_files] = strdup(dir->d_name);
            g_EditorState.num_sound_files++;
        }
    }
    closedir(d);
#endif
}

void FreeMapFileList() {
    if (g_EditorState.map_file_list) {
        for (int i = 0; i < g_EditorState.num_map_files; ++i) {
            free(g_EditorState.map_file_list[i]);
        }
        free(g_EditorState.map_file_list);
        g_EditorState.map_file_list = NULL;
        g_EditorState.num_map_files = 0;
    }
}

void ScanMapFiles() {
    FreeMapFileList();
    const char* dir_path = "./";
#ifdef PLATFORM_WINDOWS
    char search_path[256];
    sprintf(search_path, "%s*.map", dir_path);
    WIN32_FIND_DATAA find_data;
    HANDLE h_find = FindFirstFileA(search_path, &find_data);
    if (h_find == INVALID_HANDLE_VALUE) return;
    do {
        if (!(find_data.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)) {
            g_EditorState.map_file_list = realloc(g_EditorState.map_file_list, (g_EditorState.num_map_files + 1) * sizeof(char*));
            g_EditorState.map_file_list[g_EditorState.num_map_files] = _strdup(find_data.cFileName);
            g_EditorState.num_map_files++;
        }
    } while (FindNextFileA(h_find, &find_data) != 0);
    FindClose(h_find);
#else
    DIR* d = opendir(dir_path);
    if (!d) return;
    struct dirent* dir;
    while ((dir = readdir(d)) != NULL) {
        const char* ext = strrchr(dir->d_name, '.');
        if (ext && (_stricmp(ext, ".map") == 0)) {
            g_EditorState.map_file_list = realloc(g_EditorState.map_file_list, (g_EditorState.num_map_files + 1) * sizeof(char*));
            g_EditorState.map_file_list[g_EditorState.num_map_files] = strdup(dir->d_name);
            g_EditorState.num_map_files++;
        }
    }
    closedir(d);
#endif
}

// end scan stuff

// Render seperate windows

void Editor_RenderModelBrowser(Scene* scene, Engine* engine, Renderer* renderer) {
    if (!g_EditorState.show_add_model_popup) return;

    if (g_EditorState.model_browser_entries == NULL) {
        ScanModelFiles();
    }

    UI_SetNextWindowSize(700, 500);
    if (UI_Begin("Model Browser", &g_EditorState.show_add_model_popup)) {
        UI_InputText("Search", g_EditorState.model_search_filter, sizeof(g_EditorState.model_search_filter));
        UI_SameLine();
        if (UI_Button("Refresh List")) {
            ScanModelFiles();
        }
        UI_Separator();

        if (UI_BeginChild("model_grid_child", 0, 0, false, 0)) {
            float window_visible_x2 = UI_GetWindowPos_X() + UI_GetWindowContentRegionMax_X();
            float style_spacing_x = UI_GetStyle_ItemSpacing_X();
            float item_size = 96.0f;

            for (int i = 0; i < g_EditorState.num_model_files; ++i) {
                ModelBrowserEntry* entry = &g_EditorState.model_browser_entries[i];
                if (g_EditorState.model_search_filter[0] != '\0' && _stristr(entry->file_path, g_EditorState.model_search_filter) == NULL) {
                    continue;
                }

                if (entry->thumbnail_texture == 0) {
                    char path_buffer[256];
                    sprintf(path_buffer, "models/%s", entry->file_path);
                    g_is_thumbnail_mode = true;
                    LoadedModel* temp_model = Model_Load(path_buffer);

                    glGenTextures(1, &entry->thumbnail_texture);
                    glBindTexture(GL_TEXTURE_2D, entry->thumbnail_texture);
                    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 128, 128, 0, GL_RGBA, GL_UNSIGNED_BYTE, NULL);
                    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
                    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

                    if (temp_model) {
                        glBindFramebuffer(GL_FRAMEBUFFER, g_EditorState.model_thumb_fbo);
                        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, entry->thumbnail_texture, 0);
                        glViewport(0, 0, 128, 128);
                        glClearColor(0.2f, 0.2f, 0.25f, 1.0f);
                        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

                        Mat4 view = mat4_lookAt((Vec3) { 1, 1, 1 }, (Vec3) { 0, 0, 0 }, (Vec3) { 0, 1, 0 });
                        Mat4 proj = mat4_perspective(45.0f * (M_PI / 180.0f), 1.0f, 0.1f, 100.0f);

                        glUseProgram(renderer->mainShader);
                        glUniform1i(glGetUniformLocation(renderer->mainShader, "is_unlit"), 1);
                        glUniformMatrix4fv(glGetUniformLocation(renderer->mainShader, "view"), 1, GL_FALSE, view.m);
                        glUniformMatrix4fv(glGetUniformLocation(renderer->mainShader, "projection"), 1, GL_FALSE, proj.m);

                        SceneObject temp_obj;
                        memset(&temp_obj, 0, sizeof(SceneObject));
                        temp_obj.model = temp_model;
                        mat4_identity(&temp_obj.modelMatrix);
                        render_object(renderer, g_CurrentScene, renderer->mainShader, &temp_obj, false, NULL);

                        Model_Free(temp_model);
                        g_is_thumbnail_mode = false;
                        glBindFramebuffer(GL_FRAMEBUFFER, 0);
                    }
                }

                UI_PushID(i);
                UI_BeginGroup();

                if (UI_ImageButton_Flip("##thumb", (void*)(intptr_t)entry->thumbnail_texture, item_size, item_size)) {
                    if (g_EditorState.texture_browser_target == MODEL_BROWSER_TARGET_SPRINKLE) {
                        char full_path[256];
                        sprintf(full_path, "models/%s", g_EditorState.model_browser_entries[i].file_path);
                        strncpy(g_EditorState.sprinkle_model_path, full_path, sizeof(g_EditorState.sprinkle_model_path) - 1);
                        g_EditorState.show_add_model_popup = false;
                    }
                    else {
                        if (scene->numObjects < MAX_MODELS) {
                            scene->numObjects++;
                            scene->objects = realloc(scene->objects, scene->numObjects * sizeof(SceneObject));
                            SceneObject* newObj = &scene->objects[scene->numObjects - 1];
                            memset(newObj, 0, sizeof(SceneObject));

                            mat4_identity(&newObj->animated_local_transform);

                            char full_model_path[256];
                            sprintf(full_model_path, "models/%s", g_EditorState.model_browser_entries[i].file_path);
                            strncpy(newObj->modelPath, full_model_path, sizeof(newObj->modelPath) - 1);

                            Vec3 forward = { cosf(g_EditorState.editor_camera.pitch) * sinf(g_EditorState.editor_camera.yaw), sinf(g_EditorState.editor_camera.pitch), -cosf(g_EditorState.editor_camera.pitch) * cosf(g_EditorState.editor_camera.yaw) };
                            vec3_normalize(&forward);
                            newObj->pos = vec3_add(g_EditorState.editor_camera.position, vec3_muls(forward, 10.0f));
                            newObj->scale = (Vec3){ 1,1,1 };
                            newObj->casts_shadows = true;
                            SceneObject_UpdateMatrix(newObj);

                            newObj->model = Model_Load(newObj->modelPath);

                            if (newObj->model && newObj->model->combinedVertexData && newObj->model->totalIndexCount > 0) {
                                Mat4 physics_transform = create_trs_matrix(newObj->pos, newObj->rot, (Vec3) { 1, 1, 1 });
                                newObj->physicsBody = Physics_CreateStaticTriangleMesh(engine->physicsWorld, newObj->model->combinedVertexData, newObj->model->totalVertexCount, newObj->model->combinedIndexData, newObj->model->totalIndexCount, physics_transform, newObj->scale);
                            }
                            Undo_PushCreateEntity(scene, ENTITY_MODEL, scene->numObjects - 1, "Create Model");
                            g_EditorState.show_add_model_popup = false;
                        }
                        else {
                            Console_Printf_Error("Cannot add model, MAX_MODELS limit reached.");
                        }
                    }
                }

                UI_TextWrapped(entry->file_path);
                UI_EndGroup();

                float last_button_x2 = UI_GetItemRectMax_X();
                float next_button_x2 = last_button_x2 + style_spacing_x + item_size;
                if (i + 1 < g_EditorState.num_model_files && next_button_x2 < window_visible_x2) {
                    UI_SameLine();
                }
                UI_PopID();
            }
        }
        UI_EndChild();
    }
    UI_End();
}

void Editor_RenderSoundBrowser(Scene* scene) {
    if (!g_EditorState.show_sound_browser_popup) return;

    UI_SetNextWindowSize(400, 500);
    if (UI_Begin("Sound Browser", &g_EditorState.show_sound_browser_popup)) {
        UI_InputText("Search", g_EditorState.sound_search_filter, sizeof(g_EditorState.sound_search_filter));
        UI_Separator();

        if (UI_BeginChild("sound_list_child", 0, -40, true, 0)) {
            if (g_EditorState.num_sound_files > 0) {
                for (int i = 0; i < g_EditorState.num_sound_files; ++i) {
                    const char* sound_name = g_EditorState.sound_file_list[i];
                    if (g_EditorState.sound_search_filter[0] == '\0' || _stristr(sound_name, g_EditorState.sound_search_filter) != NULL) {
                        if (UI_Selectable(sound_name, g_EditorState.selected_sound_file_index == i)) {
                            g_EditorState.selected_sound_file_index = i;
                            if (g_EditorState.preview_sound_source) SoundSystem_DeleteSource(g_EditorState.preview_sound_source);
                            if (g_EditorState.preview_sound_buffer) SoundSystem_DeleteBuffer(g_EditorState.preview_sound_buffer);
                            char path_buffer[256];
                            sprintf(path_buffer, "sounds/%s", g_EditorState.sound_file_list[i]);
                            g_EditorState.preview_sound_buffer = SoundSystem_LoadSound(path_buffer);
                            if (g_EditorState.preview_sound_buffer != 0) {
                                g_EditorState.preview_sound_source = SoundSystem_PlaySound(g_EditorState.preview_sound_buffer, g_EditorState.editor_camera.position, 10.0f, 1.0f, 1000.0f, false);
                            }
                        }
                    }
                }
            }
        }
        UI_EndChild();

        UI_Separator();

        if (g_EditorState.selected_sound_file_index != -1) {
            if (UI_Button("Add to Scene")) {
                if (scene->numSoundEntities < MAX_SOUNDS) {
                    SoundEntity* s = &scene->soundEntities[scene->numSoundEntities];
                    memset(s, 0, sizeof(SoundEntity));
                    sprintf(s->targetname, "Sound_%d", scene->numSoundEntities);
                    char full_path[256];
                    sprintf(full_path, "sounds/%s", g_EditorState.sound_file_list[g_EditorState.selected_sound_file_index]);
                    strncpy(s->soundPath, full_path, sizeof(s->soundPath) - 1);
                    s->pos = g_EditorState.editor_camera.position;
                    s->volume = 1.0f;
                    s->pitch = 1.0f;
                    s->maxDistance = 50.0f;
                    s->bufferID = SoundSystem_LoadSound(s->soundPath);
                    SoundSystem_SetSourceIsGlobal(s->sourceID, s->isGlobal);
                    scene->numSoundEntities++;
                    Undo_PushCreateEntity(scene, ENTITY_SOUND, scene->numSoundEntities - 1, "Create Sound");
                    g_EditorState.show_sound_browser_popup = false;
                }
                else {
                    Console_Printf_Error("[error] Max sound entities reached.");
                }
            }
            UI_SameLine();
            if (UI_Button("Preview")) {
                if (g_EditorState.preview_sound_source) SoundSystem_DeleteSource(g_EditorState.preview_sound_source);
                if (g_EditorState.preview_sound_buffer) {
                    g_EditorState.preview_sound_source = SoundSystem_PlaySound(g_EditorState.preview_sound_buffer, g_EditorState.editor_camera.position, 10.0f, 1.0f, 1000.0f, false);
                }
            }
        }
    }
    if (!g_EditorState.show_sound_browser_popup) {
        if (g_EditorState.preview_sound_source) {
            SoundSystem_DeleteSource(g_EditorState.preview_sound_source);
            g_EditorState.preview_sound_source = 0;
        }
        if (g_EditorState.preview_sound_buffer) {
            SoundSystem_DeleteBuffer(g_EditorState.preview_sound_buffer);
            g_EditorState.preview_sound_buffer = 0;
        }
    }
    UI_End();
}

void Editor_RenderHelpWindow() {
    if (!g_EditorState.show_help_window) return;

    UI_SetNextWindowSize(800, 600);
    if (UI_Begin("Help & Documentation", &g_EditorState.show_help_window)) {
        UI_BeginChild("doc_list_child", 200, 0, true, 0);
        if (UI_Button("Refresh List")) {
            ScanDocFiles();
        }
        UI_Separator();
        if (g_EditorState.num_doc_files > 0) {
            for (int i = 0; i < g_EditorState.num_doc_files; ++i) {
                if (UI_Selectable(g_EditorState.doc_files[i], g_EditorState.selected_doc_index == i)) {
                    g_EditorState.selected_doc_index = i;
                    char path_buffer[256];
                    sprintf(path_buffer, "docs/%s", g_EditorState.doc_files[i]);

                    FILE* f = fopen(path_buffer, "rb");
                    if (f) {
                        fseek(f, 0, SEEK_END);
                        long length = ftell(f);
                        fseek(f, 0, SEEK_SET);
                        if (g_EditorState.current_doc_content) {
                            free(g_EditorState.current_doc_content);
                        }
                        g_EditorState.current_doc_content = malloc(length + 1);
                        if (g_EditorState.current_doc_content) {
                            fread(g_EditorState.current_doc_content, 1, length, f);
                            g_EditorState.current_doc_content[length] = '\0';
                        }
                        fclose(f);
                    }
                }
            }
        }
        UI_EndChild();
        UI_SameLine();

        UI_BeginChild("doc_preview_child", 0, 0, true, 0);
        if (g_EditorState.current_doc_content) {
            char* content_copy = strdup(g_EditorState.current_doc_content);
            char* line = strtok(content_copy, "\n");
            bool in_table = false;
            bool in_code_block = false;

            while (line) {
                if (strncmp(line, "```", 3) == 0) {
                    in_code_block = !in_code_block;
                    line = strtok(NULL, "\n");
                    continue;
                }

                if (in_code_block) {
                    UI_TextColored((Vec4) { 0.8f, 0.9f, 1.0f, 1.0f }, "%s", line);
                    line = strtok(NULL, "\n");
                    continue;
                }
                if (strncmp(line, "|", 1) == 0) {
                    if (!in_table) {
                        int columns = 0;
                        for (const char* p = line; *p; p++) if (*p == '|') columns++;
                        if (columns > 1) {
                            if (UI_BeginTable("md_table", columns - 1, 1 | (1 << 6), 0, 0)) {
                                in_table = true;
                            }
                        }
                    }

                    char next_line_peek = "";
                    char* next_line_ptr = strtok(NULL, "\n");
                    if (next_line_ptr) strcpy(next_line_peek, next_line_ptr);

                    if (in_table && strncmp(next_line_peek, "|:---", 5) == 0) {
                        UI_TableHeadersRow();
                        render_markdown_line(line);
                        line = strtok(NULL, "\n");
                        line = strtok(NULL, "\n");
                    }
                    else if (in_table) {
                        UI_TableNextRow();
                        render_markdown_line(line);
                    }
                    line = next_line_ptr;
                }
                else {
                    if (in_table) {
                        UI_EndTable();
                        in_table = false;
                    }
                    render_markdown_line(line);
                    line = strtok(NULL, "\n");
                }
            }
            if (in_table) {
                UI_EndTable();
            }
            free(content_copy);
        }
        else {
            UI_Text("Select a document to view.");
        }
        UI_EndChild();
    }
    UI_End();
}

void Editor_RenderVertexToolsWindow(Scene* scene) {
    if (!g_EditorState.show_vertex_tools_window) {
        return;
    }

    UI_SetNextWindowSize(250, 0);
    if (UI_Begin("Vertex Tools", &g_EditorState.show_vertex_tools_window)) {
        if (g_EditorState.is_sculpting_mode_enabled) {
            UI_Text("Sculpting");
            UI_Text("Hold Shift to Smooth");
            UI_Text("Hold Ctrl to Lower");
            UI_Separator();
            UI_DragFloat("Radius##Sculpt", &g_EditorState.sculpt_brush_radius, 0.1f, 0.1f, 50.0f);
            UI_DragFloat("Strength##Sculpt", &g_EditorState.sculpt_brush_strength, 0.05f, 0.01f, 5.0f);

            if (UI_Button("Apply Noise...")) {
                g_EditorState.show_sculpt_noise_popup = true;
            }
        }
        else if (g_EditorState.is_painting_mode_enabled) {
            UI_Text("Vertex Painting");
            UI_Separator();
            UI_DragFloat("Radius##Paint", &g_EditorState.paint_brush_radius, 0.1f, 0.1f, 50.0f);
            UI_DragFloat("Strength##Paint", &g_EditorState.paint_brush_strength, 0.05f, 0.1f, 5.0f);

            UI_Separator();
            UI_Text("Paint Channel:");
            if (UI_RadioButton("R (Tex 2)", g_EditorState.paint_channel == 0)) { g_EditorState.paint_channel = 0; }
            if (UI_RadioButton("G (Tex 3)", g_EditorState.paint_channel == 1)) { g_EditorState.paint_channel = 1; }
            if (UI_RadioButton("B (Tex 4)", g_EditorState.paint_channel == 2)) { g_EditorState.paint_channel = 2; }

            UI_Separator();
            if (UI_Button("Erase All Paint")) {
                EditorSelection* primary = Editor_GetPrimarySelection();
                if (primary && primary->type == ENTITY_BRUSH) {
                    Brush* b = &scene->brushes[primary->index];
                    if (b->numVertices > 0) {
                        Undo_BeginEntityModification(scene, ENTITY_BRUSH, primary->index);

                        for (int i = 0; i < b->numVertices; ++i) {
                            b->vertices[i].color.x = 0.0f;
                            b->vertices[i].color.y = 0.0f;
                            b->vertices[i].color.z = 0.0f;
                        }

                        Brush_CreateRenderData(b);
                        Undo_EndEntityModification(scene, ENTITY_BRUSH, primary->index, "Erase All Vertex Paint");
                    }
                }
            }
            UI_SameLine();
            if (UI_Button("Invert Channel")) {
                if (g_EditorState.num_selections > 0) {
                    Undo_BeginMultiEntityModification(scene, g_EditorState.selections, g_EditorState.num_selections);

                    bool modified_brushes[MAX_BRUSHES] = { false };

                    for (int i = 0; i < g_EditorState.num_selections; ++i) {
                        EditorSelection* sel = &g_EditorState.selections[i];
                        if (sel->type == ENTITY_BRUSH && sel->face_index != -1) {
                            Brush* b = &scene->brushes[sel->index];
                            BrushFace* face = &b->faces[sel->face_index];

                            for (int j = 0; j < face->numVertexIndices; j++) {
                                int vert_idx = face->vertexIndices[j];
                                BrushVertex* vert = &b->vertices[vert_idx];

                                switch (g_EditorState.paint_channel) {
                                case 0: vert->color.x = 1.0f - vert->color.x; break;
                                case 1: vert->color.y = 1.0f - vert->color.y; break;
                                case 2: vert->color.z = 1.0f - vert->color.z; break;
                                }
                            }
                            modified_brushes[sel->index] = true;
                        }
                    }

                    for (int i = 0; i < scene->numBrushes; i++) {
                        if (modified_brushes[i]) {
                            Brush_CreateRenderData(&scene->brushes[i]);
                        }
                    }

                    Undo_EndMultiEntityModification(scene, g_EditorState.selections, g_EditorState.num_selections, "Invert Vertex Paint");
                }
            }
        }
    }
    UI_End();

    if (!g_EditorState.show_vertex_tools_window) {
        g_EditorState.is_painting_mode_enabled = false;
        g_EditorState.is_sculpting_mode_enabled = false;
    }
}

void Editor_RenderSculptNoisePopup(Scene* scene) {
    if (g_EditorState.show_sculpt_noise_popup) {
        UI_OpenPopup("Apply Noise");
        g_EditorState.show_sculpt_noise_popup = false;
    }

    if (UI_BeginPopupModal("Apply Noise", NULL, 0)) {
        static float min_noise = -0.5f;
        static float max_noise = 0.5f;
        static float frequency = 0.2f;
        static int octaves = 4;
        static float lacunarity = 2.0f;
        static float persistence = 0.5f;

        UI_Text("Apply smooth procedural noise to all vertices.");
        UI_Separator();
        UI_DragFloat("Min Displacement", &min_noise, 0.05f, -10.0f, 10.0f);
        UI_DragFloat("Max Displacement", &max_noise, 0.05f, -10.0f, 10.0f);
        UI_Separator();
        UI_DragFloat("Frequency", &frequency, 0.01f, 0.01f, 2.0f);
        UI_DragInt("Octaves", &octaves, 1, 1, 8);
        UI_DragFloat("Lacunarity", &lacunarity, 0.1f, 1.5f, 4.0f);
        UI_DragFloat("Persistence", &persistence, 0.05f, 0.1f, 1.0f);
        UI_Separator();

        if (UI_Button("Apply")) {
            EditorSelection* primary = Editor_GetPrimarySelection();
            if (primary && primary->type == ENTITY_BRUSH) {
                Brush* b = &scene->brushes[primary->index];
                if (b->numVertices > 0) {
                    Undo_BeginEntityModification(scene, ENTITY_BRUSH, primary->index);

                    for (int i = 0; i < b->numVertices; ++i) {
                        float total = 0.0f;
                        float freq = frequency;
                        float amp = 1.0f;
                        float maxAmp = 0.0f;

                        for (int j = 0; j < octaves; ++j) {
                            float n = sin(b->vertices[i].pos.x * freq) * cos(b->vertices[i].pos.z * freq);
                            total += n * amp;
                            maxAmp += amp;
                            amp *= persistence;
                            freq *= lacunarity;
                        }

                        if (maxAmp > 0.0) {
                            total /= maxAmp;
                        }

                        float noise_val = min_noise + (total * 0.5f + 0.5f) * (max_noise - min_noise);
                        b->vertices[i].pos.y += noise_val;
                    }

                    Brush_CreateRenderData(b);
                    Undo_EndEntityModification(scene, ENTITY_BRUSH, primary->index, "Apply Smooth Noise to Brush");
                }
            }
            UI_CloseCurrentPopup();
        }
        UI_SameLine();
        if (UI_Button("Cancel")) {
            UI_CloseCurrentPopup();
        }
        UI_EndPopup();
    }
}

void Editor_RenderAboutWindow() {
    if (!g_EditorState.show_about_window) {
        return;
    }

    UI_SetNextWindowSize(320, 180);
    if (UI_Begin("About Tectonic Editor", &g_EditorState.show_about_window)) {
        UI_Text("Tectonic Editor");
        UI_Separator();
        UI_Text("Version: D.E.V. (Build %d)", Compat_GetBuildNumber());
        UI_Text("Build Date: %s, %s", __DATE__, __TIME__);
        UI_Text("Architecture: %s", ARCH_STRING);
        UI_Text("Engine branch: %s", BRANCH_NAME);
        UI_Separator();
        UI_Text("Copyright (c) 2025 Soft Sprint Studios");
        UI_Text("All rights reserved.");
        UI_Separator();

        if (UI_Button("OK")) {
            g_EditorState.show_about_window = false;
        }
    }
    UI_End();
}

void Editor_RenderTextureBrowser(Scene* scene) {
    if (!g_EditorState.show_texture_browser) return;
    EditorSelection* primary = Editor_GetPrimarySelection();

    UI_SetNextWindowSize(600, 500);
    if (UI_Begin("Texture Browser", &g_EditorState.show_texture_browser)) {
        UI_InputText("Search", g_EditorState.texture_search_filter, sizeof(g_EditorState.texture_search_filter));
        UI_Separator();

        float window_visible_x2 = UI_GetWindowPos_X() + UI_GetWindowContentRegionMax_X();
        float style_spacing_x = UI_GetStyle_ItemSpacing_X();
        int mat_count = TextureManager_GetMaterialCount();

        for (int i = 0; i < mat_count; ++i) {
            Material* mat = TextureManager_GetMaterial(i);

            if (g_EditorState.texture_search_filter[0] != '\0' &&
                _stristr(mat->name, g_EditorState.texture_search_filter) == NULL) {
                continue;
            }

            if (strncmp(mat->diffusePath, "models\\", strlen("models\\")) == 0 ||
                strncmp(mat->normalPath, "models\\", strlen("models\\")) == 0 ||
                strncmp(mat->rmaPath, "models\\", strlen("models\\")) == 0) {
                continue;
            }

            if (!mat->isLoaded && mat->diffuseMap == 0) {
                if (strlen(mat->diffusePath) > 0) {
                    mat->diffuseMap = loadTexture(mat->diffusePath, true, TEXTURE_LOAD_CONTEXT_UI_THUMBNAIL);
                }
                else {
                    mat->diffuseMap = missingTextureID;
                }
            }

            UI_PushID(i);
            char btn_id[32];
            sprintf(btn_id, "##mat_btn_%d", i);
            if (UI_ImageButton(btn_id, (void*)(intptr_t)mat->diffuseMap, 64, 64)) {
                bool is_face_material_target = (g_EditorState.texture_browser_target >= 0 && g_EditorState.texture_browser_target <= 3);

                if (g_EditorState.num_selections > 0 && is_face_material_target) {
                    Undo_BeginMultiEntityModification(scene, g_EditorState.selections, g_EditorState.num_selections);

                    for (int sel_idx = 0; sel_idx < g_EditorState.num_selections; ++sel_idx) {
                        EditorSelection* sel = &g_EditorState.selections[sel_idx];
                        if (sel->type == ENTITY_BRUSH && sel->face_index != -1) {
                            Brush* b = &scene->brushes[sel->index];
                            BrushFace* face = &b->faces[sel->face_index];

                            switch (g_EditorState.texture_browser_target) {
                            case 0: face->material = mat; break;
                            case 1: face->material2 = mat; break;
                            case 2: face->material3 = mat; break;
                            case 3: face->material4 = mat; break;
                            }
                            Brush_CreateRenderData(b);
                        }
                    }

                    Undo_EndMultiEntityModification(scene, g_EditorState.selections, g_EditorState.num_selections, "Change Face Materials");
                    g_EditorState.show_texture_browser = false;
                }
                else if (primary && primary->type == ENTITY_BRUSH && g_EditorState.texture_browser_target >= 100 && g_EditorState.texture_browser_target < 200) {
                    int prop_index = g_EditorState.texture_browser_target - 100;
                    Brush* b = &scene->brushes[primary->index];
                    if (prop_index < b->numProperties) {
                        Undo_BeginEntityModification(scene, ENTITY_BRUSH, primary->index);
                        strncpy(b->properties[prop_index].value, mat->name, sizeof(b->properties[prop_index].value) - 1);
                        Undo_EndEntityModification(scene, ENTITY_BRUSH, primary->index, "Set Brush Texture Property");
                    }
                    g_EditorState.show_texture_browser = false;
                }
                else if (primary && primary->type == ENTITY_LOGIC && g_EditorState.texture_browser_target >= 200) {
                    int prop_index = g_EditorState.texture_browser_target - 200;
                    LogicEntity* ent = &scene->logicEntities[primary->index];
                    if (prop_index < ent->numProperties) {
                        Undo_BeginEntityModification(scene, ENTITY_LOGIC, primary->index);
                        strncpy(ent->properties[prop_index].value, mat->name, sizeof(ent->properties[prop_index].value) - 1);
                        Undo_EndEntityModification(scene, ENTITY_LOGIC, primary->index, "Set Logic Texture Property");
                    }
                    g_EditorState.show_texture_browser = false;
                }
                else if (primary && primary->type == ENTITY_DECAL && g_EditorState.texture_browser_target == 5) {
                    Decal* d = &scene->decals[primary->index];
                    Undo_BeginEntityModification(scene, ENTITY_DECAL, primary->index);
                    d->material = mat;
                    Undo_EndEntityModification(scene, ENTITY_DECAL, primary->index, "Change Decal Material");
                    g_EditorState.show_texture_browser = false;
                }
                else if (primary && primary->type == ENTITY_LIGHT && g_EditorState.texture_browser_target == 4) {
                    Light* light = &scene->lights[primary->index];
                    Undo_BeginEntityModification(scene, ENTITY_LIGHT, primary->index);
                    strncpy(light->cookiePath, mat->name, sizeof(light->cookiePath) - 1);
                    light->cookiePath[sizeof(light->cookiePath) - 1] = '\0';
                    light->cookieMap = mat->diffuseMap;
                    if (light->cookieMapHandle != 0) { glMakeTextureHandleNonResidentARB(light->cookieMapHandle); }
                    light->cookieMapHandle = glGetTextureHandleARB(light->cookieMap);
                    glMakeTextureHandleResidentARB(light->cookieMapHandle);
                    Undo_EndEntityModification(scene, ENTITY_LIGHT, primary->index, "Set Light Cookie");
                    g_EditorState.show_texture_browser = false;
                }
                else if (primary && primary->type == ENTITY_SPRITE && g_EditorState.texture_browser_target == 6) {
                    Undo_BeginEntityModification(scene, ENTITY_SPRITE, primary->index);
                    scene->sprites[primary->index].material = mat;
                    Undo_EndEntityModification(scene, ENTITY_SPRITE, primary->index, "Change Sprite Material");
                    g_EditorState.show_texture_browser = false;
                }
                else if (g_EditorState.texture_browser_target == TEXTURE_TARGET_REPLACE_FIND) {
                    g_EditorState.find_material_index = i;
                    g_EditorState.show_texture_browser = false;
                }
                else if (g_EditorState.texture_browser_target == TEXTURE_TARGET_REPLACE_WITH) {
                    g_EditorState.replace_material_index = i;
                    g_EditorState.show_texture_browser = false;
                }
            }

            if (UI_IsItemHovered()) {
                UI_BeginTooltip();
                UI_Text(mat->name);
                UI_Image((void*)(intptr_t)mat->diffuseMap, 256, 256);
                UI_EndTooltip();
            }

            float last_button_x2 = UI_GetItemRectMax_X();
            float next_button_x2 = last_button_x2 + style_spacing_x + 64;
            if (i + 1 < mat_count && next_button_x2 < window_visible_x2) {
                UI_SameLine();
            }
            UI_PopID();
        }
    }
    UI_End();
}

void Editor_RenderReplaceTexturesUI(Scene* scene) {
    if (!g_EditorState.show_replace_textures_popup) {
        return;
    }

    UI_SetNextWindowSize(350, 400);
    if (UI_Begin("Replace Textures", &g_EditorState.show_replace_textures_popup)) {
        UI_Text("Find Material:");
        Material* find_mat = (g_EditorState.find_material_index != -1) ? TextureManager_GetMaterial(g_EditorState.find_material_index) : NULL;
        char find_button_label[128];
        sprintf(find_button_label, "%s##Find", find_mat ? find_mat->name : "None");
        if (UI_Button(find_button_label)) {
            g_EditorState.texture_browser_target = TEXTURE_TARGET_REPLACE_FIND;
            g_EditorState.show_texture_browser = true;
        }
        if (find_mat) {
            UI_Image((void*)(intptr_t)find_mat->diffuseMap, 64, 64);
        }

        UI_Separator();

        UI_Text("Replace With:");
        Material* replace_mat = (g_EditorState.replace_material_index != -1) ? TextureManager_GetMaterial(g_EditorState.replace_material_index) : NULL;
        char replace_button_label[128];
        sprintf(replace_button_label, "%s##Replace", replace_mat ? replace_mat->name : "None");
        if (UI_Button(replace_button_label)) {
            g_EditorState.texture_browser_target = TEXTURE_TARGET_REPLACE_WITH;
            g_EditorState.show_texture_browser = true;
        }
        if (replace_mat) {
            UI_Image((void*)(intptr_t)replace_mat->diffuseMap, 64, 64);
        }

        UI_Separator();

        if (UI_Button("Replace All in Scene")) {
            if (g_EditorState.find_material_index != -1 && g_EditorState.replace_material_index != -1 && g_EditorState.find_material_index != g_EditorState.replace_material_index) {
                Material* find_mat_ptr = TextureManager_GetMaterial(g_EditorState.find_material_index);
                Material* replace_mat_ptr = TextureManager_GetMaterial(g_EditorState.replace_material_index);
                int replaced_count = 0;

                for (int i = 0; i < scene->numBrushes; ++i) {
                    Brush* b = &scene->brushes[i];
                    bool brush_modified = false;

                    for (int j = 0; j < b->numFaces; ++j) {
                        BrushFace* face = &b->faces[j];
                        if (face->material == find_mat_ptr) { face->material = replace_mat_ptr; brush_modified = true; replaced_count++; }
                        if (face->material2 == find_mat_ptr) { face->material2 = replace_mat_ptr; brush_modified = true; replaced_count++; }
                        if (face->material3 == find_mat_ptr) { face->material3 = replace_mat_ptr; brush_modified = true; replaced_count++; }
                        if (face->material4 == find_mat_ptr) { face->material4 = replace_mat_ptr; brush_modified = true; replaced_count++; }
                    }

                    if (brush_modified) {
                        Undo_BeginEntityModification(scene, ENTITY_BRUSH, i);
                        Brush_CreateRenderData(b);
                        Undo_EndEntityModification(scene, ENTITY_BRUSH, i, "Replace Textures");
                    }
                }
                g_EditorState.show_replace_textures_popup = false;
            }
        }
    }
    UI_End();
}

void Editor_RenderFaceEditSheet(Scene* scene, Engine* engine) {
    UI_SetNextWindowSize(320, 520);
    if (UI_Begin_NoClose("Face Edit Sheet")) {
        if (g_EditorState.num_selections == 0) {
            UI_Text("No face selected.");
            UI_End();
            return;
        }

        bool all_are_brush_faces = true;
        for (int i = 0; i < g_EditorState.num_selections; ++i) {
            if (g_EditorState.selections[i].type != ENTITY_BRUSH || g_EditorState.selections[i].face_index == -1) {
                all_are_brush_faces = false;
                break;
            }
        }

        if (!all_are_brush_faces) {
            UI_Text("Selection must contain only brush faces.");
            UI_End();
            return;
        }

        EditorSelection* primary = Editor_GetPrimarySelection();
        Brush* primary_brush = &scene->brushes[primary->index];
        BrushFace* primary_face = &primary_brush->faces[primary->face_index];

        static int selected_material_layer = 0;

        if (UI_BeginTabBar("FaceEditTabs", 0)) {
            if (UI_BeginTabItem("Material")) {
                UI_Text("Texture Layer");
                UI_RadioButton_Int("Base", &selected_material_layer, 0); UI_SameLine();
                UI_RadioButton_Int("Blend R", &selected_material_layer, 1); UI_SameLine();
                UI_RadioButton_Int("Blend G", &selected_material_layer, 2); UI_SameLine();
                UI_RadioButton_Int("Blend B", &selected_material_layer, 3);
                UI_Separator();

                Material* target_material = NULL;
                Vec2* target_scale = NULL;
                Vec2* target_offset = NULL;
                float* target_rotation = NULL;

                switch (selected_material_layer) {
                case 0:
                    target_material = primary_face->material;
                    target_scale = &primary_face->uv_scale;
                    target_offset = &primary_face->uv_offset;
                    target_rotation = &primary_face->uv_rotation;
                    break;
                case 1:
                    target_material = primary_face->material2;
                    target_scale = &primary_face->uv_scale2;
                    target_offset = &primary_face->uv_offset2;
                    target_rotation = &primary_face->uv_rotation2;
                    break;
                case 2:
                    target_material = primary_face->material3;
                    target_scale = &primary_face->uv_scale3;
                    target_offset = &primary_face->uv_offset3;
                    target_rotation = &primary_face->uv_rotation3;
                    break;
                case 3:
                    target_material = primary_face->material4;
                    target_scale = &primary_face->uv_scale4;
                    target_offset = &primary_face->uv_offset4;
                    target_rotation = &primary_face->uv_rotation4;
                    break;
                }

                UI_Image((void*)(intptr_t)(target_material ? target_material->diffuseMap : missingTextureID), 128, 128);
                UI_SameLine();
                UI_BeginGroup();
                UI_Text("Current Texture:");
                UI_TextWrapped(target_material ? target_material->name : "None");
                if (UI_Button("Browse...")) {
                    g_EditorState.texture_browser_target = selected_material_layer;
                    g_EditorState.show_texture_browser = true;
                }
                if (UI_Button("Apply to All Faces")) {
                    Undo_BeginEntityModification(scene, ENTITY_BRUSH, primary->index);

                    for (int i = 0; i < primary_brush->numFaces; ++i) {
                        BrushFace* dest_face = &primary_brush->faces[i];
                        switch (selected_material_layer) {
                        case 0:
                            dest_face->material = target_material;
                            dest_face->uv_scale = *target_scale;
                            dest_face->uv_offset = *target_offset;
                            dest_face->uv_rotation = *target_rotation;
                            break;
                        case 1:
                            dest_face->material2 = target_material;
                            dest_face->uv_scale2 = *target_scale;
                            dest_face->uv_offset2 = *target_offset;
                            dest_face->uv_rotation2 = *target_rotation;
                            break;
                        case 2:
                            dest_face->material3 = target_material;
                            dest_face->uv_scale3 = *target_scale;
                            dest_face->uv_offset3 = *target_offset;
                            dest_face->uv_rotation3 = *target_rotation;
                            break;
                        case 3:
                            dest_face->material4 = target_material;
                            dest_face->uv_scale4 = *target_scale;
                            dest_face->uv_offset4 = *target_offset;
                            dest_face->uv_rotation4 = *target_rotation;
                            break;
                        }
                    }
                    Brush_CreateRenderData(primary_brush);
                    Undo_EndEntityModification(scene, ENTITY_BRUSH, primary->index, "Apply Material to All Faces");
                }
                UI_EndGroup();
                UI_Separator();

                if (target_scale && target_offset && target_rotation) {
                    UI_Text("Texture Scale"); UI_SameLine(); UI_SetNextItemWidth(80);
                    if (UI_InputFloat("X##Scale", &target_scale->x, 0.01f, 0.1f, "%.2f")) {
                        for (int i = 0; i < g_EditorState.num_selections; ++i) {
                            EditorSelection* sel = &g_EditorState.selections[i];
                            BrushFace* face = &scene->brushes[sel->index].faces[sel->face_index];
                            switch (selected_material_layer) {
                            case 0: face->uv_scale.x = target_scale->x; break;
                            case 1: face->uv_scale2.x = target_scale->x; break;
                            case 2: face->uv_scale3.x = target_scale->x; break;
                            case 3: face->uv_scale4.x = target_scale->x; break;
                            }
                            Brush_CreateRenderData(&scene->brushes[sel->index]);
                        }
                    }
                    if (UI_IsItemActivated()) { Undo_BeginMultiEntityModification(scene, g_EditorState.selections, g_EditorState.num_selections); }
                    if (UI_IsItemDeactivatedAfterEdit()) { Undo_EndMultiEntityModification(scene, g_EditorState.selections, g_EditorState.num_selections, "Edit Face UVs"); }

                    UI_SameLine(); UI_SetNextItemWidth(80);
                    if (UI_InputFloat("Y##Scale", &target_scale->y, 0.01f, 0.1f, "%.2f")) {
                        for (int i = 0; i < g_EditorState.num_selections; ++i) {
                            EditorSelection* sel = &g_EditorState.selections[i];
                            BrushFace* face = &scene->brushes[sel->index].faces[sel->face_index];
                            switch (selected_material_layer) {
                            case 0: face->uv_scale.y = target_scale->y; break;
                            case 1: face->uv_scale2.y = target_scale->y; break;
                            case 2: face->uv_scale3.y = target_scale->y; break;
                            case 3: face->uv_scale4.y = target_scale->y; break;
                            }
                            Brush_CreateRenderData(&scene->brushes[sel->index]);
                        }
                    }
                    if (UI_IsItemActivated()) { Undo_BeginMultiEntityModification(scene, g_EditorState.selections, g_EditorState.num_selections); }
                    if (UI_IsItemDeactivatedAfterEdit()) { Undo_EndMultiEntityModification(scene, g_EditorState.selections, g_EditorState.num_selections, "Edit Face UVs"); }

                    UI_Text("Texture Shift"); UI_SameLine(); UI_SetNextItemWidth(80);
                    if (UI_InputFloat("X##Shift", &target_offset->x, 0.1f, 1.0f, "%.2f")) {
                        for (int i = 0; i < g_EditorState.num_selections; ++i) {
                            EditorSelection* sel = &g_EditorState.selections[i];
                            BrushFace* face = &scene->brushes[sel->index].faces[sel->face_index];
                            switch (selected_material_layer) {
                            case 0: face->uv_offset.x = target_offset->x; break;
                            case 1: face->uv_offset2.x = target_offset->x; break;
                            case 2: face->uv_offset3.x = target_offset->x; break;
                            case 3: face->uv_offset4.x = target_offset->x; break;
                            }
                            Brush_CreateRenderData(&scene->brushes[sel->index]);
                        }
                    }
                    if (UI_IsItemActivated()) { Undo_BeginMultiEntityModification(scene, g_EditorState.selections, g_EditorState.num_selections); }
                    if (UI_IsItemDeactivatedAfterEdit()) { Undo_EndMultiEntityModification(scene, g_EditorState.selections, g_EditorState.num_selections, "Edit Face UVs"); }

                    UI_SameLine(); UI_SetNextItemWidth(80);
                    if (UI_InputFloat("Y##Shift", &target_offset->y, 0.1f, 1.0f, "%.2f")) {
                        for (int i = 0; i < g_EditorState.num_selections; ++i) {
                            EditorSelection* sel = &g_EditorState.selections[i];
                            BrushFace* face = &scene->brushes[sel->index].faces[sel->face_index];
                            switch (selected_material_layer) {
                            case 0: face->uv_offset.y = target_offset->y; break;
                            case 1: face->uv_offset2.y = target_offset->y; break;
                            case 2: face->uv_offset3.y = target_offset->y; break;
                            case 3: face->uv_offset4.y = target_offset->y; break;
                            }
                            Brush_CreateRenderData(&scene->brushes[sel->index]);
                        }
                    }
                    if (UI_IsItemActivated()) { Undo_BeginMultiEntityModification(scene, g_EditorState.selections, g_EditorState.num_selections); }
                    if (UI_IsItemDeactivatedAfterEdit()) { Undo_EndMultiEntityModification(scene, g_EditorState.selections, g_EditorState.num_selections, "Edit Face UVs"); }

                    UI_Text("Rotation"); UI_SameLine(); UI_SetNextItemWidth(172);
                    if (UI_DragFloat("##Rotation", target_rotation, 1.0f, -360.0f, 360.0f)) {
                        for (int i = 0; i < g_EditorState.num_selections; ++i) {
                            EditorSelection* sel = &g_EditorState.selections[i];
                            BrushFace* face = &scene->brushes[sel->index].faces[sel->face_index];
                            switch (selected_material_layer) {
                            case 0: face->uv_rotation = *target_rotation; break;
                            case 1: face->uv_rotation2 = *target_rotation; break;
                            case 2: face->uv_rotation3 = *target_rotation; break;
                            case 3: face->uv_rotation4 = *target_rotation; break;
                            }
                            Brush_CreateRenderData(&scene->brushes[sel->index]);
                        }
                    }
                    if (UI_IsItemActivated()) { Undo_BeginMultiEntityModification(scene, g_EditorState.selections, g_EditorState.num_selections); }
                    if (UI_IsItemDeactivatedAfterEdit()) { Undo_EndMultiEntityModification(scene, g_EditorState.selections, g_EditorState.num_selections, "Edit Face UVs"); }
                }

                UI_Separator();
                UI_Text("Justify");
                if (UI_Button("L")) { for (int i = 0; i < g_EditorState.num_selections; ++i) { EditorSelection* sel = &g_EditorState.selections[i]; Undo_BeginEntityModification(scene, ENTITY_BRUSH, sel->index); scene->brushes[sel->index].faces[sel->face_index].uv_offset.x = 0; Brush_CreateRenderData(&scene->brushes[sel->index]); Undo_EndEntityModification(scene, ENTITY_BRUSH, sel->index, "Justify UV"); } } UI_SameLine();
                if (UI_Button("R")) { for (int i = 0; i < g_EditorState.num_selections; ++i) { EditorSelection* sel = &g_EditorState.selections[i]; Undo_BeginEntityModification(scene, ENTITY_BRUSH, sel->index); BrushFace* f = &scene->brushes[sel->index].faces[sel->face_index]; f->uv_offset.x = 1.0f - (f->uv_scale.x > 0 ? fmodf(1.0f, f->uv_scale.x) : 0); Brush_CreateRenderData(&scene->brushes[sel->index]); Undo_EndEntityModification(scene, ENTITY_BRUSH, sel->index, "Justify UV"); } } UI_SameLine();
                if (UI_Button("T")) { for (int i = 0; i < g_EditorState.num_selections; ++i) { EditorSelection* sel = &g_EditorState.selections[i]; Undo_BeginEntityModification(scene, ENTITY_BRUSH, sel->index); scene->brushes[sel->index].faces[sel->face_index].uv_offset.y = 0; Brush_CreateRenderData(&scene->brushes[sel->index]); Undo_EndEntityModification(scene, ENTITY_BRUSH, sel->index, "Justify UV"); } } UI_SameLine();
                if (UI_Button("B")) { for (int i = 0; i < g_EditorState.num_selections; ++i) { EditorSelection* sel = &g_EditorState.selections[i]; Undo_BeginEntityModification(scene, ENTITY_BRUSH, sel->index); BrushFace* f = &scene->brushes[sel->index].faces[sel->face_index]; f->uv_offset.y = 1.0f - (f->uv_scale.y > 0 ? fmodf(1.0f, f->uv_scale.y) : 0); Brush_CreateRenderData(&scene->brushes[sel->index]); Undo_EndEntityModification(scene, ENTITY_BRUSH, sel->index, "Justify UV"); } } UI_SameLine();
                if (UI_Button("C")) { for (int i = 0; i < g_EditorState.num_selections; ++i) { EditorSelection* sel = &g_EditorState.selections[i]; Undo_BeginEntityModification(scene, ENTITY_BRUSH, sel->index); BrushFace* f = &scene->brushes[sel->index].faces[sel->face_index]; f->uv_offset.x = 0.5f - (f->uv_scale.x / 2.0f); f->uv_offset.y = 0.5f - (f->uv_scale.y / 2.0f); Brush_CreateRenderData(&scene->brushes[sel->index]); Undo_EndEntityModification(scene, ENTITY_BRUSH, sel->index, "Justify UV"); } } UI_SameLine();
                if (UI_Button("Fit")) {
                    for (int i = 0; i < g_EditorState.num_selections; ++i) {
                        EditorSelection* sel = &g_EditorState.selections[i];
                        Brush* b = &scene->brushes[sel->index];
                        BrushFace* face = &b->faces[sel->face_index];
                        if (face->numVertexIndices >= 3) {
                            Undo_BeginEntityModification(scene, ENTITY_BRUSH, sel->index);
                            Vec3 p0 = b->vertices[face->vertexIndices[0]].pos;
                            Vec3 p1 = b->vertices[face->vertexIndices[1]].pos;
                            Vec3 p2 = b->vertices[face->vertexIndices[2]].pos;
                            Vec3 face_normal = vec3_cross(vec3_sub(p1, p0), vec3_sub(p2, p0));
                            vec3_normalize(&face_normal);
                            Vec3 u_axis = vec3_sub(p1, p0);
                            vec3_normalize(&u_axis);
                            Vec3 v_axis = vec3_cross(face_normal, u_axis);
                            float min_u = FLT_MAX, max_u = -FLT_MAX;
                            float min_v = FLT_MAX, max_v = -FLT_MAX;
                            for (int j = 0; j < face->numVertexIndices; ++j) {
                                Vec3 vert_pos = b->vertices[face->vertexIndices[j]].pos;
                                float u = vec3_dot(vert_pos, u_axis);
                                float v = vec3_dot(vert_pos, v_axis);
                                if (u < min_u) min_u = u; if (u > max_u) max_u = u;
                                if (v < min_v) min_v = v; if (v > max_v) max_v = v;
                            }
                            float u_range = max_u - min_u;
                            float v_range = max_v - min_v;
                            if (u_range > 1e-6 && v_range > 1e-6) {
                                face->uv_scale.x = u_range;
                                face->uv_scale.y = v_range;
                                face->uv_offset.x = -min_u / u_range;
                                face->uv_offset.y = -min_v / v_range;
                                face->uv_rotation = 0;
                            }
                            Brush_CreateRenderData(b);
                            Undo_EndEntityModification(scene, ENTITY_BRUSH, sel->index, "Fit Texture to Face");
                        }
                    }
                }

                UI_Separator();
                UI_Text("Lighting");
                if (UI_DragFloat("Lightmap Scale", &primary_face->lightmap_scale, 0.125f, 0.125f, 16.0f)) {}
                if (UI_IsItemDeactivatedAfterEdit()) {
                    for (int i = 0; i < g_EditorState.num_selections; ++i) {
                        EditorSelection* sel = &g_EditorState.selections[i];
                        Undo_BeginEntityModification(scene, ENTITY_BRUSH, sel->index);
                        scene->brushes[sel->index].faces[sel->face_index].lightmap_scale = primary_face->lightmap_scale;
                        Undo_EndEntityModification(scene, ENTITY_BRUSH, sel->index, "Edit Lightmap Scale");
                    }
                }

                UI_EndTabItem();
            }

            if (UI_BeginTabItem("Properties")) {
                UI_Text("Geometry Tools");
                if (UI_Button("Flip Face Normal")) {
                    for (int i = 0; i < g_EditorState.num_selections; ++i) {
                        EditorSelection* sel = &g_EditorState.selections[i];
                        Brush* b = &scene->brushes[sel->index];
                        Undo_BeginEntityModification(scene, ENTITY_BRUSH, sel->index);
                        BrushFace* face_to_flip = &b->faces[sel->face_index];
                        int num_indices = face_to_flip->numVertexIndices;
                        for (int k = 0; k < num_indices / 2; ++k) {
                            int temp = face_to_flip->vertexIndices[k];
                            face_to_flip->vertexIndices[k] = face_to_flip->vertexIndices[num_indices - 1 - k];
                            face_to_flip->vertexIndices[num_indices - 1 - k] = temp;
                        }
                        Brush_CreateRenderData(b);
                        Undo_EndEntityModification(scene, ENTITY_BRUSH, sel->index, "Flip Brush Face");
                    }
                }
                UI_SameLine();
                if (UI_Button("Delete Face")) {
                    for (int i = g_EditorState.num_selections - 1; i >= 0; --i) {
                        EditorSelection* sel = &g_EditorState.selections[i];
                        Brush* b = &scene->brushes[sel->index];
                        Undo_BeginEntityModification(scene, ENTITY_BRUSH, sel->index);
                        free(b->faces[sel->face_index].vertexIndices);
                        for (int j = sel->face_index; j < b->numFaces - 1; ++j) {
                            b->faces[j] = b->faces[j + 1];
                        }
                        b->numFaces--;
                        Brush_CreateRenderData(b);
                        Undo_EndEntityModification(scene, ENTITY_BRUSH, sel->index, "Delete Face");
                    }
                    Editor_ClearSelection();
                }

                static int subdivide_u = 2, subdivide_v = 2;
                UI_DragInt("Subdivisions U", &subdivide_u, 1, 1, 16);
                UI_DragInt("Subdivisions V", &subdivide_v, 1, 1, 16);
                if (UI_Button("Subdivide Selected Faces")) {
                    for (int i = 0; i < g_EditorState.num_selections; ++i) {
                        EditorSelection* sel = &g_EditorState.selections[i];
                        Editor_SubdivideBrushFace(scene, engine, sel->index, sel->face_index, subdivide_u, subdivide_v);
                    }
                    Editor_ClearSelection();
                }
                UI_Separator();
                UI_Text("Utility");
                if (UI_Button("Apply Nodraw")) {
                    for (int i = 0; i < g_EditorState.num_selections; ++i) {
                        EditorSelection* sel = &g_EditorState.selections[i];
                        Brush* b = &scene->brushes[sel->index];
                        BrushFace* face = &b->faces[sel->face_index];
                        Undo_BeginEntityModification(scene, ENTITY_BRUSH, sel->index);
                        face->material = &g_NodrawMaterial;
                        Brush_CreateRenderData(b);
                        Undo_EndEntityModification(scene, ENTITY_BRUSH, sel->index, "Apply Nodraw");
                    }
                }
                UI_SameLine();
                if (UI_Button("Copy Props")) {
                    memcpy(&g_copiedFaceProperties, primary_face, sizeof(BrushFace));
                    g_copiedFaceProperties.vertexIndices = NULL;
                    g_copiedFaceProperties.numVertexIndices = 0;
                    g_hasCopiedFace = true;
                }
                UI_SameLine();
                if (UI_Button("Paste Props") && g_hasCopiedFace) {
                    for (int i = 0; i < g_EditorState.num_selections; ++i) {
                        EditorSelection* sel = &g_EditorState.selections[i];
                        Brush* b = &scene->brushes[sel->index];
                        BrushFace* face = &b->faces[sel->face_index];
                        Undo_BeginEntityModification(scene, ENTITY_BRUSH, sel->index);
                        face->material = g_copiedFaceProperties.material;
                        face->material2 = g_copiedFaceProperties.material2;
                        face->material3 = g_copiedFaceProperties.material3;
                        face->material4 = g_copiedFaceProperties.material4;
                        face->uv_offset = g_copiedFaceProperties.uv_offset;
                        face->uv_scale = g_copiedFaceProperties.uv_scale;
                        face->uv_rotation = g_copiedFaceProperties.uv_rotation;
                        face->lightmap_scale = g_copiedFaceProperties.lightmap_scale;
                        Brush_CreateRenderData(b);
                        Undo_EndEntityModification(scene, ENTITY_BRUSH, sel->index, "Paste Face Properties");
                    }
                }
                UI_EndTabItem();
            }
            UI_EndTabBar();
        }
    }
    UI_End();
}

void Editor_RenderSprinkleToolWindow(void) {
    if (!g_EditorState.show_sprinkle_tool_window) {
        return;
    }

    UI_SetNextWindowSize(300, 0);
    if (UI_Begin("Sprinkle Tool", &g_EditorState.show_sprinkle_tool_window)) {
        UI_Text("Entity to Sprinkle");
        char model_button_label[256];
        sprintf(model_button_label, "Model: %s", g_EditorState.sprinkle_model_path);
        if (UI_Button(model_button_label)) {
            g_EditorState.texture_browser_target = MODEL_BROWSER_TARGET_SPRINKLE;
            g_EditorState.show_add_model_popup = true;
            ScanModelFiles();
        }

        UI_Separator();
        UI_Text("Brush Settings");
        UI_DragFloat("Radius", &g_EditorState.sprinkle_radius, 0.1f, 0.1f, 50.0f);
        UI_DragFloat("Density (obj/sec)", &g_EditorState.sprinkle_density, 0.1f, 0.1f, 100.0f);

        UI_Separator();
        UI_Text("Placement Settings");
        UI_Checkbox("Align to Surface Normal", &g_EditorState.sprinkle_align_to_normal);
        UI_Checkbox("Randomize Yaw", &g_EditorState.sprinkle_random_yaw);
        UI_DragFloat("Min Scale", &g_EditorState.sprinkle_scale_min, 0.01f, 0.1f, 10.0f);
        UI_DragFloat("Max Scale", &g_EditorState.sprinkle_scale_max, 0.01f, 0.1f, 10.0f);

        UI_Separator();
        UI_Text("Mode");
        UI_RadioButton_Int("Additive", &g_EditorState.sprinkle_mode, 0);
        UI_SameLine();
        UI_RadioButton_Int("Subtractive", &g_EditorState.sprinkle_mode, 1);
    }
    UI_End();
}

void Editor_RenderBakeLightingWindow(Scene* scene, Engine* engine) {
    if (g_EditorState.show_bake_lighting_popup) {
        UI_Begin("Bake Lighting", &g_EditorState.show_bake_lighting_popup);
        UI_Text("Baking will save the current map file first.");
        UI_Separator();

        const char* resolutions[] = { "16", "32", "64", "128", "256", "512" };
        UI_Combo("Resolution", &g_EditorState.bake_resolution, resolutions, 6, -1);

        UI_DragInt("Bounces", &g_EditorState.bake_bounces, 1, 0, 4);

        UI_Separator();

        if (UI_Button("Bake")) {
            Scene_SaveMap(scene, NULL, g_EditorState.currentMapPath);

            int resolution_values[] = { 16, 32, 64, 128, 256, 512 };
            int resolution = resolution_values[g_EditorState.bake_resolution];

            Lightmapper_Generate(scene, engine, resolution, g_EditorState.bake_bounces);

            char map_name_sanitized[128];
            const char* last_slash = strrchr(scene->mapPath, '/');
            const char* last_bslash = strrchr(scene->mapPath, '\\');
            const char* map_filename = (last_slash > last_bslash) ? last_slash + 1 : (last_bslash ? last_bslash + 1 : scene->mapPath);
            const char* dot = strrchr(map_filename, '.');
            if (dot) {
                size_t len = dot - map_filename;
                strncpy(map_name_sanitized, map_filename, len);
                map_name_sanitized[len] = '\0';
            }
            else {
                strcpy(map_name_sanitized, map_filename);
            }

            for (int i = 0; i < scene->numBrushes; ++i) {
                Brush* b = &scene->brushes[i];
                if (b->useVertexLighting) {
                    if (b->bakedVertexColors) { free(b->bakedVertexColors); b->bakedVertexColors = NULL; }
                    if (b->bakedVertexDirections) { free(b->bakedVertexDirections); b->bakedVertexDirections = NULL; }
                    Brush_LoadVertexLighting(b, i, scene->mapPath);
                    Brush_LoadVertexDirectionalLighting(b, i, scene->mapPath);
                }
                else {
                    if (b->lightmapAtlas != 0) { glDeleteTextures(1, &b->lightmapAtlas); b->lightmapAtlas = 0; }
                    if (b->directionalLightmapAtlas != 0) { glDeleteTextures(1, &b->directionalLightmapAtlas); b->directionalLightmapAtlas = 0; }
                    Brush_GenerateLightmapAtlas(b, map_name_sanitized, i, scene->lightmapResolution);
                }
                Brush_CreateRenderData(b);
            }

            for (int i = 0; i < scene->numDecals; ++i) {
                Decal* d = &scene->decals[i];
                if (d->lightmapAtlas != 0) { glDeleteTextures(1, &d->lightmapAtlas); d->lightmapAtlas = 0; }
                if (d->directionalLightmapAtlas != 0) { glDeleteTextures(1, &d->directionalLightmapAtlas); d->directionalLightmapAtlas = 0; }
                Decal_LoadLightmaps(d, map_name_sanitized, i);
            }

            for (int i = 0; i < scene->numObjects; ++i) {
                SceneObject* obj = &scene->objects[i];
                if (obj->bakedVertexColors) {
                    free(obj->bakedVertexColors);
                    obj->bakedVertexColors = NULL;
                }
                if (obj->bakedVertexDirections) {
                    free(obj->bakedVertexDirections);
                    obj->bakedVertexDirections = NULL;
                }
                SceneObject_LoadVertexLighting(obj, i, scene->mapPath);
                SceneObject_LoadVertexDirectionalLighting(obj, i, scene->mapPath);
            }

            Scene_LoadAmbientProbes(scene);

            scene->static_shadows_generated = true;
            Console_Printf("Lightmap reload complete.");

            g_EditorState.show_bake_lighting_popup = false;
        }
        UI_SameLine();
        if (UI_Button("Cancel")) {
            g_EditorState.show_bake_lighting_popup = false;
        }
        UI_End();
    }
}

void Editor_RenderBuildCubemapsWindow(Renderer* renderer, Scene* scene, Engine* engine) {
    if (!g_EditorState.show_build_cubemaps_popup) {
        return;
    }

    UI_Begin("Build Environment probes", &g_EditorState.show_build_cubemaps_popup);
    UI_Text("Building Environment probes will re-render reflections for all probes.");
    UI_Separator();

    const char* resolutions[] = { "64", "128", "256", "512", "1024" };
    UI_Combo("Resolution", &g_EditorState.cubemap_resolution_index, resolutions, 5, -1);

    UI_Separator();

    if (UI_Button("Build")) {
        int resolution_values[] = { 64, 128, 256, 512, 1024 };
        int resolution = resolution_values[g_EditorState.cubemap_resolution_index];

        MiscRender_BuildCubemaps(renderer, scene, engine, resolution);

        for (int i = 0; i < scene->numBrushes; ++i) {
            Brush* b = &scene->brushes[i];
            if (strcmp(b->classname, "env_reflectionprobe") == 0) {
                char map_name_sanitized[128];
                const char* last_slash = strrchr(scene->mapPath, '/');
                const char* last_bslash = strrchr(scene->mapPath, '\\');
                const char* map_filename = (last_slash > last_bslash) ? last_slash + 1 : (last_bslash ? last_bslash + 1 : scene->mapPath);
                const char* dot_ptr = strrchr(map_filename, '.');
                if (dot_ptr) {
                    size_t len = dot_ptr - map_filename;
                    strncpy(map_name_sanitized, map_filename, len);
                    map_name_sanitized[len] = '\0';
                }
                else {
                    strcpy(map_name_sanitized, map_filename);
                }
                const char* suffixes[] = { "_px.png", "_nx.png", "_py.png", "_ny.png", "_pz.png", "_nz.png" };
                char face_paths[6][256];
                const char* face_pointers[6];
                for (int k = 0; k < 6; ++k) {
                    sprintf(face_paths[k], "cubemaps/%s/%s_%s.png", map_name_sanitized, b->name, suffixes[k]);
                    face_pointers[k] = face_paths[k];
                }
                b->cubemapTexture = TextureManager_ReloadCubemap(face_pointers, b->cubemapTexture);
            }
        }

        g_EditorState.show_build_cubemaps_popup = false;
    }
    UI_SameLine();
    if (UI_Button("Cancel")) {
        g_EditorState.show_build_cubemaps_popup = false;
    }
    UI_End();
}

void Editor_RenderArchPropertiesWindow(Scene* scene, Engine* engine) {
    if (!g_EditorState.show_arch_properties_popup) return;

    g_EditorState.is_in_brush_creation_mode = true;

    UI_SetNextWindowSize(370, 330);
    UI_Begin("Arch Properties", &g_EditorState.show_arch_properties_popup);

    Editor_UpdatePreviewBrushForArch();

    bool values_changed = false;
    values_changed |= UI_DragFloat("Wall width", &g_EditorState.arch_wall_width, 0.1f, 0.01f, 1024.0f);
    values_changed |= UI_DragInt("Number of Sides", &g_EditorState.arch_num_sides, 1, 3, 64);
    if (UI_Button("Circle")) { g_EditorState.arch_arc_degrees = 360.0f; values_changed = true; } UI_SameLine();
    values_changed |= UI_DragFloat("Arc", &g_EditorState.arch_arc_degrees, 1.0f, 1.0f, 360.0f);
    values_changed |= UI_DragFloat("Start Angle", &g_EditorState.arch_start_angle_degrees, 1.0f, -360.0f, 360.0f);
    values_changed |= UI_DragFloat("Add Height", &g_EditorState.arch_add_height, 1.0f, 0.0f, 4096.0f);

    if (values_changed) {
        Editor_UpdatePreviewBrushForArch();
    }

    Editor_RenderArchPreview();
    UI_Image((void*)(intptr_t)g_EditorState.arch_preview_texture, g_EditorState.arch_preview_width, g_EditorState.arch_preview_height);

    if (UI_Button("OK")) {
        Editor_CreateBrushFromPreview(scene, engine, &g_EditorState.preview_brush);
        g_EditorState.is_in_brush_creation_mode = false;
        g_EditorState.show_arch_properties_popup = false;
    }
    UI_SameLine();
    if (UI_Button("Cancel")) {
        Brush_FreeData(&g_EditorState.preview_brush);
        g_EditorState.is_in_brush_creation_mode = false;
        g_EditorState.show_arch_properties_popup = false;
    }

    if (!g_EditorState.show_arch_properties_popup) {
        Brush_FreeData(&g_EditorState.preview_brush);
        g_EditorState.is_in_brush_creation_mode = false;
    }

    UI_End();
}

void Editor_RenderMapInfoWindow(Scene* scene) {
    if (!g_EditorState.show_map_info_window) {
        return;
    }

    UI_SetNextWindowSize(250, 180);
    if (UI_Begin("Map Information", &g_EditorState.show_map_info_window)) {
        int solid_count = scene->numBrushes;
        int face_count = 0;
        for (int i = 0; i < scene->numBrushes; ++i) {
            face_count += scene->brushes[i].numFaces;
        }
        int entity_count = scene->numObjects + scene->numActiveLights + scene->numDecals +
            scene->numSoundEntities + scene->numParticleEmitters + scene->numSprites +
            scene->numVideoPlayers + scene->numParallaxRooms + scene->numLogicEntities;

        Material* unique_materials[MAX_MATERIALS];
        int unique_count = 0;

        for (int i = 0; i < scene->numBrushes; ++i) {
            for (int j = 0; j < scene->brushes[i].numFaces; ++j) {
                Material* mats[] = { scene->brushes[i].faces[j].material, scene->brushes[i].faces[j].material2, scene->brushes[i].faces[j].material3, scene->brushes[i].faces[j].material4 };
                for (int k = 0; k < 4; ++k) {
                    if (mats[k] != NULL && mats[k] != &g_NodrawMaterial) {
                        bool found = false;
                        for (int l = 0; l < unique_count; ++l) {
                            if (unique_materials[l] == mats[k]) {
                                found = true;
                                break;
                            }
                        }
                        if (!found && unique_count < MAX_MATERIALS) {
                            unique_materials[unique_count++] = mats[k];
                        }
                    }
                }
            }
        }

        UI_Text("Solids: %d", solid_count);
        UI_Text("Faces: %d", face_count);
        UI_Text("Entities: %d", entity_count);
        UI_Text("Unique textures: %d", unique_count);

        UI_Separator();
        if (UI_Button("Close")) {
            g_EditorState.show_map_info_window = false;
        }
    }
    UI_End();
}

void Editor_RenderTransformWindow(Scene* scene, Engine* engine) {
    if (!g_EditorState.show_transform_window) {
        return;
    }

    UI_SetNextWindowSize(300, 220);
    if (UI_Begin("Transformation", &g_EditorState.show_transform_window)) {
        UI_BeginGroup();
        UI_Text("Mode:");
        if (UI_RadioButton_Int("Rotate", (int*)&g_EditorState.transform_window_mode, TRANSFORM_MODE_ROTATE)) {
            g_EditorState.transform_window_values = (Vec3){ 0, 0, 0 };
        }
        if (UI_RadioButton_Int("Scale", (int*)&g_EditorState.transform_window_mode, TRANSFORM_MODE_SCALE)) {
            g_EditorState.transform_window_values = (Vec3){ 1, 1, 1 };
        }
        if (UI_RadioButton_Int("Move", (int*)&g_EditorState.transform_window_mode, TRANSFORM_MODE_MOVE)) {
            g_EditorState.transform_window_values = (Vec3){ 0, 0, 0 };
        }
        UI_EndGroup();

        UI_SameLine(0, 40);

        UI_BeginGroup();
        UI_Text("Values:");
        UI_SetNextItemWidth(120);
        UI_InputFloat("X:", &g_EditorState.transform_window_values.x, 0.0f, 0.0f, "%.3f");
        UI_SetNextItemWidth(120);
        UI_InputFloat("Y:", &g_EditorState.transform_window_values.y, 0.0f, 0.0f, "%.3f");
        UI_SetNextItemWidth(120);
        UI_InputFloat("Z:", &g_EditorState.transform_window_values.z, 0.0f, 0.0f, "%.3f");
        UI_EndGroup();

        UI_Separator();

        if (UI_Button("OK")) {
            if (g_EditorState.num_selections > 0) {
                Undo_BeginMultiEntityModification(scene, g_EditorState.selections, g_EditorState.num_selections);

                for (int i = 0; i < g_EditorState.num_selections; ++i) {
                    EditorSelection* sel = &g_EditorState.selections[i];

                    switch (sel->type) {
                    case ENTITY_MODEL: {
                        SceneObject* obj = &scene->objects[sel->index];
                        if (g_EditorState.transform_window_mode == TRANSFORM_MODE_MOVE) obj->pos = vec3_add(obj->pos, g_EditorState.transform_window_values);
                        if (g_EditorState.transform_window_mode == TRANSFORM_MODE_ROTATE) obj->rot = vec3_add(obj->rot, g_EditorState.transform_window_values);
                        if (g_EditorState.transform_window_mode == TRANSFORM_MODE_SCALE) obj->scale = vec3_mul(obj->scale, g_EditorState.transform_window_values);
                        SceneObject_UpdateMatrix(obj);
                        if (obj->physicsBody) Physics_SetWorldTransform(obj->physicsBody, obj->modelMatrix);
                        break;
                    }
                    case ENTITY_BRUSH: {
                        Brush* b = &scene->brushes[sel->index];
                        if (g_EditorState.transform_window_mode == TRANSFORM_MODE_MOVE) b->pos = vec3_add(b->pos, g_EditorState.transform_window_values);
                        if (g_EditorState.transform_window_mode == TRANSFORM_MODE_ROTATE) b->rot = vec3_add(b->rot, g_EditorState.transform_window_values);
                        if (g_EditorState.transform_window_mode == TRANSFORM_MODE_SCALE) b->scale = vec3_mul(b->scale, g_EditorState.transform_window_values);
                        Brush_UpdateMatrix(b);
                        if (b->physicsBody) Physics_SetWorldTransform(b->physicsBody, b->modelMatrix);
                        break;
                    }
                    case ENTITY_LIGHT: {
                        Light* l = &scene->lights[sel->index];
                        if (g_EditorState.transform_window_mode == TRANSFORM_MODE_MOVE) l->position = vec3_add(l->position, g_EditorState.transform_window_values);
                        if (g_EditorState.transform_window_mode == TRANSFORM_MODE_ROTATE) l->rot = vec3_add(l->rot, g_EditorState.transform_window_values);
                        break;
                    }
                    case ENTITY_DECAL: {
                        Decal* d = &scene->decals[sel->index];
                        if (g_EditorState.transform_window_mode == TRANSFORM_MODE_MOVE) d->pos = vec3_add(d->pos, g_EditorState.transform_window_values);
                        if (g_EditorState.transform_window_mode == TRANSFORM_MODE_ROTATE) d->rot = vec3_add(d->rot, g_EditorState.transform_window_values);
                        if (g_EditorState.transform_window_mode == TRANSFORM_MODE_SCALE) d->size = vec3_mul(d->size, g_EditorState.transform_window_values);
                        Decal_UpdateMatrix(d);
                        break;
                    }
                    case ENTITY_SOUND: {
                        SoundEntity* s = &scene->soundEntities[sel->index];
                        if (g_EditorState.transform_window_mode == TRANSFORM_MODE_MOVE) s->pos = vec3_add(s->pos, g_EditorState.transform_window_values);
                        break;
                    }
                    case ENTITY_PARTICLE_EMITTER: {
                        ParticleEmitter* p = &scene->particleEmitters[sel->index];
                        if (g_EditorState.transform_window_mode == TRANSFORM_MODE_MOVE) p->pos = vec3_add(p->pos, g_EditorState.transform_window_values);
                        break;
                    }
                    case ENTITY_SPRITE: {
                        Sprite* s = &scene->sprites[sel->index];
                        if (g_EditorState.transform_window_mode == TRANSFORM_MODE_MOVE) s->pos = vec3_add(s->pos, g_EditorState.transform_window_values);
                        if (g_EditorState.transform_window_mode == TRANSFORM_MODE_SCALE) s->scale *= g_EditorState.transform_window_values.x;
                        break;
                    }
                    case ENTITY_VIDEO_PLAYER: {
                        VideoPlayer* vp = &scene->videoPlayers[sel->index];
                        if (g_EditorState.transform_window_mode == TRANSFORM_MODE_MOVE) vp->pos = vec3_add(vp->pos, g_EditorState.transform_window_values);
                        if (g_EditorState.transform_window_mode == TRANSFORM_MODE_ROTATE) vp->rot = vec3_add(vp->rot, g_EditorState.transform_window_values);
                        if (g_EditorState.transform_window_mode == TRANSFORM_MODE_SCALE) {
                            vp->size.x *= g_EditorState.transform_window_values.x;
                            vp->size.y *= g_EditorState.transform_window_values.y;
                        }
                        break;
                    }
                    case ENTITY_PARALLAX_ROOM: {
                        ParallaxRoom* p = &scene->parallaxRooms[sel->index];
                        if (g_EditorState.transform_window_mode == TRANSFORM_MODE_MOVE) p->pos = vec3_add(p->pos, g_EditorState.transform_window_values);
                        if (g_EditorState.transform_window_mode == TRANSFORM_MODE_ROTATE) p->rot = vec3_add(p->rot, g_EditorState.transform_window_values);
                        if (g_EditorState.transform_window_mode == TRANSFORM_MODE_SCALE) {
                            p->size.x *= g_EditorState.transform_window_values.x;
                            p->size.y *= g_EditorState.transform_window_values.y;
                            p->roomDepth *= g_EditorState.transform_window_values.z;
                        }
                        ParallaxRoom_UpdateMatrix(p);
                        break;
                    }
                    case ENTITY_LOGIC: {
                        LogicEntity* l = &scene->logicEntities[sel->index];
                        if (g_EditorState.transform_window_mode == TRANSFORM_MODE_MOVE) l->pos = vec3_add(l->pos, g_EditorState.transform_window_values);
                        if (g_EditorState.transform_window_mode == TRANSFORM_MODE_ROTATE) l->rot = vec3_add(l->rot, g_EditorState.transform_window_values);
                        break;
                    }
                    case ENTITY_PLAYERSTART: {
                        if (g_EditorState.transform_window_mode == TRANSFORM_MODE_MOVE) scene->playerStart.position = vec3_add(scene->playerStart.position, g_EditorState.transform_window_values);
                        break;
                    }
                    default:
                        break;
                    }
                }
                Undo_EndMultiEntityModification(scene, g_EditorState.selections, g_EditorState.num_selections, "Transform Selection");
            }
            g_EditorState.show_transform_window = false;
        }
        UI_SameLine();
        if (UI_Button("Cancel")) {
            g_EditorState.show_transform_window = false;
        }
    }
    UI_End();
}

void Editor_RenderGoToCoordinatesWindow(void) {
    if (!g_EditorState.show_goto_coord_window) {
        return;
    }

    UI_SetNextWindowSize(300, 150);
    if (UI_Begin("Go to coordinates", &g_EditorState.show_goto_coord_window)) {
        UI_Text("Coordinates to go to (x y z), ex:");
        UI_Text("1 2 3");

        UI_InputText_Flags("##coord_input", g_EditorState.goto_coord_input, sizeof(g_EditorState.goto_coord_input), 0);

        if (UI_Button("OK")) {
            float x, y, z;
            if (sscanf(g_EditorState.goto_coord_input, "%f %f %f", &x, &y, &z) == 3) {
                g_EditorState.editor_camera.position.x = x;
                g_EditorState.editor_camera.position.y = y;
                g_EditorState.editor_camera.position.z = z;
                g_EditorState.show_goto_coord_window = false;
            }
        }
        UI_SameLine();
        if (UI_Button("Cancel")) {
            g_EditorState.show_goto_coord_window = false;
        }
    }
    UI_End();
}

void Editor_RenderStatusBar()
{
    const float STATUS_BAR_HEIGHT = 22.0f;
    float screen_w, screen_h;
    UI_GetDisplaySize(&screen_w, &screen_h);

    UI_SetNextWindowPos(0, screen_h - STATUS_BAR_HEIGHT);
    UI_SetNextWindowSize(screen_w, STATUS_BAR_HEIGHT);

    UI_Begin_NoTitlebar_NoResize_NoMove("Status Bar", NULL);

    UI_Text("For Help, press F1");
    UI_SameLine(0, 20.0f);
    UI_SeparatorEx(1 << 1);
    UI_SameLine(0, 20.0f);

    if (g_EditorState.num_selections > 0) {
        char selection_text[128];
        if (g_EditorState.num_selections == 1) {
            EditorSelection* sel = Editor_GetPrimarySelection();
            const char* type_name = "Object";
            switch (sel->type) {
            case ENTITY_BRUSH: type_name = "Brush"; break;
            case ENTITY_MODEL: type_name = "Model"; break;
            case ENTITY_LIGHT: type_name = "Light"; break;
            case ENTITY_DECAL: type_name = "Decal"; break;
            case ENTITY_SOUND: type_name = "Sound Entity"; break;
            case ENTITY_PARTICLE_EMITTER: type_name = "Particle Emitter"; break;
            case ENTITY_PLAYERSTART: type_name = "Player Start"; break;
            case ENTITY_SPRITE: type_name = "Sprite"; break;
            case ENTITY_VIDEO_PLAYER: type_name = "Video Player"; break;
            case ENTITY_PARALLAX_ROOM: type_name = "Parallax Room"; break;
            case ENTITY_LOGIC: type_name = "Logic Entity"; break;
            default: break;
            }
            sprintf(selection_text, "1 %s selected.", type_name);
        }
        else {
            sprintf(selection_text, "%d objects selected.", g_EditorState.num_selections);
        }
        UI_Text(selection_text);
    }
    else {
        UI_Text("no selection.");
    }
    UI_SameLine(0, 20.0f);
    UI_SeparatorEx(1 << 1);
    UI_SameLine(0, 20.0f);

    ViewportType active_2d_view = VIEW_COUNT;
    for (int i = VIEW_TOP_XZ; i <= VIEW_SIDE_YZ; ++i) {
        if (g_EditorState.is_viewport_hovered[i]) {
            active_2d_view = (ViewportType)i;
            break;
        }
    }
    if (active_2d_view == VIEW_COUNT) {
        active_2d_view = g_EditorState.last_active_2d_view;
    }

    if (active_2d_view >= VIEW_TOP_XZ && active_2d_view <= VIEW_SIDE_YZ) {
        Vec3 mouse_world = ScreenToWorld_Unsnapped_ForOrthoPicking(g_EditorState.mouse_pos_in_viewport[active_2d_view], active_2d_view);
        switch (active_2d_view) {
        case VIEW_TOP_XZ: UI_Text("@%.0f, %.0f", mouse_world.x, mouse_world.z); break;
        case VIEW_FRONT_XY: UI_Text("@%.0f, %.0f", mouse_world.x, mouse_world.y); break;
        case VIEW_SIDE_YZ: UI_Text("@%.0f, %.0f", mouse_world.z, mouse_world.y); break;
        default: break;
        }
    }

    float right_align_pos = screen_w - 400.0f;
    UI_SameLine(right_align_pos, 0);

    float zoom_level = 0.0f;
    int hovered_2d_view_index = -1;
    for (int i = VIEW_TOP_XZ; i <= VIEW_SIDE_YZ; ++i) {
        if (g_EditorState.is_viewport_hovered[i]) {
            hovered_2d_view_index = i;
            break;
        }
    }

    if (hovered_2d_view_index != -1) {
        zoom_level = g_EditorState.ortho_cam_zoom[hovered_2d_view_index - 1];
    }
    else {
        zoom_level = g_EditorState.ortho_cam_zoom[g_EditorState.last_active_2d_view - 1];
    }

    UI_Text("Zoom: %.2f", zoom_level);
    UI_SameLine(0, 20.0f);
    UI_SeparatorEx(1 << 1);
    UI_SameLine(0, 20.0f);
    UI_Text("Speed: %.1f", g_EditorState.editor_camera_speed);
    UI_SameLine(0, 20.0f);
    UI_SeparatorEx(1 << 1);
    UI_SameLine(0, 20.0f);

    UI_Text("Snap: %s", g_EditorState.snap_to_grid ? "On" : "Off");
    UI_SameLine(0, 20.0f);
    UI_SeparatorEx(1 << 1);
    UI_SameLine(0, 20.0f);

    UI_Text("Grid: %g", g_EditorState.grid_size);
    UI_SameLine(0, 20.0f);

    UI_End();
}

void Editor_RenderArchPreview() {
    glBindFramebuffer(GL_FRAMEBUFFER, g_EditorState.arch_preview_fbo);
    glViewport(0, 0, g_EditorState.arch_preview_width, g_EditorState.arch_preview_height);
    glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);

    glUseProgram(g_EditorState.debug_shader);
    Mat4 projection = mat4_ortho(0, g_EditorState.arch_preview_width, 0, g_EditorState.arch_preview_height, -1, 1);
    Mat4 view; mat4_identity(&view);
    glUniformMatrix4fv(glGetUniformLocation(g_EditorState.debug_shader, "view"), 1, GL_FALSE, view.m);
    glUniformMatrix4fv(glGetUniformLocation(g_EditorState.debug_shader, "projection"), 1, GL_FALSE, projection.m);
    Mat4 model; mat4_identity(&model);
    glUniformMatrix4fv(glGetUniformLocation(g_EditorState.debug_shader, "model"), 1, GL_FALSE, model.m);

    float world_width = 0.0f;
    if (g_EditorState.arch_creation_view == VIEW_TOP_XZ || g_EditorState.arch_creation_view == VIEW_FRONT_XY) {
        world_width = fabsf(g_EditorState.arch_creation_end_point.x - g_EditorState.arch_creation_start_point.x);
    }
    else if (g_EditorState.arch_creation_view == VIEW_SIDE_YZ) {
        world_width = fabsf(g_EditorState.arch_creation_end_point.z - g_EditorState.arch_creation_start_point.z);
    }
    float world_outer_radius = world_width / 2.0f;

    float color[] = { 1.0f, 1.0f, 1.0f, 1.0f };
    glUniform4fv(glGetUniformLocation(g_EditorState.debug_shader, "color"), 1, color);

    float center_x = g_EditorState.arch_preview_width / 2.0f;
    float center_y = 20.0f;
    float outer_radius = fminf(g_EditorState.arch_preview_width, g_EditorState.arch_preview_height) * 0.4f;
    float inner_radius = outer_radius;
    if (world_outer_radius > 0.01f) {
        float thickness_ratio = g_EditorState.arch_wall_width / world_outer_radius;
        inner_radius = outer_radius * (1.0f - thickness_ratio);
    }
    if (inner_radius < 0) inner_radius = 0;

    int num_sides = g_EditorState.arch_num_sides;
    float start_angle = g_EditorState.arch_start_angle_degrees * (M_PI / 180.0f);
    float arc = g_EditorState.arch_arc_degrees * (M_PI / 180.0f);
    float angle_step = arc / num_sides;

    Vec3* lines = malloc((num_sides * 4 + 4) * sizeof(Vec3));
    int line_idx = 0;

    for (int i = 0; i <= num_sides; ++i) {
        float angle = start_angle + i * angle_step;
        if (i > 0) {
            float prev_angle = start_angle + (i - 1) * angle_step;
            lines[line_idx++] = (Vec3){ center_x + cosf(prev_angle) * outer_radius, center_y + sinf(prev_angle) * outer_radius, 0 };
            lines[line_idx++] = (Vec3){ center_x + cosf(angle) * outer_radius, center_y + sinf(angle) * outer_radius, 0 };
            lines[line_idx++] = (Vec3){ center_x + cosf(prev_angle) * inner_radius, center_y + sinf(prev_angle) * inner_radius, 0 };
            lines[line_idx++] = (Vec3){ center_x + cosf(angle) * inner_radius, center_y + sinf(angle) * inner_radius, 0 };
        }
    }

    float start_cap_angle = start_angle;
    lines[line_idx++] = (Vec3){ center_x + cosf(start_cap_angle) * outer_radius, center_y + sinf(start_cap_angle) * outer_radius, 0 };
    lines[line_idx++] = (Vec3){ center_x + cosf(start_cap_angle) * inner_radius, center_y + sinf(start_cap_angle) * inner_radius, 0 };

    float end_cap_angle = start_angle + arc;
    lines[line_idx++] = (Vec3){ center_x + cosf(end_cap_angle) * outer_radius, center_y + sinf(end_cap_angle) * outer_radius, 0 };
    lines[line_idx++] = (Vec3){ center_x + cosf(end_cap_angle) * inner_radius, center_y + sinf(end_cap_angle) * inner_radius, 0 };

    glBindVertexArray(g_EditorState.vertex_points_vao);
    glBindBuffer(GL_ARRAY_BUFFER, g_EditorState.vertex_points_vbo);
    glBufferData(GL_ARRAY_BUFFER, line_idx * sizeof(Vec3), lines, GL_DYNAMIC_DRAW);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(Vec3), (void*)0);
    glEnableVertexAttribArray(0);
    glDrawArrays(GL_LINES, 0, line_idx);
    glBindVertexArray(0);

    free(lines);
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

// End render seperate windows