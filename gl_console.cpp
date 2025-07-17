/*
 * Copyright © 2025 Soft Sprint Studios
 * All rights reserved.
 *
 * This file is proprietary and confidential. Unauthorized reproduction,
 * modification, or distribution is strictly prohibited unless explicit
 * written permission is granted by Soft Sprint Studios.
 */
#include "imgui-master/imgui.h"
#include "imgui-master/backends/imgui_impl_sdl2.h"
#include "imgui-master/backends/imgui_impl_opengl3.h"
#include "imgui-master/imgui_internal.h"

#include <stdio.h>
#include <vector>
#include <string>
#include <cstdarg>
#include <stdint.h>

#include "gl_console.h"
#include "cvar.h"

static bool show_console = false;
static command_callback_t command_handler = nullptr;

struct ConsoleItem {
    char* text;
    ConsoleTextColor color;
};

struct Console {
    char                  InputBuf[256];
    std::vector<ConsoleItem> Items;
    bool                  ScrollToBottom;
    Console() { ClearLog(); memset(InputBuf, 0, sizeof(InputBuf)); ScrollToBottom = true; }
    ~Console() { ClearLog(); }
    void ClearLog() { for (int i = 0; i < Items.size(); i++) free(Items[i].text); Items.clear(); }

    void AddLog(ConsoleTextColor color, const char* fmt, va_list args) {
        char buf[1024];
        vsnprintf(buf, IM_ARRAYSIZE(buf), fmt, args);
        buf[IM_ARRAYSIZE(buf) - 1] = 0;

        ConsoleItem item;
        item.text = _strdup(buf);
        item.color = color;
        Items.push_back(item);
        ScrollToBottom = true;
    }

    void Draw() {
        if (!show_console) return;
        ImGui::SetNextWindowSize(ImVec2(520, 600), ImGuiCond_FirstUseEver);
        if (!ImGui::Begin("Console", &show_console)) { ImGui::End(); return; }
        const float footer_height_to_reserve = ImGui::GetStyle().ItemSpacing.y + ImGui::GetFrameHeightWithSpacing();
        ImGui::BeginChild("ScrollingRegion", ImVec2(0, -footer_height_to_reserve), false, ImGuiWindowFlags_HorizontalScrollbar);
        for (int i = 0; i < Items.size(); i++) {
            const ConsoleItem& item = Items[i];
            ImVec4 color;
            bool has_color = false;
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
        bool reclaim_focus = false;
        if (ImGui::InputText("Input", InputBuf, IM_ARRAYSIZE(InputBuf), ImGuiInputTextFlags_EnterReturnsTrue)) { char* s = InputBuf; if (s[0]) ExecCommand(s); strcpy(s, ""); reclaim_focus = true; }
        ImGui::SameLine();
        if (ImGui::Button("Submit")) { char* s = InputBuf; if (s[0]) ExecCommand(s); strcpy(s, ""); reclaim_focus = true; }
        ImGui::SetItemDefaultFocus(); if (reclaim_focus) ImGui::SetKeyboardFocusHere(-1); ImGui::End();
    }
    void ExecCommand(const char* command_line) {
        Console_Printf("# %s", command_line);
        if (command_handler) {
            char* cmd_copy = _strdup(command_line); const int MAX_ARGS = 16; int argc = 0; char* argv[MAX_ARGS];
            char* p = strtok(cmd_copy, " ");
            while (p != NULL && argc < MAX_ARGS) { argv[argc++] = p; p = strtok(NULL, " "); }
            if (argc > 0) command_handler(argc, argv); free(cmd_copy);
        }
    }
};

static Console console_instance;

extern "C" {

    void UI_Init(SDL_Window* window, SDL_GLContext context) {
        IMGUI_CHECKVERSION(); ImGui::CreateContext(); ImGuiIO& io = ImGui::GetIO(); (void)io;
        ImGui::StyleColorsDark(); ImGui_ImplSDL2_InitForOpenGL(window, context);
        ImGui_ImplOpenGL3_Init("#version 450");
        Console_Printf("Console Initialized.");
    }
    void UI_Shutdown() { ImGui_ImplOpenGL3_Shutdown(); ImGui_ImplSDL2_Shutdown(); ImGui::DestroyContext(); }
    void UI_ProcessEvent(SDL_Event* event) { ImGui_ImplSDL2_ProcessEvent(event); }
    void UI_BeginFrame() { ImGui_ImplOpenGL3_NewFrame(); ImGui_ImplSDL2_NewFrame(); ImGui::NewFrame(); }
    void UI_EndFrame(SDL_Window* window) { ImGui::Render(); ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData()); SDL_GL_SwapWindow(window); }
    void Console_Toggle() { show_console = !show_console; }
    bool Console_IsVisible() { return show_console; }
    void Console_Draw() { console_instance.Draw(); }
    void Console_SetCommandHandler(command_callback_t handler) { command_handler = handler; }

    void Console_Printf(const char* fmt, ...) {
        va_list args;
        va_start(args, fmt);
        console_instance.AddLog(CONSOLE_COLOR_WHITE, fmt, args);
        va_end(args);
    }

    void Console_Printf_Error(const char* fmt, ...) {
        va_list args;
        va_start(args, fmt);
        console_instance.AddLog(CONSOLE_COLOR_RED, fmt, args);
        va_end(args);
    }

    void Console_Printf_Warning(const char* fmt, ...) {
        va_list args;
        va_start(args, fmt);
        console_instance.AddLog(CONSOLE_COLOR_YELLOW, fmt, args);
        va_end(args);
    }

    void UI_RenderGameHUD(float fps, float px, float py, float pz) {
        bool show_fps = Cvar_GetInt("show_fps");
        bool show_pos = Cvar_GetInt("show_pos");
        bool show_crosshair = Cvar_GetInt("crosshair");

        if (!show_fps && !show_pos) {
            return;
        }

        const float DISTANCE = 10.0f;
        ImVec2 window_pos = ImVec2(DISTANCE, DISTANCE);
        ImVec2 window_pos_pivot = ImVec2(0.0f, 0.0f);
        ImGui::SetNextWindowPos(window_pos, ImGuiCond_Always, window_pos_pivot);
        ImGui::SetNextWindowBgAlpha(0.35f);

        ImGuiWindowFlags window_flags = ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoFocusOnAppearing | ImGuiWindowFlags_NoNav | ImGuiWindowFlags_NoMove;

        if (ImGui::Begin("GameHUD", NULL, window_flags))
        {
            if (show_fps) {
                ImGui::Text("FPS: %.1f", fps);
            }
            if (show_pos) {
                ImGui::Text("Pos: %.2f, %.2f, %.2f", px, py, pz);
            }
        }
        ImGui::End();
        if (show_crosshair) {
            float screen_width = ImGui::GetIO().DisplaySize.x;
            float screen_height = ImGui::GetIO().DisplaySize.y;
            float center_x = screen_width / 2.0f;
            float center_y = screen_height / 2.0f;

            ImDrawList* draw_list = ImGui::GetForegroundDrawList();

            float line_length = 8.0f;
            float gap_size = 6.0f;
            float thickness = 2.0f;
            ImU32 color = IM_COL32(255, 255, 255, 200);

            draw_list->AddLine(ImVec2(center_x, center_y - gap_size - line_length), ImVec2(center_x, center_y - gap_size), color, thickness);

            draw_list->AddLine(ImVec2(center_x, center_y + gap_size), ImVec2(center_x, center_y + gap_size + line_length), color, thickness);

            draw_list->AddLine(ImVec2(center_x - gap_size - line_length, center_y), ImVec2(center_x - gap_size, center_y), color, thickness);

            draw_list->AddLine(ImVec2(center_x + gap_size, center_y), ImVec2(center_x + gap_size + line_length, center_y), color, thickness);
        }
    }
    bool UI_Begin(const char* name, bool* p_open) { return ImGui::Begin(name, p_open); }
    bool UI_Begin_NoClose(const char* name) { return ImGui::Begin(name, NULL); }
    void UI_OpenPopup(const char* str_id) { ImGui::OpenPopup(str_id); }
    bool UI_BeginPopupModal(const char* name, bool* p_open, int flags) { return ImGui::BeginPopupModal(name, p_open, (ImGuiWindowFlags)flags); }
    void UI_CloseCurrentPopup(void) { ImGui::CloseCurrentPopup(); }
    void UI_End() { ImGui::End(); }
    bool UI_BeginMainMenuBar() { return ImGui::BeginMainMenuBar(); }
    void UI_EndMainMenuBar() { ImGui::EndMainMenuBar(); }
    bool UI_BeginMenu(const char* label, bool enabled) { return ImGui::BeginMenu(label, enabled); }
    void UI_EndMenu() { ImGui::EndMenu(); }
    bool UI_MenuItem(const char* label, const char* shortcut, bool selected, bool enabled) { return ImGui::MenuItem(label, shortcut, selected, enabled); }
    void UI_Text(const char* fmt, ...) { va_list args; va_start(args, fmt); ImGui::TextV(fmt, args); va_end(args); }
    void UI_Separator() { ImGui::Separator(); }
    bool UI_CollapsingHeader(const char* label, int flags) { return ImGui::CollapsingHeader(label, (ImGuiTreeNodeFlags)flags); }
    bool UI_Selectable(const char* label, bool selected) { return ImGui::Selectable(label, selected); }
    bool UI_Button(const char* label) { return ImGui::Button(label); }
    bool UI_DragFloat3(const char* label, float v[3], float v_speed, float v_min, float v_max) { return ImGui::DragFloat3(label, v, v_speed, v_min, v_max); }
    bool UI_DragFloat2(const char* label, float v[2], float v_speed, float v_min, float v_max) { return ImGui::DragFloat2(label, v, v_speed, v_min, v_max); }
    bool UI_DragFloat(const char* label, float* v, float v_speed, float v_min, float v_max) { return ImGui::DragFloat(label, v, v_speed, v_min, v_max); }
    bool UI_DragInt(const char* label, int* v, float v_speed, int v_min, int v_max) { return ImGui::DragInt(label, v, v_speed, v_min, v_max); }
    bool UI_ColorEdit3(const char* label, float col[3]) { return ImGui::ColorEdit3(label, col); }
    void UI_Image(void* user_texture_id, float width, float height) { ImGui::Image((ImTextureID)user_texture_id, ImVec2(width, height), ImVec2(0, 1), ImVec2(1, 0)); }
    bool UI_IsWindowFocused() { return ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows); }
    bool UI_IsWindowHovered() { return ImGui::IsWindowHovered(ImGuiHoveredFlags_RootAndChildWindows); }
    bool UI_IsMouseDragging(int button) { return ImGui::IsMouseDragging(button); }
    void UI_GetContentRegionAvail(float* w, float* h) {
        ImVec2 size = ImGui::GetContentRegionAvail();
        if (w) *w = size.x;
        if (h) *h = size.y;
    }
    void UI_GetWindowContentRegionMin(float* x, float* y) { ImVec2 min = ImGui::GetWindowContentRegionMin(); *x = min.x; *y = min.y; }
    void UI_GetMousePos(float* x, float* y) { ImVec2 pos = ImGui::GetIO().MousePos; *x = pos.x; *y = pos.y; }
    void UI_GetWindowPos(float* x, float* y) { ImVec2 pos = ImGui::GetWindowPos(); *x = pos.x; *y = pos.y; }
    void UI_GetWindowSize(float* w, float* h) { ImVec2 size = ImGui::GetWindowSize(); *w = size.x; *h = size.y; }
    void UI_PushStyleVar_WindowPadding(float val_x, float val_y) { ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(val_x, val_y)); }
    void UI_PopStyleVar(int count) { ImGui::PopStyleVar(count); }
    void UI_InputText(const char* label, char* buf, size_t buf_size) { ImGui::InputText(label, buf, buf_size); }

    void UI_EndChild() { ImGui::EndChild(); }
    bool UI_BeginChild(const char* str_id, float width, float height, bool border, int flags) { return ImGui::BeginChild(str_id, ImVec2(width, height), border, (ImGuiWindowFlags)flags); }
    void UI_SameLine() { ImGui::SameLine(); }
    bool UI_RadioButton(const char* label, bool active) { return ImGui::RadioButton(label, active); }
    bool UI_RadioButton_Int(const char* label, int* v, int v_button) { return ImGui::RadioButton(label, v, v_button); }
    bool UI_Combo(const char* label, int* current_item, const char* const items[], int items_count, int popup_max_height_in_items) { return ImGui::Combo(label, current_item, items, items_count, popup_max_height_in_items); }
    void UI_SetNextWindowPos(float x, float y) { ImGui::SetNextWindowPos(ImVec2(x, y)); }
    void UI_SetNextWindowSize(float w, float h) { ImGui::SetNextWindowSize(ImVec2(w, h)); }

    bool UI_IsItemHovered() { return ImGui::IsItemHovered(); }
    bool UI_ListBox(const char* label, int* current_item, const char* const* items, int items_count, int height_in_items) { return ImGui::ListBox(label, current_item, items, items_count, height_in_items); }
    float UI_GetMouseWheel() { return ImGui::GetIO().MouseWheel; }
    void UI_GetMouseDragDelta(int button, float lock_threshold, float* dx, float* dy) { ImVec2 delta = ImGui::GetMouseDragDelta((ImGuiMouseButton)button, lock_threshold); *dx = delta.x; *dy = delta.y; }
    void UI_ResetMouseDragDelta(int button) { ImGui::ResetMouseDragDelta((ImGuiMouseButton)button); }

    bool UI_IsItemActivated(void) {
        return ImGui::IsItemActivated();
    }
    bool UI_IsItemDeactivatedAfterEdit(void) {
        return ImGui::IsItemDeactivatedAfterEdit();
    }
    bool UI_Checkbox(const char* label, bool* v) {
        return ImGui::Checkbox(label, v);
    }

    bool UI_ImageButton(const char* str_id, unsigned int user_texture_id, float width, float height) {
        return ImGui::ImageButton(str_id, (ImTextureID)(intptr_t)user_texture_id, ImVec2(width, height));
    }
    void UI_BeginTooltip() {
        ImGui::BeginTooltip();
    }
    void UI_EndTooltip() {
        ImGui::EndTooltip();
    }
    float UI_GetWindowPos_X() {
        return ImGui::GetWindowPos().x;
    }
    float UI_GetWindowContentRegionMax_X() {
        return ImGui::GetWindowContentRegionMax().x;
    }
    float UI_GetItemRectMax_X() {
        return ImGui::GetItemRectMax().x;
    }
    float UI_GetStyle_ItemSpacing_X() {
        return ImGui::GetStyle().ItemSpacing.x;
    }
    void UI_PushID(int int_id) {
        ImGui::PushID(int_id);
    }
    void UI_PopID() {
        ImGui::PopID();
    }
    void UI_GetDisplaySize(float* w, float* h) {
        ImVec2 size = ImGui::GetIO().DisplaySize;
        if (w) *w = size.x;
        if (h) *h = size.y;
    }
    void UI_Spacing() {
        ImGui::Spacing();
    }
    bool UI_BeginPopupContextItem(const char* str_id) {
        return ImGui::BeginPopupContextItem(str_id);
    }
    void UI_EndPopup() {
        ImGui::EndPopup();
    }
    void UI_SetNextItemWidth(float item_width) {
        ImGui::SetNextItemWidth(item_width);
    }
    void UI_TextColored(Vec4 color, const char* fmt, ...) { va_list args; va_start(args, fmt); ImGui::TextColoredV(ImVec4(color.x, color.y, color.z, color.w), fmt, args); va_end(args); }
    void UI_TextWrapped(const char* fmt, ...) { va_list args; va_start(args, fmt); ImGui::TextWrappedV(fmt, args); va_end(args); }
    void UI_BulletText(const char* fmt, ...) { va_list args; va_start(args, fmt); ImGui::BulletTextV(fmt, args); va_end(args); }
    bool UI_BeginTable(const char* str_id, int column, int flags, float outer_width, float inner_width) { return ImGui::BeginTable(str_id, column, (ImGuiTableFlags)flags, ImVec2(outer_width, inner_width)); }
    void UI_EndTable() { ImGui::EndTable(); }
    void UI_TableNextRow() { ImGui::TableNextRow(); }
    void UI_TableNextColumn() { ImGui::TableNextColumn(); }
    void UI_TableHeadersRow() { ImGui::TableHeadersRow(); }
}