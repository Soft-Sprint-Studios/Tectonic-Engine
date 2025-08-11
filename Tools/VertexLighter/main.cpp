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
#include <FL/Fl_Menu_Bar.H>
#include <FL/Fl_File_Chooser.H>
#include <FL/Fl_Browser.H>
#include <FL/Fl_Box.H>
#include <FL/fl_ask.H>

#include <iostream>
#include <fstream>
#include <vector>
#include <string>
#include <sstream>
#include <iomanip>
#include <algorithm>

struct Vec4 {
    float x, y, z, w;
};

std::vector<Vec4> g_vlm_data;
std::vector<Vec4> g_vld_data;
Fl_Browser* g_browser = nullptr;
Fl_Box* g_statusBar = nullptr;

void update_browser() {
    g_browser->clear();
    g_browser->add("@B| Index | @B|     Color (R, G, B)     | @B|    Direction (X, Y, Z)");

    size_t num_entries = std::max(g_vlm_data.size(), g_vld_data.size());

    for (size_t i = 0; i < num_entries; ++i) {
        std::stringstream ss;
        ss << std::fixed << std::setprecision(3);
        ss << i << "\t| ";

        if (i < g_vlm_data.size()) {
            ss << "(" << g_vlm_data[i].x << ", " << g_vlm_data[i].y << ", " << g_vlm_data[i].z << ")";
        } else {
            ss << "N/A";
        }

        ss << "\t| ";

        if (i < g_vld_data.size()) {
            ss << "(" << g_vld_data[i].x << ", " << g_vld_data[i].y << ", " << g_vld_data[i].z << ")";
        } else {
            ss << "N/A";
        }
        g_browser->add(ss.str().c_str());
    }
}

void load_file(const char* path, bool is_vlm) {
    std::ifstream file(path, std::ios::binary);
    if (!file) {
        std::string msg = "Error: Could not open file: ";
        msg += path;
        fl_alert(msg.c_str());
        return;
    }

    char header[5] = {0};
    file.read(header, 4);

    const char* expected_header = is_vlm ? "VLM1" : "VLD1";
    if (strcmp(header, expected_header) != 0) {
        std::string msg = "Error: Invalid file format for ";
        msg += path;
        fl_alert(msg.c_str());
        return;
    }

    unsigned int count;
    file.read(reinterpret_cast<char*>(&count), sizeof(unsigned int));

    std::vector<Vec4>& target_vector = is_vlm ? g_vlm_data : g_vld_data;
    target_vector.resize(count);

    file.read(reinterpret_cast<char*>(target_vector.data()), sizeof(Vec4) * count);

    std::stringstream status;
    status << "Loaded " << count << " vertices from " << (is_vlm ? "VLM" : "VLD") << " file.";
    g_statusBar->label(status.str().c_str());
    update_browser();
}

void on_open_vlm_cb(Fl_Widget*, void*) {
    Fl_File_Chooser chooser(".", "*.vlm", Fl_File_Chooser::SINGLE, "Open Vertex Light Map");
    chooser.show();
    while(chooser.shown()) { Fl::wait(); }
    if (chooser.value()) {
        g_vlm_data.clear();
        load_file(chooser.value(), true);
    }
}

void on_open_vld_cb(Fl_Widget*, void*) {
    Fl_File_Chooser chooser(".", "*.vld", Fl_File_Chooser::SINGLE, "Open Vertex Light Direction");
    chooser.show();
    while(chooser.shown()) { Fl::wait(); }
    if (chooser.value()) {
        g_vld_data.clear();
        load_file(chooser.value(), false);
    }
}

void on_quit_cb(Fl_Widget*, void*) {
    exit(0);
}

void on_about_cb(Fl_Widget*, void*) {
    fl_message_title("About Tectonic Vertex Light Debugging");
    fl_message("A simple tool to inspect vertex lighting data for the Tectonic Engine.\n\n"
               "Copyright (c) 2025 Soft Sprint Studios");
}

int main(int argc, char **argv) {
    Fl_Window *window = new Fl_Window(700, 500, "Tectonic Vertex Light Debugging");

    Fl_Menu_Bar* menu = new Fl_Menu_Bar(0, 0, 700, 25);
    menu->add("File/Open VLM...", FL_CTRL + 'o', on_open_vlm_cb);
    menu->add("File/Open VLD...", FL_CTRL + FL_SHIFT + 'o', on_open_vld_cb);
    menu->add("File/Quit", FL_CTRL + 'q', on_quit_cb);
    menu->add("Help/About", 0, on_about_cb);
    
    g_browser = new Fl_Browser(10, 35, 680, 430);
    g_browser->type(FL_MULTI_BROWSER);
    g_browser->column_widths(new int[3]{60, 250, 0});
    g_browser->column_char('|');

    g_statusBar = new Fl_Box(10, 470, 680, 20, "Ready.");
    g_statusBar->align(FL_ALIGN_LEFT | FL_ALIGN_INSIDE | FL_ALIGN_CLIP);
    
    update_browser();

    window->end();
    window->resizable(g_browser);
    window->show(argc, argv);

    return Fl::run();
}