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
#include <iostream>
#include <fstream>
#include <filesystem>
#include <string>
#include <vector>
#include <cctype>
#include <cstring>

#define CGLTF_IMPLEMENTATION
#include "cgltf.h"

namespace fs = std::filesystem;

void ensure_directory_exists(const fs::path& path) {
    if (!fs::exists(path)) {
        fs::create_directories(path);
    }
}

void sanitize_for_filename(const std::string& input, std::string& output) {
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

bool save_image_data(cgltf_image* image, const std::string& base_filename, const std::string& suffix, std::string& out_texture_path) {
    if (!image) return false;

    if (image->buffer_view) {
        std::string extension = ".png";
        if (image->mime_type && strcmp(image->mime_type, "image/jpeg") == 0) {
            extension = ".jpg";
        }

        out_texture_path = base_filename + "_" + suffix + extension;
        fs::path full_output_path = fs::path("textures") / out_texture_path;

        std::ofstream ofs(full_output_path, std::ios::binary);
        if (!ofs) {
            std::cerr << "ERROR: Could not write: " << full_output_path << "\n";
            return false;
        }

        const char* buffer_data = static_cast<const char*>(image->buffer_view->buffer->data) + image->buffer_view->offset;
        ofs.write(buffer_data, image->buffer_view->size);
        return true;
    }

    if (image->uri && std::strlen(image->uri) > 0) {
        std::cout << "WARNING: External image '" << image->uri << "' detected. Skipping extraction.\n";
        out_texture_path = (fs::path("models") / fs::path(image->uri).filename()).string();
        return true;
    }

    return false;
}



int process_gltf(const fs::path& gltf_path) {
    std::string gltf_basename = gltf_path.stem().string();

    cgltf_options options = {};
    cgltf_data* data = nullptr;
    cgltf_result result = cgltf_parse_file(&options, gltf_path.string().c_str(), &data);
    if (result != cgltf_result_success) {
        std::cerr << "ERROR: Could not parse GLTF file: " << gltf_path << "\n";
        return 1;
    }

    result = cgltf_load_buffers(&options, data, gltf_path.string().c_str());
    if (result != cgltf_result_success) {
        std::cerr << "ERROR: Could not load GLTF buffers for: " << gltf_path << "\n";
        cgltf_free(data);
        return 1;
    }

    std::cout << "Successfully loaded '" << gltf_path << "'. Processing " << data->materials_count << " materials...\n";

    ensure_directory_exists("textures");

    std::ofstream mat_file("materials.def", std::ios::app);
    if (!mat_file) {
        std::cerr << "ERROR: Could not open materials.def for appending.\n";
        cgltf_free(data);
        return 1;
    }

    for (size_t i = 0; i < data->materials_count; ++i) {
        cgltf_material* mat = &data->materials[i];

        std::string mat_name_sanitized;
        if (mat->name) {
            sanitize_for_filename(mat->name, mat_name_sanitized);
        }
        else {
            mat_name_sanitized = gltf_basename + "_mat_" + std::to_string(i);
        }

        std::cout << "Processing material: " << (mat->name ? mat->name : mat_name_sanitized.c_str()) << "\n";

        mat_file << "\"" << (mat->name ? mat->name : mat_name_sanitized) << "\"\n";
        mat_file << "{\n";

        std::string texture_path_buffer;

        if (mat->has_pbr_metallic_roughness && mat->pbr_metallic_roughness.base_color_texture.texture) {
            cgltf_image* image = mat->pbr_metallic_roughness.base_color_texture.texture->image;
            if (save_image_data(image, mat_name_sanitized, "diffuse", texture_path_buffer)) {
                mat_file << "    diffuse = \"" << texture_path_buffer << "\"\n";
            }
        }

        if (mat->normal_texture.texture) {
            cgltf_image* image = mat->normal_texture.texture->image;
            if (save_image_data(image, mat_name_sanitized, "normal", texture_path_buffer)) {
                mat_file << "    normal = \"" << texture_path_buffer << "\"\n";
            }
        }

        if (mat->has_pbr_metallic_roughness && mat->pbr_metallic_roughness.metallic_roughness_texture.texture) {
            cgltf_image* image = mat->pbr_metallic_roughness.metallic_roughness_texture.texture->image;
            if (save_image_data(image, mat_name_sanitized, "rma", texture_path_buffer)) {
                mat_file << "    rma = \"" << texture_path_buffer << "\"\n";
            }
        }

        mat_file << "}\n\n";
    }

    mat_file.close();
    cgltf_free(data);
    std::cout << "Processing complete. Check materials.def and the textures/ directory.\n";
    return 0;
}

bool has_gltf_extension(const fs::path& filename) {
    auto ext = filename.extension().string();
    for (auto& c : ext) c = static_cast<char>(std::tolower(c));
    return ext == ".gltf" || ext == ".glb";
}

int main(int argc, char* argv[]) {
    if (argc < 2 || argc > 3) {
        std::cout << "Tectonic Engine GLTF Importer\n";
        std::cout << "Usage: " << argv[0] << " <path_to_gltf_file_or_folder> [output_folder]\n";
        return 1;
    }

    fs::path input_path = argv[1];
    fs::path output_folder = "textures";

    if (argc == 3) {
        output_folder = argv[2];
    }

    if (!fs::exists(input_path)) {
        std::cerr << "ERROR: Invalid path: " << input_path << "\n";
        return 1;
    }

    ensure_directory_exists(output_folder);

    fs::current_path(output_folder);

    if (fs::is_directory(input_path)) {
        bool found = false;
        for (auto& p : fs::directory_iterator(input_path)) {
            if (fs::is_regular_file(p) && has_gltf_extension(p.path())) {
                found = true;
                int ret = process_gltf(p.path());
                if (ret != 0) {
                    std::cerr << "Failed to process: " << p.path() << "\n";
                }
            }
        }
        if (!found) {
            std::cerr << "ERROR: No .gltf/.glb files found in directory.\n";
            return 1;
        }
    }
    else if (fs::is_regular_file(input_path)) {
        if (!has_gltf_extension(input_path)) {
            std::cerr << "ERROR: Input file does not have .gltf or .glb extension.\n";
            return 1;
        }
        return process_gltf(input_path);
    }
    else {
        std::cerr << "ERROR: Input path is neither file nor directory.\n";
        return 1;
    }

    return 0;
}