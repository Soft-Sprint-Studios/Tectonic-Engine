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
#include "imgui.h"
#include "imgui_impl_sdl2.h"
#include "imgui_impl_opengl3.h"
#include "imgui_internal.h"

#include <stdio.h>
#include <vector>
#include <string>
#include <cstdarg>
#include <stdint.h>
#include <time.h>
#include <algorithm>

#include "gl_console.h"
#include "ipc_system.h"
#include "cvar.h"
#include "commands.h"

static Bool show_console = false;
static command_callback_t command_handler = nullptr;
static FILE* g_log_file = nullptr;

struct Console {
    Char                  InputBuf[256];
    vector<ConsoleItem> Items;
    Bool                  ScrollToBottom;
    vector<const Char*> Candidates;
    Int                   CandidatePos;
    Bool                  CandidateListOpen;
    Bool                  ReclaimFocus;

    Console() {
        ClearLog();
        memset(InputBuf, 0, sizeof(InputBuf));
        ScrollToBottom = true;
        CandidatePos = 0;
        CandidateListOpen = false;
        ReclaimFocus = false;
    }
    ~Console() { ClearLog(); }
    void ClearLog() { for (Int i = 0; i < Items.size(); i++) delete[] Items[i].text; Items.clear(); }

    void AddLog(ConsoleTextColor color, const Char* fmt, va_list args) {
        Char buf[1024];
        vsnprintf(buf, IM_ARRAYSIZE(buf), fmt, args);
        buf[IM_ARRAYSIZE(buf) - 1] = 0;

        IPC_SendMessage(buf);

        if (g_log_file) {
            fprintf(g_log_file, "%s\n", buf);
            fflush(g_log_file);
        }

        ConsoleItem item;
        item.text = new Char[strlen(buf) + 1];
        strcpy(item.text, buf);
        item.color = color;
        Items.push_back(item);
        ScrollToBottom = true;
    }

    static Int TextEditCallback(ImGuiInputTextCallbackData* data) {
        Console* console = (Console*)data->UserData;

        if (data->EventFlag == ImGuiInputTextFlags_CallbackCompletion) {
            if (console->CandidateListOpen && !console->Candidates.empty()) {
                const Char* selected = console->Candidates[console->CandidatePos];
                data->DeleteChars(0, data->BufTextLen);
                data->InsertChars(0, selected);
                data->InsertChars(data->CursorPos, " ");
                console->CandidateListOpen = false;
            }
        }
        else if (data->EventFlag == ImGuiInputTextFlags_CallbackHistory) {
            if (console->CandidateListOpen && !console->Candidates.empty()) {
                if (data->EventKey == ImGuiKey_UpArrow) {
                    if (console->CandidatePos > 0)
                        console->CandidatePos--;
                    else
                        console->CandidatePos = (Int)console->Candidates.size() - 1;
                }
                else if (data->EventKey == ImGuiKey_DownArrow) {
                    if (console->CandidatePos < (Int)console->Candidates.size() - 1)
                        console->CandidatePos++;
                    else
                        console->CandidatePos = 0;
                }
            }
        }
        return 0;
    }

    void UpdateCandidates() {
        Candidates.clear();
        if (InputBuf[0] == 0) {
            CandidateListOpen = false;
            return;
        }

        for (Int i = 0; i < Commands_GetCount(); i++) {
            const Command* cmd = Commands_GetCommand(i);
            if (_strnicmp(cmd->name, InputBuf, strlen(InputBuf)) == 0) {
                Candidates.push_back(cmd->name);
            }
        }

        for (Int i = 0; i < Cvar_GetCount(); i++) {
            const Cvar* cvar = Cvar_GetCvar(i);
            if (_strnicmp(cvar->name, InputBuf, strlen(InputBuf)) == 0) {
                Candidates.push_back(cvar->name);
            }
        }

        sort(Candidates.begin(), Candidates.end(), [](const Char* a, const Char* b) {
            return strcmp(a, b) < 0;
            });

        if (!Candidates.empty()) {
            CandidateListOpen = true;
            CandidatePos = 0;
        }
        else {
            CandidateListOpen = false;
        }
    }

    void Draw() {
        if (!show_console) return;
        ImGui::SetNextWindowSize(ImVec2(520, 600), ImGuiCond_FirstUseEver);
        if (!ImGui::Begin("Console", &show_console)) { ImGui::End(); return; }
        const Float footer_height_to_reserve = ImGui::GetStyle().ItemSpacing.y + ImGui::GetFrameHeightWithSpacing();
        ImGui::BeginChild("ScrollingRegion", ImVec2(0, -footer_height_to_reserve), false, ImGuiWindowFlags_HorizontalScrollbar);
        for (Int i = 0; i < Items.size(); i++) {
            const ConsoleItem& item = Items[i];
            ImVec4 color;
            Bool has_color = false;
            if (item.color == CONSOLE_COLOR_RED) {
                color = ImVec4(1.0f, 0.4f, 0.4f, 1.0f);
                has_color = true;
            }
            else if (item.color == CONSOLE_COLOR_YELLOW) {
                color = ImVec4(1.0f, 1.0f, 0.4f, 1.0f);
                has_color = true;
            }

            if (has_color) ImGui::PushStyleColor(ImGuiCol_Text, color);
            ImGui::TextUnformatted(item.text);
            if (has_color) ImGui::PopStyleColor();
        }
        if (ScrollToBottom) ImGui::SetScrollY(ImGui::GetScrollMaxY());
        ScrollToBottom = false;
        ImGui::EndChild();
        ImGui::Separator();

        if (ReclaimFocus) {
            ImGui::SetKeyboardFocusHere();
            ReclaimFocus = false;
        }

        ImVec2 input_pos = ImGui::GetCursorScreenPos();
        Float input_width = ImGui::GetContentRegionAvail().x - 60;
        ImGui::SetNextItemWidth(input_width);

        if (ImGui::InputText("##Input", InputBuf, IM_ARRAYSIZE(InputBuf), ImGuiInputTextFlags_EnterReturnsTrue | ImGuiInputTextFlags_CallbackCompletion | ImGuiInputTextFlags_CallbackHistory, &TextEditCallback, (void*)this)) {
            Char* s = InputBuf;
            if (s[0]) ExecCommand(s);
            strcpy(s, "");
            ReclaimFocus = true;
            CandidateListOpen = false;
        }

        if (ImGui::IsItemEdited()) {
            UpdateCandidates();
        }

        if (CandidateListOpen && !Candidates.empty()) {
            Float popup_height = (min((Int)Candidates.size(), 7) * ImGui::GetTextLineHeightWithSpacing()) + 10;
            ImGui::SetNextWindowPos(ImVec2(input_pos.x, input_pos.y - popup_height));
            ImGui::SetNextWindowSize(ImVec2(input_width, 0.0f));

            ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
            if (ImGui::Begin("##Suggestions", nullptr, ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoFocusOnAppearing | ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_AlwaysAutoResize))
            {
                for (Int i = 0; i < Candidates.size(); i++) {
                    Bool is_selected = (CandidatePos == i);
                    if (ImGui::Selectable(Candidates[i], is_selected)) {
                        strncpy(InputBuf, Candidates[i], sizeof(InputBuf) - 1);
                        strncat(InputBuf, " ", sizeof(InputBuf) - strlen(InputBuf) - 1);
                        CandidateListOpen = false;
                        ReclaimFocus = true;
                    }
                    if (is_selected) {
                        ImGui::SetItemDefaultFocus();
                    }
                }
            }
            ImGui::End();
            ImGui::PopStyleVar();
        }
        ImGui::SameLine();
        if (ImGui::Button("Submit")) {
            Char* s = InputBuf;
            if (s[0]) ExecCommand(s);
            strcpy(s, "");
            ReclaimFocus = true;
            CandidateListOpen = false;
        }
        ImGui::End();
    }
    void ExecCommand(const Char* command_line) {
        Console_Printf("# %s", command_line);
        if (command_handler) {
            Char* cmd_copy = new Char[strlen(command_line) + 1];
            strcpy(cmd_copy, command_line);
            const Int MAX_ARGS = 16; Int argc = 0; Char* argv[MAX_ARGS];
            Char* p = strtok(cmd_copy, " ");
            while (p != nullptr && argc < MAX_ARGS) { argv[argc++] = p; p = strtok(nullptr, " "); }
            if (argc > 0) command_handler(argc, argv); delete[] cmd_copy;
        }
    }
};

static Console console_instance;

    void Console_Toggle() { show_console = !show_console; }
    Bool Console_IsVisible() { return show_console; }
    void Console_Draw() { console_instance.Draw(); }
    void Console_SetCommandHandler(command_callback_t handler) { command_handler = handler; }

    void Log_Init(const Char* filename) {
        if (g_log_file) {
            fclose(g_log_file);
        }
        g_log_file = fopen(filename, "w");
        if (!g_log_file) {
            printf("[ERROR] Failed to open log file: %s\n", filename);
        }
        else {
            time_t now = time(nullptr);
            fprintf(g_log_file, "Log started at %s\n", ctime(&now));
            fflush(g_log_file);
        }
    }

    void Log_Shutdown(void) {
        if (g_log_file) {
            time_t now = time(nullptr);
            fprintf(g_log_file, "\nLog ended at %s\n", ctime(&now));
            fclose(g_log_file);
            g_log_file = nullptr;
        }
    }

    void Console_Printf(const Char* fmt, ...) {
        va_list args;
        va_start(args, fmt);
        console_instance.AddLog(CONSOLE_COLOR_WHITE, fmt, args);
        va_end(args);
    }

    void Console_Printf_Error(const Char* fmt, ...) {
        Char final_fmt[2048];
        snprintf(final_fmt, sizeof(final_fmt), "[ERROR] %s", fmt);

        va_list args;
        va_start(args, fmt);
        console_instance.AddLog(CONSOLE_COLOR_RED, final_fmt, args);
        va_end(args);
    }

    void Console_Printf_Warning(const Char* fmt, ...) {
        Char final_fmt[2048];
        snprintf(final_fmt, sizeof(final_fmt), "[WARNING] %s", fmt);

        va_list args;
        va_start(args, fmt);
        console_instance.AddLog(CONSOLE_COLOR_YELLOW, final_fmt, args);
        va_end(args);
    }

    void Console_ClearLog() {
        console_instance.ClearLog();
    }

    const ConsoleItem* Console_GetLogItems(Int* count) {
        if (count) {
            *count = console_instance.Items.size();
        }
        if (console_instance.Items.empty()) {
            return nullptr;
        }
        return console_instance.Items.data();
    }

    void UI_RenderGameText(Int num_messages, const Char* texts[4], const Float positions_x[4], const Float positions_y[4], const Vec4 colors[4], const Float alphas[4], const Int states[4], const Float scales[4]) {
        ImGuiIO& io = ImGui::GetIO();
        ImDrawList* draw_list = ImGui::GetForegroundDrawList();

        for (Int i = 0; i < num_messages; ++i) {
            if (states[i] == 0 || alphas[i] <= 0.0f) {
                continue;
            }

            ImFont* font = io.Fonts->Fonts[0];
            Float base_font_size = ImGui::GetFontSize();
            Float current_scale = scales[i] > 0.0f ? scales[i] : 1.0f;
            Float scaled_font_size = base_font_size * current_scale;

            ImVec2 text_size = font->CalcTextSizeA(scaled_font_size, FLT_MAX, 0.0f, texts[i]);

            Float pos_x, pos_y;

            if (positions_x[i] == -1.0f) {
                pos_x = (io.DisplaySize.x - text_size.x) * 0.5f;
            }
            else {
                pos_x = io.DisplaySize.x * positions_x[i];
            }

            if (positions_y[i] == -1.0f) {
                pos_y = (io.DisplaySize.y - text_size.y) * 0.5f;
            }
            else {
                pos_y = io.DisplaySize.y * positions_y[i];
            }

            ImU32 color = IM_COL32(colors[i].x * 255, colors[i].y * 255, colors[i].z * 255, alphas[i] * 255);
            draw_list->AddText(font, scaled_font_size, ImVec2(pos_x, pos_y), color, texts[i]);
        }
    }

    void UI_RenderGameHUD(Float fps, Float px, Float py, Float pz, Float health, Bool canUse, Float radiation, Float rads_per_second, const Float* fps_history, Int history_size) {
        Bool show_fps = Cvar_GetInt("show_fps");
        Bool show_pos = Cvar_GetInt("show_pos");
        Bool show_health = Cvar_GetInt("show_health");
        Bool show_crosshair = Cvar_GetInt("crosshair");
        Bool show_graph = Cvar_GetInt("r_showgraph");
        Bool show_watermark = Cvar_GetInt("watermark");

        constexpr Float DISTANCE = 10.0f;
        ImVec2 window_pos = ImVec2(DISTANCE, DISTANCE);
        ImVec2 window_pos_pivot = ImVec2(0.0f, 0.0f);
        ImGui::SetNextWindowPos(window_pos, ImGuiCond_Always, window_pos_pivot);
        ImGui::SetNextWindowBgAlpha(0.35f);

        ImGuiWindowFlags window_flags = ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoFocusOnAppearing | ImGuiWindowFlags_NoNav | ImGuiWindowFlags_NoMove;

        if (show_fps || show_pos || health > 0) {
            if (ImGui::Begin("GameHUD", nullptr, window_flags)) {
                if (show_fps) {
                    ImGui::Text("FPS: %.1f", fps);
                }
                if (show_pos) {
                    ImGui::Text("Pos: %.2f, %.2f, %.2f", px, py, pz);
                }
                if (show_health) {
                    ImGui::Text("Health: %.0f", health);
                }
                if (radiation > 0.1f) {
                    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.6f, 1.0f, 0.6f, 1.0f));
                    ImGui::Text("RAD: %.1f | RAD/s: %.2f", radiation, rads_per_second);
                    ImGui::PopStyleColor();
                }
            }
            if (show_graph && history_size > 0) {
                Float max_fps = 0.0f;
                for (Int i = 0; i < history_size; ++i) {
                    if (fps_history[i] > max_fps) {
                        max_fps = fps_history[i];
                    }
                }
                ImGui::PlotLines("##FPSGraph", fps_history, history_size, 0, nullptr, 0.0f, max_fps * 1.2f, ImVec2(ImGui::GetContentRegionAvail().x, 80));
            }
            ImGui::End();
        }

        if (show_crosshair) {
            Float screen_width = ImGui::GetIO().DisplaySize.x;
            Float screen_height = ImGui::GetIO().DisplaySize.y;
            Float center_x = screen_width / 2.0f;
            Float center_y = screen_height / 2.0f;

            ImDrawList* draw_list = ImGui::GetForegroundDrawList();

            ImU32 color = IM_COL32(255, 255, 255, 200);
            Float thickness = 2.0f;

            if (canUse) {
                Float radius = 10.0f;
                draw_list->AddCircle(ImVec2(center_x, center_y), radius, color, 32, thickness);
            }
            else {
                Float line_length = 8.0f;
                Float gap_size = 6.0f;
                draw_list->AddLine(ImVec2(center_x, center_y - gap_size - line_length), ImVec2(center_x, center_y - gap_size), color, thickness);
                draw_list->AddLine(ImVec2(center_x, center_y + gap_size), ImVec2(center_x, center_y + gap_size + line_length), color, thickness);
                draw_list->AddLine(ImVec2(center_x - gap_size - line_length, center_y), ImVec2(center_x - gap_size, center_y), color, thickness);
                draw_list->AddLine(ImVec2(center_x + gap_size, center_y), ImVec2(center_x + gap_size + line_length, center_y), color, thickness);
            }
        }

        if (show_watermark) {
            constexpr Float PADDING = 10.0f;
            ImGuiIO& io = ImGui::GetIO();
            ImVec2 window_pos = ImVec2(io.DisplaySize.x - PADDING, PADDING);
            ImVec2 window_pos_pivot = ImVec2(1.0f, 0.0f);
            ImGui::SetNextWindowPos(window_pos, ImGuiCond_Always, window_pos_pivot);
            ImGui::SetNextWindowBgAlpha(0.0f);

            ImGuiWindowFlags window_flags = ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoFocusOnAppearing | ImGuiWindowFlags_NoNav | ImGuiWindowFlags_NoMove;

            if (ImGui::Begin("Watermark", nullptr, window_flags)) {
                ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 1.0f, 1.0f, 0.6f));

                ImGui::Text("Tectonic Engine");
                ImGui::Text("Build: %d", Common::GetBuildNumber());
                ImGui::Text("Branch: %s", BRANCH_NAME);
                ImGui::Text("Built on %s at %s", __DATE__, __TIME__);

                ImGui::PopStyleColor();
            }

            ImGui::End();
        }
    }
    void UI_RenderCredits(Bool active, const Char* text, Float timer, Float duration) {
        if (!active || !text) {
            return;
        }

        ImGuiIO& io = ImGui::GetIO();
        ImDrawList* draw_list = ImGui::GetForegroundDrawList();
        ImFont* font = io.Fonts->Fonts[0];
        Float font_size = ImGui::GetFontSize() * 1.5f;

        draw_list->AddRectFilled(ImVec2(0, 0), io.DisplaySize, IM_COL32(0, 0, 0, 255));

        Float total_text_height = 0;
        Char* text_copy = new Char[strlen(text) + 1];
        strcpy(text_copy, text);
        Char* line = strtok(text_copy, "\n");
        while (line) {
            ImVec2 text_size = font->CalcTextSizeA(font_size, FLT_MAX, 0.0f, line);
            total_text_height += text_size.y + 5.0f;
            line = strtok(nullptr, "\n");
        }
        delete[] text_copy;

        Float progress = 0.0f;
        if (duration > 0.0f) {
            progress = timer / duration;
        }
        progress = fminf(progress, 1.0f);

        Float start_y = io.DisplaySize.y;
        Float end_y = -total_text_height;
        Float current_y = start_y + (end_y - start_y) * progress;

        text_copy = new Char[strlen(text) + 1];
        strcpy(text_copy, text);
        line = strtok(text_copy, "\n");
        while (line) {
            ImVec2 text_size = font->CalcTextSizeA(font_size, FLT_MAX, 0.0f, line);
            Float pos_x = (io.DisplaySize.x - text_size.x) * 0.5f;

            if (current_y > -text_size.y && current_y < io.DisplaySize.y) {
                draw_list->AddText(font, font_size, ImVec2(pos_x, current_y), IM_COL32(255, 255, 255, 255), line);
            }

            current_y += text_size.y + 5.0f;
            line = strtok(nullptr, "\n");
        }
        delete[] text_copy;
    }
    void UI_RenderDeveloperOverlay(void) {
        if (Cvar_GetInt("developer") == 0) {
            return;
        }

        const Float DISTANCE = 10.0f;
        ImVec2 window_pos = ImVec2(DISTANCE, DISTANCE + 80);
        ImVec2 window_pos_pivot = ImVec2(0.0f, 0.0f);
        ImGui::SetNextWindowPos(window_pos, ImGuiCond_Always, window_pos_pivot);
        ImGui::SetNextWindowBgAlpha(0.0f);

        ImGuiWindowFlags window_flags = ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoFocusOnAppearing | ImGuiWindowFlags_NoNav | ImGuiWindowFlags_NoMove;

        if (ImGui::Begin("DeveloperOverlay", nullptr, window_flags)) {
            Int start_index = console_instance.Items.size() - 10;
            if (start_index < 0) start_index = 0;

            for (Int i = start_index; i < console_instance.Items.size(); i++) {
                const ConsoleItem& item = console_instance.Items[i];
                ImVec4 color;
                Bool has_color = false;
                if (item.color == CONSOLE_COLOR_RED) { color = ImVec4(1.0f, 0.4f, 0.4f, 1.0f); has_color = true; }
                else if (item.color == CONSOLE_COLOR_YELLOW) { color = ImVec4(1.0f, 1.0f, 0.4f, 1.0f); has_color = true; }

                if (has_color) ImGui::PushStyleColor(ImGuiCol_Text, color);
                ImGui::TextUnformatted(item.text);
                if (has_color) ImGui::PopStyleColor();
            }
        }
        ImGui::End();
    }