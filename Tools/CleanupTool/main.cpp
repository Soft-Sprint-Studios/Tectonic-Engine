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
#include <iostream>
#include <fstream>
#include <string>
#include <vector>
#include <set>
#include <filesystem>
#include <algorithm>
#include <map>
#include <cctype>
#include <sstream>

namespace fs = std::filesystem;

struct MaterialInfo {
    string diffusePath;
    string normalPath;
    string rmaPath;
    string heightPath;
};

struct WaterInfo {
    string normalPath;
    string dudvPath;
    string flowmapPath;
};

bool string_ends_with(const std::string& str, const std::string& suffix) {
    if (str.length() < suffix.length()) {
        return false;
    }
    return str.compare(str.length() - suffix.length(), suffix.length(), suffix) == 0;
}

string trim(const string& str) {
    size_t first = str.find_first_not_of(" \t\n\r");
    if (string::npos == first) {
        return str;
    }
    size_t last = str.find_last_not_of(" \t\n\r");
    return str.substr(first, (last - first + 1));
}

set<string> get_all_files_in(const fs::path& dir, const vector<string>& extensions) {
    set<string> files;
    if (!fs::exists(dir) || !fs::is_directory(dir)) {
        return files;
    }

    for (const auto& entry : fs::recursive_directory_iterator(dir)) {
        if (entry.is_regular_file()) {
            string ext = entry.path().extension().string();
            transform(ext.begin(), ext.end(), ext.begin(), [](unsigned char c) { return tolower(c); });
            for (const auto& valid_ext : extensions) {
                if (ext == valid_ext) {
                    string relative_path = fs::relative(entry.path(), fs::current_path()).string();
                    replace(relative_path.begin(), relative_path.end(), '\\', '/');
                    files.insert(relative_path);
                    break;
                }
            }
        }
    }
    return files;
}

map<string, MaterialInfo> parse_materials_def(const string& path) {
    map<string, MaterialInfo> materials;
    ifstream file(path);
    if (!file.is_open()) return materials;

    string line;
    MaterialInfo current_material;
    string current_name;
    bool in_material = false;

    while (getline(file, line)) {
        line = trim(line);
        if (line.empty() || line.rfind("//", 0) == 0 || line.rfind("#", 0) == 0) continue;

        if (line[0] == '"') {
            if (!current_name.empty()) materials[current_name] = current_material;
            current_name = line.substr(1, line.find('"', 1) - 1);
            current_material = {};
        }
        else if (line == "{") {
            in_material = true;
        }
        else if (line == "}") {
            if (!current_name.empty()) materials[current_name] = current_material;
            in_material = false;
            current_name = "";
        }
        else if (in_material) {
            stringstream ss(line);
            string key, eq, value;
            ss >> key >> eq >> value;
            value = trim(value);
            if (value.front() == '"' && value.back() == '"') value = value.substr(1, value.length() - 2);

            if (key == "diffuse") current_material.diffusePath = value;
            else if (key == "normal") current_material.normalPath = value;
            else if (key == "arm") current_material.rmaPath = value;
            else if (key == "height") current_material.heightPath = value;
        }
    }
    if (!current_name.empty()) materials[current_name] = current_material;
    return materials;
}

map<string, WaterInfo> parse_waters_def(const string& path) {
    map<string, WaterInfo> waters;
    ifstream file(path);
    if (!file.is_open()) return waters;

    string line;
    WaterInfo current_water;
    string current_name;
    bool in_water_def = false;

    while (getline(file, line)) {
        line = trim(line);
        if (line.empty() || line.rfind("//", 0) == 0 || line.rfind("#", 0) == 0) continue;

        if (line[0] == '"') {
            if (!current_name.empty()) waters[current_name] = current_water;
            current_name = line.substr(1, line.find('"', 1) - 1);
            current_water = {};
        }
        else if (line == "{") {
            in_water_def = true;
        }
        else if (line == "}") {
            if (!current_name.empty()) waters[current_name] = current_water;
            in_water_def = false;
            current_name = "";
        }
        else if (in_water_def) {
            stringstream ss(line);
            string key, eq, value;
            ss >> key >> eq >> value;
            value = trim(value);
            if (value.front() == '"' && value.back() == '"') value = value.substr(1, value.length() - 2);

            if (key == "normal") current_water.normalPath = value;
            else if (key == "dudv") current_water.dudvPath = value;
            else if (key == "flowmap") current_water.flowmapPath = value;
        }
    }
    if (!current_name.empty()) waters[current_name] = current_water;
    return waters;
}

map<string, string> parse_particle_files(const string& dir_path) {
    map<string, string> particle_materials;
    if (!fs::exists(dir_path)) return particle_materials;

    for (const auto& entry : fs::directory_iterator(dir_path)) {
        if (entry.is_regular_file() && entry.path().extension() == ".par") {
            ifstream file(entry.path());
            string line;
            while (getline(file, line)) {
                stringstream ss(line);
                string key, value;
                ss >> key >> value;
                if (key == "texture") {
                    string relative_texture_path = "textures/" + value;
                    particle_materials[entry.path().string()] = relative_texture_path;
                    break;
                }
            }
        }
    }
    return particle_materials;
}

void rewrite_materials_def(const string& path, const map<string, MaterialInfo>& all_materials, const set<string>& used_materials) {
    ifstream in(path);
    if (!in.is_open()) return;

    ofstream out("materials_temp.def");
    if (!out.is_open()) return;

    string line;
    string current_name;
    vector<string> block_lines;
    bool in_block = false;

    while (getline(in, line)) {
        string trimmed = trim(line);

        if (!in_block && !trimmed.empty() && trimmed[0] == '"') {
            current_name = trimmed.substr(1, trimmed.find('"', 1) - 1);
            block_lines.clear();
            block_lines.push_back(line);
            in_block = true;
            continue;
        }

        if (in_block) {
            block_lines.push_back(line);

            if (trimmed == "}") {
                if (used_materials.count(current_name)) {
                    for (const auto& l : block_lines) {
                        out << l << "\n";
                    }
                }
                in_block = false;
                current_name.clear();
                block_lines.clear();
            }
        }
        else {
            out << line << "\n";
        }
    }

    in.close();
    out.close();

    fs::remove(path);
    fs::rename("materials_temp.def", path);
}

void parse_map_file(const fs::path& path, set<string>& used_files, set<string>& used_materials, set<string>& used_water_defs) {
    ifstream file(path);
    if (!file.is_open()) return;

    bool in_brush_block = false;
    bool in_brush_props_block = false;
    string current_brush_classname;

    string line;
    while (getline(file, line)) {
        line = trim(line);
        stringstream ss(line);
        string keyword;
        ss >> keyword;

        if (keyword == "brush_begin") { in_brush_block = true; current_brush_classname = ""; continue; }
        if (keyword == "brush_end") { in_brush_block = false; in_brush_props_block = false; continue; }

        if (in_brush_block) {
            if (keyword == "classname") { ss >> current_brush_classname; current_brush_classname = current_brush_classname.substr(1, current_brush_classname.length() - 2); continue; }
            if (keyword == "properties") { in_brush_props_block = true; continue; }
            if (in_brush_props_block && keyword == "}") { in_brush_props_block = false; continue; }

            if (in_brush_props_block && current_brush_classname == "func_water") {
                if (line.find("\"water_def\"") != string::npos) {
                    string key_part, val_part;
                    stringstream prop_ss(line);
                    prop_ss >> key_part >> val_part;
                    val_part = val_part.substr(1, val_part.length() - 2);
                    used_water_defs.insert(val_part);
                }
            }
        }

        if (keyword == "gltf_model") {
            string model_path; ss >> model_path; used_files.insert(model_path);
        }
        else if (keyword == "decal" || keyword == "env_overlay" || keyword == "sprite") {
            string material_name; ss >> material_name; material_name = material_name.substr(1, material_name.length() - 2); used_materials.insert(material_name);
        }
        else if (keyword == "f") {
            size_t first_quote = line.find('"');
            if (first_quote != string::npos) {
                size_t current_pos = first_quote;
                for (int i = 0; i < 4; ++i) {
                    current_pos = line.find('"', current_pos);
                    if (current_pos == string::npos) break;
                    size_t next_quote = line.find('"', current_pos + 1);
                    if (next_quote == string::npos) break;
                    string mat_name = line.substr(current_pos + 1, next_quote - (current_pos + 1));
                    if (mat_name != "null" && mat_name != "___MISSING___") {
                        used_materials.insert(mat_name);
                    }
                    current_pos = next_quote + 1;
                }
            }
        }
        else if (keyword == "skybox") {
            int dummy; string skybox_name; ss >> dummy; ss >> skybox_name; skybox_name = skybox_name.substr(1, skybox_name.length() - 2);
            const char* suffixes[] = { "_px.png", "_nx.png", "_py.png", "_ny.png", "_pz.png", "_nz.png" };
            for (const auto& suffix : suffixes) { used_files.insert("skybox/" + skybox_name + suffix); }
        }
        else if (keyword == "sound_entity") {
            string name_quoted, sound_path_quoted; ss >> quoted(name_quoted) >> quoted(sound_path_quoted); used_files.insert(sound_path_quoted);
        }
        else if (keyword == "particle_emitter") {
            string par_file_quoted, name_quoted; ss >> quoted(par_file_quoted) >> quoted(name_quoted); used_files.insert(par_file_quoted);
        }
        else if (keyword == "video_player") {
            string video_path_quoted, name_quoted; ss >> quoted(video_path_quoted) >> quoted(name_quoted); used_files.insert(video_path_quoted);
        }
        else if (keyword == "parallax_room") {
            string cubemap_path_base_quoted, name_quoted; ss >> quoted(cubemap_path_base_quoted) >> quoted(name_quoted);
            const char* suffixes[] = { "_px.png", "_nx.png", "_py.png", "_ny.png", "_pz.png", "_nz.png" };
            for (const auto& suffix : suffixes) { used_files.insert(cubemap_path_base_quoted + suffix); }
        }
        else if (keyword == "color_correction") {
            int dummy; string lut_path_quoted; ss >> dummy >> quoted(lut_path_quoted);
            if (!lut_path_quoted.empty() && lut_path_quoted != "null") {
                used_files.insert("textures/" + lut_path_quoted);
            }
        }
    }
}

int main(int argc, char* argv[]) {
    bool perform_delete = false;
    if (argc > 1 && string(argv[1]) == "--delete") {
        cout << "!!! DELETE MODE ENABLED. Unused files will be permanently removed. !!!" << endl;
        perform_delete = true;
    }

    ofstream log_file("cleanup_log.txt");

    cout << "--- Tectonic Engine Cleanup Tool ---" << endl;
    log_file << "--- Tectonic Engine Cleanup Tool ---" << endl;

    string materials_def_path = "materials.def";
    string waters_def_path = "waters.def";
    string particle_dir_path = "particles/";
    string maps_dir_path = "maps/";
    string textures_dir_path = "textures/";
    string models_dir_path = "models/";
    string sounds_dir_path = "sounds/";
    string media_dir_path = "media/";
    string skybox_dir_path = "skybox/";

    auto all_materials = parse_materials_def(materials_def_path);
    auto all_waters = parse_waters_def(waters_def_path);
    auto all_particle_defs = parse_particle_files(particle_dir_path);

    set<string> used_asset_files;
    set<string> used_material_names;
    set<string> used_water_defs;

    cout << "Scanning map in " << maps_dir_path << "..." << endl;
    log_file << "Scanning map in " << maps_dir_path << "..." << endl;

    if (fs::exists(maps_dir_path)) {
        for (const auto& entry : fs::recursive_directory_iterator(maps_dir_path)) {
            if (entry.is_regular_file() && (entry.path().extension() == ".map" || entry.path().extension() == ".sav")) {
                cout << "  - " << entry.path().string() << endl;
                log_file << "  - " << entry.path().string() << endl;
                parse_map_file(entry.path(), used_asset_files, used_material_names, used_water_defs);
            }
        }
    }

    cout << "Resolving used assets..." << endl;
    log_file << "Resolving used assets..." << endl;

    for (const auto& file_path : used_asset_files) {
        if (string_ends_with(file_path, ".par")) {
            if (all_particle_defs.count(file_path)) {
                used_material_names.insert(all_particle_defs[file_path]);
            }
        }
    }

    for (const auto& mat_name : used_material_names) {
        if (all_materials.count(mat_name)) {
            const auto& mat_info = all_materials[mat_name];
            if (!mat_info.diffusePath.empty()) used_asset_files.insert(textures_dir_path + mat_info.diffusePath);
            if (!mat_info.normalPath.empty()) used_asset_files.insert(textures_dir_path + mat_info.normalPath);
            if (!mat_info.rmaPath.empty()) used_asset_files.insert(textures_dir_path + mat_info.rmaPath);
            if (!mat_info.heightPath.empty()) used_asset_files.insert(textures_dir_path + mat_info.heightPath);
        }
    }

    for (const auto& water_name : used_water_defs) {
        if (all_waters.count(water_name)) {
            const auto& water_info = all_waters[water_name];
            if (!water_info.normalPath.empty()) used_asset_files.insert(textures_dir_path + water_info.normalPath);
            if (!water_info.dudvPath.empty()) used_asset_files.insert(textures_dir_path + water_info.dudvPath);
            if (!water_info.flowmapPath.empty()) used_asset_files.insert(textures_dir_path + water_info.flowmapPath);
        }
    }

    used_asset_files.insert(media_dir_path + "cursor.png");
    used_asset_files.insert(sounds_dir_path + "flashlight01.wav");
    used_asset_files.insert(sounds_dir_path + "footstep.wav");
    used_asset_files.insert(sounds_dir_path + "jump.wav");
    used_asset_files.insert(sounds_dir_path + "geiger_tick.wav");
    used_asset_files.insert(media_dir_path + "menu.mpg");
    used_asset_files.insert("fonts/Roboto-Regular.ttf");
    used_asset_files.insert("brdf_lut.png");
    used_asset_files.insert("clouds.png");
    used_asset_files.insert("dev01.png");
    used_asset_files.insert("dev01_normal.png");
    used_asset_files.insert("dev02.png");
    used_asset_files.insert("dev02_normal.png");
    used_asset_files.insert(models_dir_path + "error.glb");
    used_asset_files.insert(sounds_dir_path + "pistol_fire.mp3");

    cout << "Scanning asset directories..." << endl;
    log_file << "Scanning asset directories..." << endl;

    auto disk_textures = get_all_files_in(textures_dir_path, { ".png", ".jpg", ".tga" });
    auto disk_models = get_all_files_in(models_dir_path, { ".gltf", ".glb" });
    auto disk_sounds = get_all_files_in(sounds_dir_path, { ".wav", ".mp3" });
    auto disk_particles = get_all_files_in(particle_dir_path, { ".par" });
    auto disk_videos = get_all_files_in(media_dir_path, { ".mpg" });
    auto disk_skybox = get_all_files_in(skybox_dir_path, { ".png" });

    cout << "\n--- Analysis Complete ---" << endl;
    log_file << "--- Analysis Complete ---" << endl;

    vector<fs::path> unused_files;

    for (const auto& file : disk_textures) if (used_asset_files.find(file) == used_asset_files.end()) unused_files.push_back(file);
    for (const auto& file : disk_models) if (used_asset_files.find(file) == used_asset_files.end()) unused_files.push_back(file);
    for (const auto& file : disk_sounds) if (used_asset_files.find(file) == used_asset_files.end()) unused_files.push_back(file);
    for (const auto& file : disk_particles) if (used_asset_files.find(file) == used_asset_files.end()) unused_files.push_back(file);
    for (const auto& file : disk_videos) if (used_asset_files.find(file) == used_asset_files.end()) unused_files.push_back(file);
    for (const auto& file : disk_skybox) if (used_asset_files.find(file) == used_asset_files.end()) unused_files.push_back(file);

    if (unused_files.empty()) {
        cout << "No unused assets found. Your project is clean!" << endl;
        log_file << "No unused assets found." << endl;
    }
    else {
        cout << "Found " << unused_files.size() << " unused asset files:" << endl;
        log_file << "Found " << unused_files.size() << " unused asset files:" << endl;

        for (const auto& path : unused_files) {
            cout << "  - " << path.string() << endl;
            log_file << "  - " << path.string() << endl;

            if (perform_delete) {
                try {
                    fs::remove(path);
                    log_file << "Deleted: " << path.string() << endl;
                }
                catch (const fs::filesystem_error& e) {
                    cerr << "    Error deleting file: " << e.what() << endl;
                    log_file << "Error deleting: " << path.string() << " | " << e.what() << endl;
                }
            }
        }

        if (perform_delete) {
            rewrite_materials_def(materials_def_path, all_materials, used_material_names);
            cout << "\n" << unused_files.size() << " unused files have been processed for deletion." << endl;
            log_file << unused_files.size() << " unused files deleted." << endl;
        }
        else {
            cout << "\nTo delete these files, run with the --delete flag." << endl;
            log_file << "Run with --delete to remove unused files." << endl;
        }
    }

    return 0;
}