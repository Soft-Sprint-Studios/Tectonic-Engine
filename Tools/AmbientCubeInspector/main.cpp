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

struct Vec3 {
    float x, y, z;
};

struct AmbientProbe {
    Vec3 position;
    Vec3 colors[6];
    Vec3 dominant_direction;
};

std::vector<AmbientProbe> g_probes;
Fl_Browser* g_browser = nullptr;
Fl_Box* g_statusBar = nullptr;

void update_browser() {
    g_browser->clear();
    g_browser->add("@B|Idx|@B| Position (X,Y,Z)  |@B| Colors (+X,-X,+Y,-Y,+Z,-Z) |@B| Dominant Dir (X,Y,Z)");

    for (size_t i = 0; i < g_probes.size(); ++i) {
        const auto& probe = g_probes[i];
        std::stringstream ss;
        ss << std::fixed << std::setprecision(2);
        
        ss << i << "\t| ";
        ss << "(" << probe.position.x << ", " << probe.position.y << ", " << probe.position.z << ")\t| ";
        
        ss << "[";
        for(int j=0; j<6; ++j) {
            ss << "(" << probe.colors[j].x << "," << probe.colors[j].y << "," << probe.colors[j].z << ")" << (j < 5 ? ";" : "");
        }
        ss << "]\t| ";

        ss << "(" << probe.dominant_direction.x << ", " << probe.dominant_direction.y << ", " << probe.dominant_direction.z << ")";
        
        g_browser->add(ss.str().c_str());
    }
}

void load_amp_file(const char* path) {
    std::ifstream file(path, std::ios::binary);
    if (!file) {
        fl_alert("Error: Could not open file: %s", path);
        return;
    }

    char header[5] = {0};
    file.read(header, 4);

    if (strcmp(header, "AMBI") != 0) {
        fl_alert("Error: Invalid .amp file format for %s", path);
        return;
    }

    int count = 0;
    file.read(reinterpret_cast<char*>(&count), sizeof(int));

    g_probes.resize(count);
    file.read(reinterpret_cast<char*>(g_probes.data()), sizeof(AmbientProbe) * count);

    std::stringstream status;
    status << "Loaded " << count << " ambient probes from " << path;
    g_statusBar->label(status.str().c_str());
    update_browser();
}

void on_open_cb(Fl_Widget*, void*) {
    Fl_File_Chooser chooser(".", "*.amp", Fl_File_Chooser::SINGLE, "Open Ambient Probes File");
    chooser.show();
    while(chooser.shown()) { Fl::wait(); }
    if (chooser.value()) {
        g_probes.clear();
        load_amp_file(chooser.value());
    }
}

void on_quit_cb(Fl_Widget*, void*) {
    exit(0);
}

void on_about_cb(Fl_Widget*, void*) {
    fl_message_title("About Tectonic Ambient Cube Inspector");
    fl_message("A simple tool to inspect ambient probe data (.amp) for the Tectonic Engine.\n\n"
               "Copyright (c) 2025 Soft Sprint Studios");
}

int main(int argc, char **argv) {
    Fl_Window *window = new Fl_Window(800, 600, "Tectonic Ambient Cube Inspector");

    Fl_Menu_Bar* menu = new Fl_Menu_Bar(0, 0, 800, 25);
    menu->add("File/Open...", FL_CTRL + 'o', on_open_cb);
    menu->add("File/Quit", FL_CTRL + 'q', on_quit_cb);
    menu->add("Help/About", 0, on_about_cb);
    
    g_browser = new Fl_Browser(10, 35, 780, 530);
    g_browser->type(FL_MULTI_BROWSER);
    g_browser->column_widths(new int[4]{40, 180, 320, 0});
    g_browser->column_char('|');

    g_statusBar = new Fl_Box(10, 570, 780, 20, "Ready. Open an .amp file to begin.");
    g_statusBar->align(FL_ALIGN_LEFT | FL_ALIGN_INSIDE | FL_ALIGN_CLIP);
    
    update_browser();

    window->end();
    window->resizable(g_browser);
    window->show(argc, argv);

    return Fl::run();
}