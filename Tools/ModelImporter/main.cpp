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

#include <FL/Fl.H>
#include <FL/Fl_Window.H>
#include <FL/Fl_Button.H>
#include <FL/Fl_Input.H>
#include <FL/Fl_Text_Display.H>
#include <FL/Fl_Text_Buffer.H>
#include <FL/fl_ask.H>
#include <FL/Fl_File_Chooser.H>
#include <FL/Fl_Box.H>
#include <iostream>
#include <fstream>
#include <filesystem>
#include <string>
#include <vector>
#include <cctype>
#include <cstring>
#include <thread>
#include <mutex>
#include <algorithm>

#define CGLTF_IMPLEMENTATION
#include "../../cgltf/cgltf.h"

namespace fs = std::filesystem;

struct ThreadData {
    std::string input_path;
    std::string output_path;
    std::vector<std::string>* messages;
    std::mutex* mtx;
    bool* is_running;
};

void post_message(ThreadData* data, const std::string& msg) {
    std::lock_guard<std::mutex> lock(*(data->mtx));
    data->messages->push_back(msg);
    Fl::awake();
}

void ensureDirectoryExists(const fs::path& path, ThreadData* data) {
    if (!fs::exists(path)) {
        fs::create_directories(path);
        post_message(data, "Created directory: " + path.string());
    }
}

void sanitizeForFilename(const std::string& input, std::string& output) {
    output.clear();
    for (char c : input) {
        if (std::isalnum(static_cast<unsigned char>(c)) || c == '_' || c == '-') {
            output += c;
        }
        else {
            output += '_';
        }
    }
}

bool saveImageData(cgltf_image* image, const std::string& base_filename, const std::string& suffix, const fs::path& textures_path, std::string& out_texture_path, ThreadData* data) {
    if (!image) return false;
    if (image->buffer_view) {
        std::string extension = ".png";
        if (image->mime_type && strcmp(image->mime_type, "image/jpeg") == 0) {
            extension = ".jpg";
        }
        out_texture_path = base_filename + "_" + suffix + extension;
        fs::path full_output_path = textures_path / out_texture_path;
        std::ofstream ofs(full_output_path, std::ios::binary);
        if (!ofs) {
            post_message(data, "ERROR: Could not write: " + full_output_path.string());
            return false;
        }
        const char* buffer_data = static_cast<const char*>(image->buffer_view->buffer->data) + image->buffer_view->offset;
        ofs.write(buffer_data, image->buffer_view->size);
        post_message(data, "  Extracted texture: " + out_texture_path);
        return true;
    }
    if (image->uri && std::strlen(image->uri) > 0) {
        post_message(data, "  WARNING: External image '" + std::string(image->uri) + "' detected. Assuming it exists in textures folder.");
        out_texture_path = fs::path(image->uri).filename().string();
        return true;
    }
    return false;
}

void processGltf(const fs::path& gltf_path, const fs::path& output_path, ThreadData* data) {
    post_message(data, "------------------------------------------");
    post_message(data, "Processing file: " + gltf_path.filename().string());

    std::string gltf_basename = gltf_path.stem().string();
    cgltf_options options = {};
    cgltf_data* cdata = nullptr;
    if (cgltf_parse_file(&options, gltf_path.string().c_str(), &cdata) != cgltf_result_success) {
        post_message(data, "ERROR: Could not parse GLTF file: " + gltf_path.string());
        return;
    }
    if (cgltf_load_buffers(&options, cdata, gltf_path.string().c_str()) != cgltf_result_success) {
        post_message(data, "ERROR: Could not load GLTF buffers for: " + gltf_path.string());
        cgltf_free(cdata);
        return;
    }

    fs::path textures_path = output_path / "textures";
    fs::path materials_def_path = output_path / "materials.def";

    ensureDirectoryExists(textures_path, data);
    std::ofstream mat_file(materials_def_path, std::ios::app);
    if (!mat_file) {
        post_message(data, "ERROR: Could not open " + materials_def_path.string() + " for appending.");
        cgltf_free(cdata);
        return;
    }

    for (size_t i = 0; i < cdata->materials_count; ++i) {
        cgltf_material* mat = &cdata->materials[i];
        std::string mat_name_sanitized;
        std::string original_mat_name = mat->name ? mat->name : (gltf_basename + "_mat_" + std::to_string(i));
        sanitizeForFilename(original_mat_name, mat_name_sanitized);
        post_message(data, "  > Processing material: " + original_mat_name);
        mat_file << "\"" << original_mat_name << "\"\n{\n";
        std::string texture_path_buffer;

        if (mat->has_pbr_metallic_roughness && mat->pbr_metallic_roughness.base_color_texture.texture) {
            if (saveImageData(mat->pbr_metallic_roughness.base_color_texture.texture->image, mat_name_sanitized, "diffuse", textures_path, texture_path_buffer, data)) {
                mat_file << "    diffuse = \"" << texture_path_buffer << "\"\n";
            }
        }
        if (mat->normal_texture.texture) {
            if (saveImageData(mat->normal_texture.texture->image, mat_name_sanitized, "normal", textures_path, texture_path_buffer, data)) {
                mat_file << "    normal = \"" << texture_path_buffer << "\"\n";
            }
        }
        if (mat->has_pbr_metallic_roughness && mat->pbr_metallic_roughness.metallic_roughness_texture.texture) {
            if (saveImageData(mat->pbr_metallic_roughness.metallic_roughness_texture.texture->image, mat_name_sanitized, "rma", textures_path, texture_path_buffer, data)) {
                mat_file << "    rma = \"" << texture_path_buffer << "\"\n";
            }
        }
        mat_file << "}\n\n";
    }

    mat_file.close();
    cgltf_free(cdata);
}

bool isGltfFile(const fs::path& path) {
    if (!fs::is_regular_file(path)) return false;
    std::string ext = path.extension().string();
    std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);
    return ext == ".gltf" || ext == ".glb";
}

void import_thread_func(ThreadData* data) {
    fs::path input_path(data->input_path);
    fs::path output_path(data->output_path);

    if (!fs::exists(output_path)) {
        fs::create_directories(output_path);
    }

    if (fs::is_directory(input_path)) {
        bool found = false;
        for (const auto& entry : fs::directory_iterator(input_path)) {
            if (isGltfFile(entry.path())) {
                found = true;
                processGltf(entry.path(), output_path, data);
            }
        }
        if (!found) {
            post_message(data, "ERROR: No .gltf or .glb files found in directory.");
        }
    }
    else if (fs::is_regular_file(input_path)) {
        processGltf(input_path, output_path, data);
    }

    post_message(data, "======= Import Finished =======");
    *(data->is_running) = false;
    Fl::awake();
}

class ModelImporterWindow : public Fl_Window {
public:
    Fl_Input* inputPath;
    Fl_Button* browseBtn;
    Fl_Input* outputPath;
    Fl_Button* browseOutBtn;
    Fl_Button* importBtn;
    Fl_Text_Display* logDisplay;
    Fl_Text_Buffer* logBuffer;
    Fl_Box* statusBar;

    std::thread worker_thread;
    std::vector<std::string> messages;
    std::mutex mtx;
    bool is_running = false;

    ModelImporterWindow(int W, int H, const char* L = 0) : Fl_Window(W, H, L) {
        begin();
        inputPath = new Fl_Input(80, 10, 400, 25, "Input:");
        browseBtn = new Fl_Button(490, 10, 80, 25, "Browse...");
        outputPath = new Fl_Input(80, 45, 400, 25, "Output:");
        browseOutBtn = new Fl_Button(490, 45, 80, 25, "Browse...");
        importBtn = new Fl_Button(10, 80, 560, 30, "Import");
        logBuffer = new Fl_Text_Buffer();
        logDisplay = new Fl_Text_Display(10, 120, 560, 340, "");
        logDisplay->buffer(logBuffer);
        statusBar = new Fl_Box(10, 465, 560, 25, "Ready.");
        statusBar->align(FL_ALIGN_LEFT | FL_ALIGN_INSIDE);
        end();

        browseBtn->callback(on_browse_cb, this);
        browseOutBtn->callback(on_browse_output_cb, this);
        importBtn->callback(on_import_cb, this);

        Fl::add_idle(on_idle_cb, this);
    }

    ~ModelImporterWindow() {
        if (worker_thread.joinable()) {
            worker_thread.join();
        }
    }

private:
    static void on_browse_cb(Fl_Widget* w, void* data) {
        ModelImporterWindow* win = (ModelImporterWindow*)data;
        Fl_File_Chooser chooser(".", "*", Fl_File_Chooser::DIRECTORY, "Select Input Folder or File");
        chooser.show();
        while (chooser.shown()) { Fl::wait(); }
        if (chooser.value()) {
            win->inputPath->value(chooser.value());
        }
    }

    static void on_browse_output_cb(Fl_Widget* w, void* data) {
        ModelImporterWindow* win = (ModelImporterWindow*)data;
        Fl_File_Chooser chooser(".", "*", Fl_File_Chooser::DIRECTORY, "Select Output Folder");
        chooser.show();
        while (chooser.shown()) { Fl::wait(); }
        if (chooser.value()) {
            win->outputPath->value(chooser.value());
        }
    }

    static void on_import_cb(Fl_Widget* w, void* data) {
        ModelImporterWindow* win = (ModelImporterWindow*)data;
        if (win->is_running) return;

        const char* in_path = win->inputPath->value();
        const char* out_path = win->outputPath->value();

        if (!in_path || strlen(in_path) == 0 || !out_path || strlen(out_path) == 0) {
            fl_alert("Please select both input and output paths.");
            return;
        }

        win->logBuffer->text("");
        win->is_running = true;
        win->importBtn->deactivate();
        win->statusBar->label("Importing...");

        ThreadData* t_data = new ThreadData{
            in_path,
            out_path,
            &win->messages,
            &win->mtx,
            &win->is_running
        };

        if (win->worker_thread.joinable()) {
            win->worker_thread.join();
        }

        win->worker_thread = std::thread(import_thread_func, t_data);
    }

    static void on_idle_cb(void* data) {
        ModelImporterWindow* win = (ModelImporterWindow*)data;
        win->mtx.lock();
        if (!win->messages.empty()) {
            for (const auto& msg : win->messages) {
                win->logBuffer->append(msg.c_str());
                win->logBuffer->append("\n");
            }
            win->messages.clear();
            win->logDisplay->scroll(win->logBuffer->length(), 0);
        }
        win->mtx.unlock();

        if (!win->is_running && !win->importBtn->active()) {
            win->importBtn->activate();
            win->statusBar->label("Ready.");
        }
    }
};

int main(int argc, char** argv) {
    ModelImporterWindow* window = new ModelImporterWindow(580, 500, "Tectonic Model Importer");
    window->end();
    window->show(argc, argv);
    return Fl::run();
}
