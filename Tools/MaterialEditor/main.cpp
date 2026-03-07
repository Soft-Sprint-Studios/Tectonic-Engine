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
#include <FL/Fl.H>
#include <FL/Fl_Window.H>
#include <FL/Fl_Menu_Bar.H>
#include <FL/Fl_Select_Browser.H>
#include <FL/Fl_Input.H>
#include <FL/Fl_Float_Input.H>
#include <FL/Fl_Check_Button.H>
#include <FL/Fl_Button.H>
#include <FL/Fl_File_Chooser.H>
#include <FL/fl_ask.H>
#include <vector>
#include <string>
#include <fstream>
#include <sstream>
#include <algorithm>
#include <filesystem>

#ifdef PLATFORM_WINDOWS
#include <windows.h>
#else
#include <unistd.h>
#endif

struct Material {
    string name;
    string diffusePath;
    string normalPath;
    string rmaPath;
    string heightPath;
    string detailDiffusePath;
    Float heightScale = 0.0f;
    Float detailScale = 1.0f;
    Float roughness = -1.0f;
    Float metalness = -1.0f;
    Bool alpha = false;
};

vector<Material> g_materials;
Int g_selected_material_index = -1;
Bool g_is_dirty = false;
const Char* g_materials_file = "materials.def";

Fl_Window* g_main_window;
Fl_Select_Browser* g_material_browser;
Fl_Input* g_name_input;
Fl_Input* g_diffuse_input;
Fl_Input* g_normal_input;
Fl_Input* g_rma_input;
Fl_Input* g_height_input;
Fl_Input* g_detail_diffuse_input;
Fl_Float_Input* g_height_scale_input;
Fl_Float_Input* g_detail_scale_input;
Fl_Float_Input* g_roughness_input;
Fl_Float_Input* g_metalness_input;
Fl_Check_Button* g_alpha_check;

void update_ui_for_selection();
void save_materials(const Char* path);

string get_relative_texture_path(const Char* full_path) {
    if (!full_path) return "";
    filesystem::path p(full_path);
    auto it = find_if(p.begin(), p.end(), [](const filesystem::path& part) {
        return part == "textures";
        });

    if (it != p.end()) {
        filesystem::path relative_p;
        for (++it; it != p.end(); ++it) {
            relative_p /= *it;
        }
        string result = relative_p.string();
#ifdef PLATFORM_WINDOWS
        replace(result.begin(), result.end(), '\\', '/');
#endif
        if (!result.empty() && (result[0] == '/' || result[0] == '\\')) {
            return result.substr(1);
        }
        return result;
    }
    return p.filename().string();
}


void load_materials(const Char* path) {
    g_materials.clear();
    ifstream file(path);
    if (!file.is_open()) {
        fl_alert("Could not open %s", path);
        return;
    }

    string line;
    Material current_material;
    Bool in_material = false;

    while (getline(file, line)) {
        stringstream ss(line);
        string token;
        ss >> token;

        if (token.empty() || token[0] == '/' || token[0] == '#') continue;

        if (token[0] == '"') {
            if (in_material) {
                g_materials.push_back(current_material);
            }
            current_material = Material();
            current_material.name = line.substr(1, line.find('"', 1) - 1);
            in_material = false;
        }
        else if (token == "{") {
            in_material = true;
        }
        else if (token == "}") {
            if (in_material) {
                g_materials.push_back(current_material);
                in_material = false;
            }
        }
        else if (in_material) {
            string key = token;
            ss >> token;
            string value_str;
            getline(ss, value_str);

            value_str.erase(0, value_str.find_first_not_of(" \t\""));
            value_str.erase(value_str.find_last_not_of(" \t\"") + 1);

            if (key == "diffuse") current_material.diffusePath = value_str;
            else if (key == "normal") current_material.normalPath = value_str;
            else if (key == "arm") current_material.rmaPath = value_str;
            else if (key == "height") current_material.heightPath = value_str;
            else if (key == "detail") current_material.detailDiffusePath = value_str;
            else if (key == "heightScale") current_material.heightScale = stof(value_str);
            else if (key == "detailscale") current_material.detailScale = stof(value_str);
            else if (key == "roughness") current_material.roughness = stof(value_str);
            else if (key == "metalness") current_material.metalness = stof(value_str);
            else if (key == "alpha") current_material.alpha = (stof(value_str) != 0.0f);
        }
    }
    g_is_dirty = false;
}

void populate_browser() {
    g_material_browser->clear();
    for (const auto& mat : g_materials) {
        g_material_browser->add(mat.name.c_str());
    }
}

void input_changed_cb(Fl_Widget* w, void* data) {
    if (g_selected_material_index == -1) return;
    Material& mat = g_materials[g_selected_material_index];

    if (w == g_name_input) {
        mat.name = g_name_input->value();
        g_material_browser->text(g_selected_material_index + 1, mat.name.c_str());
    }
    else if (w == g_diffuse_input) mat.diffusePath = g_diffuse_input->value();
    else if (w == g_normal_input) mat.normalPath = g_normal_input->value();
    else if (w == g_rma_input) mat.rmaPath = g_rma_input->value();
    else if (w == g_height_input) mat.heightPath = g_height_input->value();
    else if (w == g_detail_diffuse_input) mat.detailDiffusePath = g_detail_diffuse_input->value();
    else if (w == g_height_scale_input) mat.heightScale = stof(g_height_scale_input->value());
    else if (w == g_detail_scale_input) mat.detailScale = stof(g_detail_scale_input->value());
    else if (w == g_roughness_input) mat.roughness = stof(g_roughness_input->value());
    else if (w == g_metalness_input) mat.metalness = stof(g_metalness_input->value());
    else if (w == g_alpha_check) mat.alpha = g_alpha_check->value();

    g_is_dirty = true;
}

void material_select_cb(Fl_Widget* w, void*) {
    if (g_material_browser->value() == 0) {
        g_selected_material_index = -1;
    }
    else {
        g_selected_material_index = g_material_browser->value() - 1;
    }
    update_ui_for_selection();
}

void update_ui_for_selection() {
    Bool enabled = (g_selected_material_index != -1);
    g_name_input->deactivate();
    g_diffuse_input->deactivate();
    g_normal_input->deactivate();
    g_rma_input->deactivate();
    g_height_input->deactivate();
    g_detail_diffuse_input->deactivate();
    g_height_scale_input->deactivate();
    g_detail_scale_input->deactivate();
    g_roughness_input->deactivate();
    g_metalness_input->deactivate();
    g_alpha_check->deactivate();

    if (enabled) {
        const Material& mat = g_materials[g_selected_material_index];
        Char buffer[32];
        g_name_input->value(mat.name.c_str());
        g_diffuse_input->value(mat.diffusePath.c_str());
        g_normal_input->value(mat.normalPath.c_str());
        g_rma_input->value(mat.rmaPath.c_str());
        g_height_input->value(mat.heightPath.c_str());
        g_detail_diffuse_input->value(mat.detailDiffusePath.c_str());
        snprintf(buffer, sizeof(buffer), "%.3f", mat.heightScale); g_height_scale_input->value(buffer);
        snprintf(buffer, sizeof(buffer), "%.3f", mat.detailScale); g_detail_scale_input->value(buffer);
        snprintf(buffer, sizeof(buffer), "%.3f", mat.roughness); g_roughness_input->value(buffer);
        snprintf(buffer, sizeof(buffer), "%.3f", mat.metalness); g_metalness_input->value(buffer);
        g_alpha_check->value(mat.alpha);

        g_name_input->activate();
        g_diffuse_input->activate();
        g_normal_input->activate();
        g_rma_input->activate();
        g_height_input->activate();
        g_detail_diffuse_input->activate();
        g_height_scale_input->activate();
        g_detail_scale_input->activate();
        g_roughness_input->activate();
        g_metalness_input->activate();
        g_alpha_check->activate();
    }
    else {
        g_name_input->value("");
        g_diffuse_input->value("");
        g_normal_input->value("");
        g_rma_input->value("");
        g_height_input->value("");
        g_detail_diffuse_input->value("");
        g_height_scale_input->value("");
        g_detail_scale_input->value("");
        g_roughness_input->value("");
        g_metalness_input->value("");
        g_alpha_check->value(0);
    }
}

void new_cb(Fl_Widget* w, void*) {
    Material new_mat;
    new_mat.name = "NewMaterial";
    g_materials.push_back(new_mat);
    populate_browser();
    g_material_browser->select(g_materials.size());
    g_selected_material_index = g_materials.size() - 1;
    update_ui_for_selection();
    g_is_dirty = true;
}

void delete_cb(Fl_Widget* w, void*) {
    if (g_selected_material_index != -1) {
        g_materials.erase(g_materials.begin() + g_selected_material_index);
        g_selected_material_index = -1;
        populate_browser();
        update_ui_for_selection();
        g_is_dirty = true;
    }
}

void save_cb(Fl_Widget* w, void*) {
    save_materials(g_materials_file);
}

void exit_cb(Fl_Widget* w, void*) {
    if (g_is_dirty) {
        Int choice = fl_choice("You have unsaved changes. Save before exiting?", "Cancel", "Save", "Don't Save");
        if (choice == 0) return;
        if (choice == 1) save_materials(g_materials_file);
    }
    g_main_window->hide();
}

void save_materials(const Char* path) {
    ofstream file(path);
    if (!file.is_open()) {
        fl_alert("Failed to save to %s", path);
        return;
    }

    for (const auto& mat : g_materials) {
        file << "\"" << mat.name << "\"\n";
        file << "{\n";
        if (!mat.diffusePath.empty()) file << "\tdiffuse = \"" << mat.diffusePath << "\"\n";
        if (!mat.normalPath.empty()) file << "\tnormal = \"" << mat.normalPath << "\"\n";
        if (!mat.rmaPath.empty()) file << "\tarm = \"" << mat.rmaPath << "\"\n";
        if (!mat.heightPath.empty()) file << "\theight = \"" << mat.heightPath << "\"\n";
        if (!mat.detailDiffusePath.empty()) file << "\tdetail = \"" << mat.detailDiffusePath << "\"\n";
        if (mat.heightScale != 0.0f) file << "\theightScale = " << mat.heightScale << "\n";
        if (mat.detailScale != 1.0f) file << "\tdetailscale = " << mat.detailScale << "\n";
        if (mat.roughness != -1.0f) file << "\troughness = " << mat.roughness << "\n";
        if (mat.metalness != -1.0f) file << "\tmetalness = " << mat.metalness << "\n";
        if (mat.alpha) file << "\talpha = 1\n";
        file << "}\n\n";
    }
    g_is_dirty = false;
    fl_alert("Materials saved to %s", path);
}

void browse_cb(Fl_Widget* w, void* data) {
    if (g_selected_material_index == -1) return;

    Fl_Input* target_input = (Fl_Input*)data;
    Fl_File_Chooser chooser(".", "*.png\tPNG Files\n*.jpg\tJPG Files\n*.*\tAll Files", Fl_File_Chooser::SINGLE, "Select Texture");
    chooser.show();
    while (chooser.shown()) { Fl::wait(); }

    if (chooser.value() != NULL) {
        string rel_path = get_relative_texture_path(chooser.value());
        target_input->value(rel_path.c_str());
        target_input->do_callback();
    }
}

void on_about_cb(Fl_Widget*, void*) {
    fl_message_title("About Tectonic Material Editor");
    fl_message("A tool to edit materials for the Tectonic Engine.\n\nCopyright (c) 2025-2026 Soft Sprint Studios");
}

Int main(Int argc, Char** argv) {
    g_main_window = new Fl_Window(800, 490, "Tectonic Material Editor");

    Fl_Menu_Bar* menu = new Fl_Menu_Bar(0, 0, 800, 25);
    menu->add("&File/&Save", FL_CTRL + 's', save_cb);
    menu->add("&File/&Exit", FL_CTRL + 'q', exit_cb);
    menu->add("&Material/&New", FL_CTRL + 'n', new_cb);
    menu->add("&Material/&Delete", FL_Delete, delete_cb);
    menu->add("Help/About", 0, on_about_cb);

    g_material_browser = new Fl_Select_Browser(10, 35, 210, 435, "Materials");
    g_material_browser->callback(material_select_cb);
    g_material_browser->align(FL_ALIGN_BOTTOM);

    constexpr Int x_box = 370;
    constexpr Int input_w = 380;
    constexpr Int h = 25;
    constexpr Int step = 30;
    constexpr Int bw = 30;
    Int cur_y = 35;

    auto setup_input = [&](Fl_Input* in) {
        in->align(FL_ALIGN_LEFT);
        in->callback(input_changed_cb);
        };

    g_name_input = new Fl_Input(x_box, cur_y, input_w + bw + 5, h, "Material Name");
    setup_input(g_name_input);
    cur_y += step;

    g_diffuse_input = new Fl_Input(x_box, cur_y, input_w, h, "Diffuse Map");
    setup_input(g_diffuse_input);
    Fl_Button* b_diff = new Fl_Button(x_box + input_w + 5, cur_y, bw, h, "...");
    b_diff->callback(browse_cb, g_diffuse_input);
    cur_y += step;

    g_normal_input = new Fl_Input(x_box, cur_y, input_w, h, "Normal Map");
    setup_input(g_normal_input);
    Fl_Button* b_norm = new Fl_Button(x_box + input_w + 5, cur_y, bw, h, "...");
    b_norm->callback(browse_cb, g_normal_input);
    cur_y += step;

    g_rma_input = new Fl_Input(x_box, cur_y, input_w, h, "RMA (ARM) Map");
    setup_input(g_rma_input);
    Fl_Button* b_rma = new Fl_Button(x_box + input_w + 5, cur_y, bw, h, "...");
    b_rma->callback(browse_cb, g_rma_input);
    cur_y += step;

    g_height_input = new Fl_Input(x_box, cur_y, input_w, h, "Height Map");
    setup_input(g_height_input);
    Fl_Button* b_height = new Fl_Button(x_box + input_w + 5, cur_y, bw, h, "...");
    b_height->callback(browse_cb, g_height_input);
    cur_y += step;

    g_detail_diffuse_input = new Fl_Input(x_box, cur_y, input_w, h, "Detail Map");
    setup_input(g_detail_diffuse_input);
    Fl_Button* b_detail = new Fl_Button(x_box + input_w + 5, cur_y, bw, h, "...");
    b_detail->callback(browse_cb, g_detail_diffuse_input);
    cur_y += step + 10;

    g_height_scale_input = new Fl_Float_Input(x_box, cur_y, input_w + bw + 5, h, "Height Scale");
    g_height_scale_input->callback(input_changed_cb);
    cur_y += step;

    g_detail_scale_input = new Fl_Float_Input(x_box, cur_y, input_w + bw + 5, h, "Detail Scale");
    g_detail_scale_input->callback(input_changed_cb);
    cur_y += step;

    g_roughness_input = new Fl_Float_Input(x_box, cur_y, input_w + bw + 5, h, "Roughness Override");
    g_roughness_input->callback(input_changed_cb);
    cur_y += step;

    g_metalness_input = new Fl_Float_Input(x_box, cur_y, input_w + bw + 5, h, "Metalness Override");
    g_metalness_input->callback(input_changed_cb);
    cur_y += step + 10;

    g_alpha_check = new Fl_Check_Button(x_box, cur_y, 200, h, "Alpha Test (Cutout)");
    g_alpha_check->callback(input_changed_cb);

    g_main_window->end();
    g_main_window->callback(exit_cb);

    load_materials(g_materials_file);
    populate_browser();
    update_ui_for_selection();

    g_main_window->show(argc, argv);
    return Fl::run();
}