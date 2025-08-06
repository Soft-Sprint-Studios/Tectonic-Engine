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
#include <FL/Fl_Input.H>
#include <FL/Fl_Float_Input.H>
#include <FL/Fl_Int_Input.H>
#include <FL/Fl_Button.H>
#include <FL/Fl_Menu_Bar.H>
#include <FL/Fl_File_Chooser.H>
#include <FL/fl_ask.H>
#include <FL/Fl_Box.H>
#include <FL/Fl_Tabs.H>
#include <FL/Fl_Group.H>
#include <FL/Fl_Check_Button.H>

#include <string>
#include <fstream>
#include <sstream>
#include <vector>
#include <cstring>
#include <algorithm>

struct Vec3_t { float x, y, z; };
struct Vec4_t { float r, g, b, a; };

struct ParticleProperties {
    int maxParticles;
    float spawnRate;
    float lifetime;
    float lifetimeVariation;
    float startSize;
    float endSize;
    float startAngle;
    float angleVariation;
    float startAngularVelocity;
    float angularVelocityVariation;
    std::string texture;
    Vec3_t gravity;
    Vec4_t startColor;
    Vec4_t endColor;
    Vec3_t startVelocity;
    Vec3_t velocityVariation;
    bool additiveBlending;
};

Fl_Window* g_mainWindow = nullptr;
Fl_Input* g_textureInput = nullptr;
Fl_Int_Input* g_maxParticlesInput = nullptr;
Fl_Float_Input* g_spawnRateInput = nullptr;
Fl_Float_Input* g_lifetimeInput = nullptr;
Fl_Float_Input* g_lifetimeVarInput = nullptr;
Fl_Float_Input* g_startSizeInput = nullptr;
Fl_Float_Input* g_endSizeInput = nullptr;
Fl_Float_Input* g_startAngleInput = nullptr;
Fl_Float_Input* g_angleVarInput = nullptr;
Fl_Float_Input* g_startAngVelInput = nullptr;
Fl_Float_Input* g_angVelVarInput = nullptr;
Fl_Float_Input* g_gravityXInput = nullptr;
Fl_Float_Input* g_gravityYInput = nullptr;
Fl_Float_Input* g_gravityZInput = nullptr;
Fl_Float_Input* g_startColorRInput = nullptr;
Fl_Float_Input* g_startColorGInput = nullptr;
Fl_Float_Input* g_startColorBInput = nullptr;
Fl_Float_Input* g_startColorAInput = nullptr;
Fl_Float_Input* g_endColorRInput = nullptr;
Fl_Float_Input* g_endColorGInput = nullptr;
Fl_Float_Input* g_endColorBInput = nullptr;
Fl_Float_Input* g_endColorAInput = nullptr;
Fl_Float_Input* g_startVelXInput = nullptr;
Fl_Float_Input* g_startVelYInput = nullptr;
Fl_Float_Input* g_startVelZInput = nullptr;
Fl_Float_Input* g_velVarXInput = nullptr;
Fl_Float_Input* g_velVarYInput = nullptr;
Fl_Float_Input* g_velVarZInput = nullptr;
Fl_Check_Button* g_additiveBlendCheck = nullptr;

ParticleProperties g_currentProps;
std::string g_currentFilePath = "";
bool g_isDirty = false;

void set_dirty(bool dirty) {
    g_isDirty = dirty;
    std::string title = "Tectonic Particle Editor";
    if (!g_currentFilePath.empty()) {
        title += " - " + g_currentFilePath;
    }
    if (g_isDirty) {
        title += " *";
    }
    g_mainWindow->label(title.c_str());
}

void mark_dirty_cb(Fl_Widget*, void*) {
    set_dirty(true);
}

void update_ui_from_props() {
    g_textureInput->value(g_currentProps.texture.c_str());
    
    static char buffer[32];
    snprintf(buffer, sizeof(buffer), "%d", g_currentProps.maxParticles); g_maxParticlesInput->value(buffer);
    snprintf(buffer, sizeof(buffer), "%.2f", g_currentProps.spawnRate); g_spawnRateInput->value(buffer);
    snprintf(buffer, sizeof(buffer), "%.2f", g_currentProps.lifetime); g_lifetimeInput->value(buffer);
    snprintf(buffer, sizeof(buffer), "%.2f", g_currentProps.lifetimeVariation); g_lifetimeVarInput->value(buffer);
    snprintf(buffer, sizeof(buffer), "%.2f", g_currentProps.startSize); g_startSizeInput->value(buffer);
    snprintf(buffer, sizeof(buffer), "%.2f", g_currentProps.endSize); g_endSizeInput->value(buffer);
    snprintf(buffer, sizeof(buffer), "%.2f", g_currentProps.startAngle); g_startAngleInput->value(buffer);
    snprintf(buffer, sizeof(buffer), "%.2f", g_currentProps.angleVariation); g_angleVarInput->value(buffer);
    snprintf(buffer, sizeof(buffer), "%.2f", g_currentProps.startAngularVelocity); g_startAngVelInput->value(buffer);
    snprintf(buffer, sizeof(buffer), "%.2f", g_currentProps.angularVelocityVariation); g_angVelVarInput->value(buffer);

    snprintf(buffer, sizeof(buffer), "%.2f", g_currentProps.gravity.x); g_gravityXInput->value(buffer);
    snprintf(buffer, sizeof(buffer), "%.2f", g_currentProps.gravity.y); g_gravityYInput->value(buffer);
    snprintf(buffer, sizeof(buffer), "%.2f", g_currentProps.gravity.z); g_gravityZInput->value(buffer);

    snprintf(buffer, sizeof(buffer), "%.2f", g_currentProps.startColor.r); g_startColorRInput->value(buffer);
    snprintf(buffer, sizeof(buffer), "%.2f", g_currentProps.startColor.g); g_startColorGInput->value(buffer);
    snprintf(buffer, sizeof(buffer), "%.2f", g_currentProps.startColor.b); g_startColorBInput->value(buffer);
    snprintf(buffer, sizeof(buffer), "%.2f", g_currentProps.startColor.a); g_startColorAInput->value(buffer);

    snprintf(buffer, sizeof(buffer), "%.2f", g_currentProps.endColor.r); g_endColorRInput->value(buffer);
    snprintf(buffer, sizeof(buffer), "%.2f", g_currentProps.endColor.g); g_endColorGInput->value(buffer);
    snprintf(buffer, sizeof(buffer), "%.2f", g_currentProps.endColor.b); g_endColorBInput->value(buffer);
    snprintf(buffer, sizeof(buffer), "%.2f", g_currentProps.endColor.a); g_endColorAInput->value(buffer);
    
    snprintf(buffer, sizeof(buffer), "%.2f", g_currentProps.startVelocity.x); g_startVelXInput->value(buffer);
    snprintf(buffer, sizeof(buffer), "%.2f", g_currentProps.startVelocity.y); g_startVelYInput->value(buffer);
    snprintf(buffer, sizeof(buffer), "%.2f", g_currentProps.startVelocity.z); g_startVelZInput->value(buffer);

    snprintf(buffer, sizeof(buffer), "%.2f", g_currentProps.velocityVariation.x); g_velVarXInput->value(buffer);
    snprintf(buffer, sizeof(buffer), "%.2f", g_currentProps.velocityVariation.y); g_velVarYInput->value(buffer);
    snprintf(buffer, sizeof(buffer), "%.2f", g_currentProps.velocityVariation.z); g_velVarZInput->value(buffer);
    
    g_additiveBlendCheck->value(g_currentProps.additiveBlending);

    set_dirty(false);
}

void update_props_from_ui() {
    g_currentProps.texture = g_textureInput->value();
    g_currentProps.maxParticles = atoi(g_maxParticlesInput->value());
    g_currentProps.spawnRate = atof(g_spawnRateInput->value());
    g_currentProps.lifetime = atof(g_lifetimeInput->value());
    g_currentProps.lifetimeVariation = atof(g_lifetimeVarInput->value());
    g_currentProps.startSize = atof(g_startSizeInput->value());
    g_currentProps.endSize = atof(g_endSizeInput->value());
    g_currentProps.startAngle = atof(g_startAngleInput->value());
    g_currentProps.angleVariation = atof(g_angleVarInput->value());
    g_currentProps.startAngularVelocity = atof(g_startAngVelInput->value());
    g_currentProps.angularVelocityVariation = atof(g_angVelVarInput->value());

    g_currentProps.gravity.x = atof(g_gravityXInput->value());
    g_currentProps.gravity.y = atof(g_gravityYInput->value());
    g_currentProps.gravity.z = atof(g_gravityZInput->value());

    g_currentProps.startColor.r = atof(g_startColorRInput->value());
    g_currentProps.startColor.g = atof(g_startColorGInput->value());
    g_currentProps.startColor.b = atof(g_startColorBInput->value());
    g_currentProps.startColor.a = atof(g_startColorAInput->value());

    g_currentProps.endColor.r = atof(g_endColorRInput->value());
    g_currentProps.endColor.g = atof(g_endColorGInput->value());
    g_currentProps.endColor.b = atof(g_endColorBInput->value());
    g_currentProps.endColor.a = atof(g_endColorAInput->value());

    g_currentProps.startVelocity.x = atof(g_startVelXInput->value());
    g_currentProps.startVelocity.y = atof(g_startVelYInput->value());
    g_currentProps.startVelocity.z = atof(g_startVelZInput->value());
    
    g_currentProps.velocityVariation.x = atof(g_velVarXInput->value());
    g_currentProps.velocityVariation.y = atof(g_velVarYInput->value());
    g_currentProps.velocityVariation.z = atof(g_velVarZInput->value());
    
    g_currentProps.additiveBlending = g_additiveBlendCheck->value();
}

void new_file() {
    g_currentProps = {};
    g_currentProps.maxParticles = 1000;
    g_currentProps.spawnRate = 100.0f;
    g_currentProps.lifetime = 2.0f;
    g_currentProps.startColor = {1.0f, 1.0f, 1.0f, 1.0f};
    g_currentProps.endColor = {1.0f, 1.0f, 1.0f, 0.0f};
    g_currentProps.startSize = 0.5f;
    g_currentProps.endSize = 0.1f;
    g_currentProps.gravity = {0.0f, -9.81f, 0.0f};
    g_currentFilePath = "";
    update_ui_from_props();
}

bool save_file(const std::string& path) {
    update_props_from_ui();

    std::ofstream file(path);
    if (!file.is_open()) {
        fl_alert("Error: Could not open file for writing:\n%s", path.c_str());
        return false;
    }

    file << "maxParticles " << g_currentProps.maxParticles << "\n";
    file << "spawnRate " << g_currentProps.spawnRate << "\n";
    file << "lifetime " << g_currentProps.lifetime << "\n";
    file << "lifetimeVariation " << g_currentProps.lifetimeVariation << "\n";
    file << "startSize " << g_currentProps.startSize << "\n";
    file << "endSize " << g_currentProps.endSize << "\n";
    file << "startAngle " << g_currentProps.startAngle << "\n";
    file << "angleVariation " << g_currentProps.angleVariation << "\n";
    file << "startAngularVelocity " << g_currentProps.startAngularVelocity << "\n";
    file << "angularVelocityVariation " << g_currentProps.angularVelocityVariation << "\n";
    file << "texture " << g_currentProps.texture << "\n";
    file << "gravity " << g_currentProps.gravity.x << "," << g_currentProps.gravity.y << "," << g_currentProps.gravity.z << "\n";
    file << "startColor " << g_currentProps.startColor.r << "," << g_currentProps.startColor.g << "," << g_currentProps.startColor.b << "," << g_currentProps.startColor.a << "\n";
    file << "endColor " << g_currentProps.endColor.r << "," << g_currentProps.endColor.g << "," << g_currentProps.endColor.b << "," << g_currentProps.endColor.a << "\n";
    file << "startVelocity " << g_currentProps.startVelocity.x << "," << g_currentProps.startVelocity.y << "," << g_currentProps.startVelocity.z << "\n";
    file << "velocityVariation " << g_currentProps.velocityVariation.x << "," << g_currentProps.velocityVariation.y << "," << g_currentProps.velocityVariation.z << "\n";
    if (g_currentProps.additiveBlending) {
        file << "blendFunc additive\n";
    }

    file.close();
    g_currentFilePath = path;
    set_dirty(false);
    return true;
}

void open_file(const std::string& path) {
    std::ifstream file(path);
    if (!file.is_open()) {
        fl_alert("Error: Could not open file:\n%s", path.c_str());
        return;
    }

    new_file();

    std::string line;
    while (std::getline(file, line)) {
        std::stringstream ss(line);
        std::string key;
        ss >> key;

        if (key == "maxParticles") ss >> g_currentProps.maxParticles;
        else if (key == "spawnRate") ss >> g_currentProps.spawnRate;
        else if (key == "lifetime") ss >> g_currentProps.lifetime;
        else if (key == "lifetimeVariation") ss >> g_currentProps.lifetimeVariation;
        else if (key == "startSize") ss >> g_currentProps.startSize;
        else if (key == "endSize") ss >> g_currentProps.endSize;
        else if (key == "startAngle") ss >> g_currentProps.startAngle;
        else if (key == "angleVariation") ss >> g_currentProps.angleVariation;
        else if (key == "startAngularVelocity") ss >> g_currentProps.startAngularVelocity;
        else if (key == "angularVelocityVariation") ss >> g_currentProps.angularVelocityVariation;
        else if (key == "texture") ss >> g_currentProps.texture;
        else if (key == "gravity") {
            char comma;
            ss >> g_currentProps.gravity.x >> comma >> g_currentProps.gravity.y >> comma >> g_currentProps.gravity.z;
        } else if (key == "startColor") {
            char comma;
            ss >> g_currentProps.startColor.r >> comma >> g_currentProps.startColor.g >> comma >> g_currentProps.startColor.b >> comma >> g_currentProps.startColor.a;
        } else if (key == "endColor") {
            char comma;
            ss >> g_currentProps.endColor.r >> comma >> g_currentProps.endColor.g >> comma >> g_currentProps.endColor.b >> comma >> g_currentProps.endColor.a;
        } else if (key == "startVelocity") {
            char comma;
            ss >> g_currentProps.startVelocity.x >> comma >> g_currentProps.startVelocity.y >> comma >> g_currentProps.startVelocity.z;
        } else if (key == "velocityVariation") {
            char comma;
            ss >> g_currentProps.velocityVariation.x >> comma >> g_currentProps.velocityVariation.y >> comma >> g_currentProps.velocityVariation.z;
        } else if (key == "blendFunc") {
            std::string blendMode;
            ss >> blendMode;
            if (blendMode == "additive") {
                g_currentProps.additiveBlending = true;
            }
        }
    }
    file.close();
    g_currentFilePath = path;
    update_ui_from_props();
}

void on_new_cb(Fl_Widget*, void*) {
    if (g_isDirty) {
        int choice = fl_choice("You have unsaved changes. Do you want to save first?", "Save", "Discard", "Cancel");
        if (choice == 2) return;
        if (choice == 0) {
            if (g_currentFilePath.empty()) {
                Fl_File_Chooser chooser(".", "*.par", Fl_File_Chooser::CREATE, "Save Particle File");
                chooser.show();
                while (chooser.shown()) { Fl::wait(); }
                if (chooser.value()) {
                    if (!save_file(chooser.value())) return;
                } else {
                    return;
                }
            } else {
                if (!save_file(g_currentFilePath)) return;
            }
        }
    }
    new_file();
}

void on_save_as_cb(Fl_Widget*, void*) {
    Fl_File_Chooser chooser(".", "*.par", Fl_File_Chooser::CREATE, "Save Particle File As");
    chooser.show();
    while (chooser.shown()) { Fl::wait(); }
    if (chooser.value()) {
        save_file(chooser.value());
    }
}

void on_save_cb(Fl_Widget*, void*) {
    if (g_currentFilePath.empty()) {
        on_save_as_cb(nullptr, nullptr);
    }
    else {
        save_file(g_currentFilePath);
    }
}

void on_open_cb(Fl_Widget*, void*) {
     if (g_isDirty) {
        int choice = fl_choice("You have unsaved changes. Do you want to save first?", "Save", "Discard", "Cancel");
        if (choice == 2) return;
        if (choice == 0) { on_save_cb(nullptr, nullptr); }
    }
    Fl_File_Chooser chooser(".", "*.par", Fl_File_Chooser::SINGLE, "Open Particle File");
    chooser.show();
    while(chooser.shown()) { Fl::wait(); }
    if (chooser.value()) {
        open_file(chooser.value());
    }
}

void on_quit_cb(Fl_Widget*, void*) {
    if (g_isDirty) {
        int choice = fl_choice("You have unsaved changes. Quit without saving?", "Quit", "Save and Quit", "Cancel");
        if (choice == 2) {
            return;
        }
        if (choice == 1) {
            if (g_currentFilePath.empty()) {
                Fl_File_Chooser chooser(".", "*.par", Fl_File_Chooser::CREATE, "Save Particle File");
                chooser.show();
                while (chooser.shown()) { Fl::wait(); }
                if (chooser.value()) {
                    if (!save_file(chooser.value())) return;
                }
                else {
                    return;
                }
            }
            else {
                if (!save_file(g_currentFilePath)) return;
            }
        }
    }
    exit(0);
}

void on_about_cb(Fl_Widget*, void*) {
    fl_message_title("About Tectonic Particle Editor");
    fl_message("A tool to create and edit .par files for the Tectonic Engine.\n\nCopyright (c) 2025 Soft Sprint Studios");
}

int main(int argc, char** argv) {
    g_mainWindow = new Fl_Window(500, 480, "Tectonic Particle Editor");
    g_mainWindow->callback((Fl_Callback*)on_quit_cb);
    
    Fl_Menu_Bar* menu = new Fl_Menu_Bar(0, 0, 500, 25);
    menu->add("File/New", FL_CTRL + 'n', on_new_cb);
    menu->add("File/Open", FL_CTRL + 'o', on_open_cb);
    menu->add("File/Save", FL_CTRL + 's', on_save_cb);
    menu->add("File/Save As", FL_CTRL + FL_SHIFT + 's', on_save_as_cb);
    menu->add("File/Quit", FL_CTRL + 'q', (Fl_Callback*)on_quit_cb);
    menu->add("Help/About", 0, on_about_cb);

    Fl_Tabs* tabs = new Fl_Tabs(10, 35, 480, 435);
    {
        Fl_Group* generalGroup = new Fl_Group(10, 60, 480, 410, "General");
        {
            g_textureInput = new Fl_Input(150, 75, 330, 25, "Texture");
            g_maxParticlesInput = new Fl_Int_Input(150, 105, 100, 25, "Max Particles");
            g_spawnRateInput = new Fl_Float_Input(150, 135, 100, 25, "Spawn Rate");
            g_additiveBlendCheck = new Fl_Check_Button(150, 165, 150, 25, "Additive Blending");
        }
        generalGroup->end();

        Fl_Group* lifetimeGroup = new Fl_Group(10, 60, 480, 410, "Lifetime & Size");
        {
            g_lifetimeInput = new Fl_Float_Input(150, 75, 100, 25, "Lifetime");
            g_lifetimeVarInput = new Fl_Float_Input(150, 105, 100, 25, "Lifetime Variation");
            g_startSizeInput = new Fl_Float_Input(150, 135, 100, 25, "Start Size");
            g_endSizeInput = new Fl_Float_Input(150, 165, 100, 25, "End Size");
        }
        lifetimeGroup->end();

        Fl_Group* physicsGroup = new Fl_Group(10, 60, 480, 410, "Physics & Motion");
        {
            g_gravityXInput = new Fl_Float_Input(150, 75, 80, 25, "Gravity X");
            g_gravityYInput = new Fl_Float_Input(150, 105, 80, 25, "Gravity Y");
            g_gravityZInput = new Fl_Float_Input(150, 135, 80, 25, "Gravity Z");
            
            g_startVelXInput = new Fl_Float_Input(350, 75, 80, 25, "Start Velocity X");
            g_startVelYInput = new Fl_Float_Input(350, 105, 80, 25, "Start Velocity Y");
            g_startVelZInput = new Fl_Float_Input(350, 135, 80, 25, "Start Velocity Z");

            g_velVarXInput = new Fl_Float_Input(150, 180, 80, 25, "Velocity Var. X");
            g_velVarYInput = new Fl_Float_Input(150, 210, 80, 25, "Velocity Var. Y");
            g_velVarZInput = new Fl_Float_Input(150, 240, 80, 25, "Velocity Var. Z");
        }
        physicsGroup->end();

        Fl_Group* colorGroup = new Fl_Group(10, 60, 480, 410, "Color");
        {
            g_startColorRInput = new Fl_Float_Input(150, 75, 80, 25, "Start Color R");
            g_startColorGInput = new Fl_Float_Input(150, 105, 80, 25, "Start Color G");
            g_startColorBInput = new Fl_Float_Input(150, 135, 80, 25, "Start Color B");
            g_startColorAInput = new Fl_Float_Input(150, 165, 80, 25, "Start Color A");

            g_endColorRInput = new Fl_Float_Input(350, 75, 80, 25, "End Color R");
            g_endColorGInput = new Fl_Float_Input(350, 105, 80, 25, "End Color G");
            g_endColorBInput = new Fl_Float_Input(350, 135, 80, 25, "End Color B");
            g_endColorAInput = new Fl_Float_Input(350, 165, 80, 25, "End Color A");
        }
        colorGroup->end();
        
        Fl_Group* rotationGroup = new Fl_Group(10, 60, 480, 410, "Rotation");
        {
            g_startAngleInput = new Fl_Float_Input(150, 75, 100, 25, "Start Angle");
            g_angleVarInput = new Fl_Float_Input(150, 105, 100, 25, "Angle Variation");
            g_startAngVelInput = new Fl_Float_Input(150, 135, 100, 25, "Start Ang. Vel.");
            g_angVelVarInput = new Fl_Float_Input(150, 165, 100, 25, "Ang. Vel. Var.");
        }
        rotationGroup->end();
    }
    tabs->end();

    g_textureInput->callback(mark_dirty_cb);
    g_maxParticlesInput->callback(mark_dirty_cb);
    g_spawnRateInput->callback(mark_dirty_cb);
    g_lifetimeInput->callback(mark_dirty_cb);
    g_lifetimeVarInput->callback(mark_dirty_cb);
    g_startSizeInput->callback(mark_dirty_cb);
    g_endSizeInput->callback(mark_dirty_cb);
    g_startAngleInput->callback(mark_dirty_cb);
    g_angleVarInput->callback(mark_dirty_cb);
    g_startAngVelInput->callback(mark_dirty_cb);
    g_angVelVarInput->callback(mark_dirty_cb);
    g_gravityXInput->callback(mark_dirty_cb);
    g_gravityYInput->callback(mark_dirty_cb);
    g_gravityZInput->callback(mark_dirty_cb);
    g_startColorRInput->callback(mark_dirty_cb);
    g_startColorGInput->callback(mark_dirty_cb);
    g_startColorBInput->callback(mark_dirty_cb);
    g_startColorAInput->callback(mark_dirty_cb);
    g_endColorRInput->callback(mark_dirty_cb);
    g_endColorGInput->callback(mark_dirty_cb);
    g_endColorBInput->callback(mark_dirty_cb);
    g_endColorAInput->callback(mark_dirty_cb);
    g_startVelXInput->callback(mark_dirty_cb);
    g_startVelYInput->callback(mark_dirty_cb);
    g_startVelZInput->callback(mark_dirty_cb);
    g_velVarXInput->callback(mark_dirty_cb);
    g_velVarYInput->callback(mark_dirty_cb);
    g_velVarZInput->callback(mark_dirty_cb);
    g_additiveBlendCheck->callback(mark_dirty_cb);


    g_mainWindow->end();
    g_mainWindow->show(argc, argv);

    new_file();

    return Fl::run();
}