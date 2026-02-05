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

#include "imgui_wrapper.h"

#include <stdio.h>
#include <cstdarg>

    void UI_Init(SDL_Window* window, SDL_GLContext context) {
        IMGUI_CHECKVERSION();
        ImGui::CreateContext();
        ImGuiIO& io = ImGui::GetIO(); (void)io;
        ImGui::StyleColorsDark();
        ImGui_ImplSDL2_InitForOpenGL(window, context);
        ImGui_ImplOpenGL3_Init("#version 450");
    }

    void UI_Shutdown() {
        ImGui_ImplOpenGL3_Shutdown();
        ImGui_ImplSDL2_Shutdown();
        ImGui::DestroyContext();
    }

    void UI_ProcessEvent(SDL_Event* event) {
        ImGui_ImplSDL2_ProcessEvent(event);
    }

    void UI_BeginFrame() {
        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplSDL2_NewFrame();
        ImGui::NewFrame();
    }

    void UI_EndFrame(SDL_Window* window) {
        ImGui::Render();
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
        SDL_GL_SwapWindow(window);
    }

    bool UI_Begin(const char* name, bool* p_open) { return ImGui::Begin(name, p_open); }
    bool UI_Begin_NoBringToFront(const char* name, bool* p_open) {
        return ImGui::Begin(name, p_open, ImGuiWindowFlags_NoBringToFrontOnFocus);
    }
    bool UI_Begin_NoClose(const char* name) { return ImGui::Begin(name, nullptr); }
    bool UI_Begin_NoTitlebar_NoResize_NoMove(const char* name, bool* p_open) {
        return ImGui::Begin(name, p_open, ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove);
    }
    bool UI_Begin_WithFlags(const char* name, bool* p_open, int flags) {
        return ImGui::Begin(name, p_open, (ImGuiWindowFlags)flags);
    }
    bool UI_IsWindowOpen(const char* name) {
        ImGuiWindow* window = ImGui::FindWindowByName(name);
        return (window != nullptr && window->WasActive);
    }
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
    void UI_SeparatorEx(int flags) {
        ImGui::SeparatorEx((ImGuiSeparatorFlags)flags);
    }
    bool UI_CollapsingHeader(const char* label, int flags) { return ImGui::CollapsingHeader(label, (ImGuiTreeNodeFlags)flags); }
    bool UI_Selectable(const char* label, bool selected) { return ImGui::Selectable(label, selected); }
    bool UI_Button(const char* label) { return ImGui::Button(label); }
    bool UI_DragFloat3(const char* label, float v[3], float v_speed, float v_min, float v_max) { return ImGui::DragFloat3(label, v, v_speed, v_min, v_max); }
    bool UI_DragFloat2(const char* label, float v[2], float v_speed, float v_min, float v_max) { return ImGui::DragFloat2(label, v, v_speed, v_min, v_max); }
    bool UI_DragFloat(const char* label, float* v, float v_speed, float v_min, float v_max) { return ImGui::DragFloat(label, v, v_speed, v_min, v_max); }
    bool UI_DragInt(const char* label, int* v, float v_speed, int v_min, int v_max) { return ImGui::DragInt(label, v, v_speed, v_min, v_max); }
    bool UI_InputFloat(const char* label, float* v, float step, float step_fast, const char* format) {
        return ImGui::InputFloat(label, v, step, step_fast, format);
    }
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
    bool UI_InputText_Flags(const char* label, char* buf, size_t buf_size, int flags) {
        return ImGui::InputText(label, buf, buf_size, (ImGuiInputTextFlags)flags);
    }
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
    bool UI_BeginTabBar(const char* str_id, int flags) { return ImGui::BeginTabBar(str_id, (ImGuiTabBarFlags)flags); }
    void UI_EndTabBar() { ImGui::EndTabBar(); }
    bool UI_BeginTabItem(const char* label) { return ImGui::BeginTabItem(label); }
    void UI_EndTabItem() { ImGui::EndTabItem(); }
    void UI_SetCursorPosX(float x) { ImGui::SetCursorPosX(x); }
    float UI_GetWindowWidth() { return ImGui::GetWindowSize().x; }
    bool UI_WantCaptureMouse() { return ImGui::GetIO().WantCaptureMouse; }
    bool UI_WantCaptureKeyboard() { return ImGui::GetIO().WantCaptureKeyboard; }
    void UI_BeginDisabled(bool disabled) {
        ImGui::BeginDisabled(disabled);
    }
    void UI_EndDisabled(void) {
        ImGui::EndDisabled();
    }
    void UI_BeginGroup(void) {
        ImGui::BeginGroup();
    }

    void UI_EndGroup(void) {
        ImGui::EndGroup();
    }
    bool UI_ImageButton_Flip(const char* id, void* texture_id, float width, float height) {
        return ImGui::ImageButton(id, reinterpret_cast<ImTextureID>(texture_id), ImVec2(width, height), ImVec2(0, 1), ImVec2(1, 0));
    }
    void* UI_GetWindowDrawList() {
        return (void*)ImGui::GetWindowDrawList();
    }

    void UI_DrawList_AddText(void* draw_list, float pos_x, float pos_y, unsigned int col, const char* text) {
        if (draw_list) {
            ImDrawList* list = (ImDrawList*)draw_list;
            list->AddText(ImVec2(pos_x, pos_y), col, text);
        }
    }

    unsigned int UI_GetColorU32(int r, int g, int b, int a) {
        return IM_COL32(r, g, b, a);
    }