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
#include <FL/Fl_Box.H>
#include <FL/fl_ask.H>

#include <string>
#include <cstdlib>

#ifdef _WIN32
#include <windows.h>
#else
#include <unistd.h>
#include <sys/wait.h>
#include <iostream>
#endif

void launch_tool(const char* tool_executable) {
#ifdef _WIN32
    STARTUPINFOA si;
    PROCESS_INFORMATION pi;
    ZeroMemory(&si, sizeof(si));
    si.cb = sizeof(si);
    ZeroMemory(&pi, sizeof(pi));

    char cmdLine[256];
    strncpy_s(cmdLine, tool_executable, sizeof(cmdLine) - 1);
    cmdLine[sizeof(cmdLine) - 1] = '\0';

    if (!CreateProcessA(NULL, cmdLine, NULL, NULL, FALSE, 0, NULL, NULL, &si, &pi)) {
        std::string error_msg = "Failed to launch '";
        error_msg += tool_executable;
        error_msg += "'.\nEnsure it is in the same directory as the launcher.";
        fl_alert(error_msg.c_str());
        return;
    }
    CloseHandle(pi.hProcess);
    CloseHandle(pi.hThread);
#else
    pid_t pid = fork();
    if (pid == 0) {
        char path_buffer[512];
        snprintf(path_buffer, sizeof(path_buffer), "./%s", tool_executable);
        execlp(path_buffer, tool_executable, (char*)NULL);
        perror("execlp");
        _exit(1); 
    } else if (pid < 0) {
        std::string error_msg = "Failed to fork process for '";
        error_msg += tool_executable;
        error_msg += "'.";
        fl_alert(error_msg.c_str());
    }
#endif
}

void on_launch_console_cb(Fl_Widget*, void*) {
#ifdef _WIN32
    launch_tool("TConsole.exe");
#else
    launch_tool("TConsole");
#endif
}

void on_launch_model_importer_cb(Fl_Widget*, void*) {
#ifdef _WIN32
    launch_tool("TectonicModelImporter.exe");
#else
    launch_tool("TectonicModelImporter");
#endif
}

void on_launch_particle_editor_cb(Fl_Widget*, void*) {
#ifdef _WIN32
    launch_tool("TectonicParticleEditor.exe");
#else
    launch_tool("TectonicParticleEditor");
#endif
}

void on_quit_cb(Fl_Widget* w, void*) {
    Fl_Window* win = (Fl_Window*)w->window();
    win->hide();
}

int main(int argc, char** argv) {
    const int win_w = 320;
    const int win_h = 240;

    Fl_Window* window = new Fl_Window(win_w, win_h, "Tectonic SDK Launcher");
    window->begin();

    Fl_Box* title = new Fl_Box(0, 20, win_w, 40, "Tectonic SDK");
    title->labelsize(24);
    title->labelfont(FL_BOLD);

    Fl_Button* btn_console = new Fl_Button(30, 80, win_w - 60, 30, "Tectonic Console");
    btn_console->callback(on_launch_console_cb);

    Fl_Button* btn_model_importer = new Fl_Button(30, 120, win_w - 60, 30, "Model Importer");
    btn_model_importer->callback(on_launch_model_importer_cb);

    Fl_Button* btn_particle_editor = new Fl_Button(30, 160, win_w - 60, 30, "Particle Editor");
    btn_particle_editor->callback(on_launch_particle_editor_cb);

    Fl_Button* btn_quit = new Fl_Button(win_w - 90, win_h - 40, 80, 25, "Quit");
    btn_quit->callback(on_quit_cb);
    
    window->end();
    window->show(argc, argv);

    return Fl::run();
}