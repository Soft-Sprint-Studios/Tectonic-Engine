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

    Bool UI_Begin(const Char* name, Bool* p_open) { return ImGui::Begin(name, p_open); }
    Bool UI_Begin_NoBringToFront(const Char* name, Bool* p_open) {
        return ImGui::Begin(name, p_open, ImGuiWindowFlags_NoBringToFrontOnFocus);
    }
    Bool UI_Begin_NoClose(const Char* name) { return ImGui::Begin(name, nullptr); }
    Bool UI_Begin_NoTitlebar_NoResize_NoMove(const Char* name, Bool* p_open) {
        return ImGui::Begin(name, p_open, ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove);
    }
    Bool UI_Begin_WithFlags(const Char* name, Bool* p_open, Int flags) {
        return ImGui::Begin(name, p_open, (ImGuiWindowFlags)flags);
    }
    Bool UI_IsWindowOpen(const Char* name) {
        ImGuiWindow* window = ImGui::FindWindowByName(name);
        return (window != nullptr && window->WasActive);
    }
    void UI_OpenPopup(const Char* str_id) { ImGui::OpenPopup(str_id); }
    Bool UI_BeginPopupModal(const Char* name, Bool* p_open, Int flags) { return ImGui::BeginPopupModal(name, p_open, (ImGuiWindowFlags)flags); }
    void UI_CloseCurrentPopup(void) { ImGui::CloseCurrentPopup(); }
    void UI_End() { ImGui::End(); }
    Bool UI_BeginMainMenuBar() { return ImGui::BeginMainMenuBar(); }
    void UI_EndMainMenuBar() { ImGui::EndMainMenuBar(); }
    Bool UI_BeginMenu(const Char* label, Bool enabled) { return ImGui::BeginMenu(label, enabled); }
    void UI_EndMenu() { ImGui::EndMenu(); }
    Bool UI_MenuItem(const Char* label, const Char* shortcut, Bool selected, Bool enabled) { return ImGui::MenuItem(label, shortcut, selected, enabled); }
    void UI_Text(const Char* fmt, ...) { va_list args; va_start(args, fmt); ImGui::TextV(fmt, args); va_end(args); }
    void UI_Separator() { ImGui::Separator(); }
    void UI_SeparatorEx(Int flags) {
        ImGui::SeparatorEx((ImGuiSeparatorFlags)flags);
    }
    Bool UI_CollapsingHeader(const Char* label, Int flags) { return ImGui::CollapsingHeader(label, (ImGuiTreeNodeFlags)flags); }
    Bool UI_Selectable(const Char* label, Bool selected) { return ImGui::Selectable(label, selected); }
    Bool UI_Button(const Char* label) { return ImGui::Button(label); }
    Bool UI_DragFloat3(const Char* label, Float v[3], Float v_speed, Float v_min, Float v_max) { return ImGui::DragFloat3(label, v, v_speed, v_min, v_max); }
    Bool UI_DragFloat2(const Char* label, Float v[2], Float v_speed, Float v_min, Float v_max) { return ImGui::DragFloat2(label, v, v_speed, v_min, v_max); }
    Bool UI_DragFloat(const Char* label, Float* v, Float v_speed, Float v_min, Float v_max) { return ImGui::DragFloat(label, v, v_speed, v_min, v_max); }
    Bool UI_DragInt(const Char* label, Int* v, Float v_speed, Int v_min, Int v_max) { return ImGui::DragInt(label, v, v_speed, v_min, v_max); }
    Bool UI_InputFloat(const Char* label, Float* v, Float step, Float step_fast, const Char* format) {
        return ImGui::InputFloat(label, v, step, step_fast, format);
    }
    Bool UI_ColorEdit3(const Char* label, Float col[3]) { return ImGui::ColorEdit3(label, col); }
    void UI_Image(void* user_texture_id, Float width, Float height) { ImGui::Image((ImTextureID)user_texture_id, ImVec2(width, height), ImVec2(0, 1), ImVec2(1, 0)); }
    Bool UI_IsWindowFocused() { return ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows); }
    Bool UI_IsWindowHovered() { return ImGui::IsWindowHovered(ImGuiHoveredFlags_RootAndChildWindows); }
    Bool UI_IsMouseDragging(Int button) { return ImGui::IsMouseDragging(button); }
    void UI_GetContentRegionAvail(Float* w, Float* h) {
        ImVec2 size = ImGui::GetContentRegionAvail();
        if (w) *w = size.x;
        if (h) *h = size.y;
    }
    void UI_GetWindowContentRegionMin(Float* x, Float* y) { ImVec2 min = ImGui::GetWindowContentRegionMin(); *x = min.x; *y = min.y; }
    void UI_GetMousePos(Float* x, Float* y) { ImVec2 pos = ImGui::GetIO().MousePos; *x = pos.x; *y = pos.y; }
    void UI_GetWindowPos(Float* x, Float* y) { ImVec2 pos = ImGui::GetWindowPos(); *x = pos.x; *y = pos.y; }
    void UI_GetWindowSize(Float* w, Float* h) { ImVec2 size = ImGui::GetWindowSize(); *w = size.x; *h = size.y; }
    void UI_PushStyleVar_WindowPadding(Float val_x, Float val_y) { ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(val_x, val_y)); }
    void UI_PopStyleVar(Int count) { ImGui::PopStyleVar(count); }
    void UI_InputText(const Char* label, Char* buf, Usize buf_size) { ImGui::InputText(label, buf, buf_size); }
    Bool UI_InputText_Flags(const Char* label, Char* buf, Usize buf_size, Int flags) {
        return ImGui::InputText(label, buf, buf_size, (ImGuiInputTextFlags)flags);
    }
    void UI_EndChild() { ImGui::EndChild(); }
    Bool UI_BeginChild(const Char* str_id, Float width, Float height, Bool border, Int flags) { return ImGui::BeginChild(str_id, ImVec2(width, height), border, (ImGuiWindowFlags)flags); }
    void UI_SameLine() { ImGui::SameLine(); }
    Bool UI_RadioButton(const Char* label, Bool active) { return ImGui::RadioButton(label, active); }
    Bool UI_RadioButton_Int(const Char* label, Int* v, Int v_button) { return ImGui::RadioButton(label, v, v_button); }
    Bool UI_Combo(const Char* label, Int* current_item, const Char* const items[], Int items_count, Int popup_max_height_in_items) { return ImGui::Combo(label, current_item, items, items_count, popup_max_height_in_items); }
    void UI_SetNextWindowPos(Float x, Float y) { ImGui::SetNextWindowPos(ImVec2(x, y)); }
    void UI_SetNextWindowSize(Float w, Float h) { ImGui::SetNextWindowSize(ImVec2(w, h)); }

    Bool UI_IsItemHovered() { return ImGui::IsItemHovered(); }
    Bool UI_ListBox(const Char* label, Int* current_item, const Char* const* items, Int items_count, Int height_in_items) { return ImGui::ListBox(label, current_item, items, items_count, height_in_items); }
    Float UI_GetMouseWheel() { return ImGui::GetIO().MouseWheel; }
    void UI_GetMouseDragDelta(Int button, Float lock_threshold, Float* dx, Float* dy) { ImVec2 delta = ImGui::GetMouseDragDelta((ImGuiMouseButton)button, lock_threshold); *dx = delta.x; *dy = delta.y; }
    void UI_ResetMouseDragDelta(Int button) { ImGui::ResetMouseDragDelta((ImGuiMouseButton)button); }

    Bool UI_IsItemActivated(void) {
        return ImGui::IsItemActivated();
    }
    Bool UI_IsItemDeactivatedAfterEdit(void) {
        return ImGui::IsItemDeactivatedAfterEdit();
    }
    Bool UI_Checkbox(const Char* label, Bool* v) {
        return ImGui::Checkbox(label, v);
    }

    Bool UI_ImageButton(const Char* str_id, Uint user_texture_id, Float width, Float height) {
        return ImGui::ImageButton(str_id, (ImTextureID)(intptr_t)user_texture_id, ImVec2(width, height));
    }
    void UI_BeginTooltip() {
        ImGui::BeginTooltip();
    }
    void UI_EndTooltip() {
        ImGui::EndTooltip();
    }
    Float UI_GetWindowPos_X() {
        return ImGui::GetWindowPos().x;
    }
    Float UI_GetWindowContentRegionMax_X() {
        return ImGui::GetWindowContentRegionMax().x;
    }
    Float UI_GetItemRectMax_X() {
        return ImGui::GetItemRectMax().x;
    }
    Float UI_GetStyle_ItemSpacing_X() {
        return ImGui::GetStyle().ItemSpacing.x;
    }
    void UI_PushID(Int int_id) {
        ImGui::PushID(int_id);
    }
    void UI_PopID() {
        ImGui::PopID();
    }
    void UI_GetDisplaySize(Float* w, Float* h) {
        ImVec2 size = ImGui::GetIO().DisplaySize;
        if (w) *w = size.x;
        if (h) *h = size.y;
    }
    void UI_Spacing() {
        ImGui::Spacing();
    }
    Bool UI_BeginPopupContextItem(const Char* str_id) {
        return ImGui::BeginPopupContextItem(str_id);
    }
    void UI_EndPopup() {
        ImGui::EndPopup();
    }
    void UI_SetNextItemWidth(Float item_width) {
        ImGui::SetNextItemWidth(item_width);
    }
    void UI_TextColored(Vec4 color, const Char* fmt, ...) { va_list args; va_start(args, fmt); ImGui::TextColoredV(ImVec4(color.x, color.y, color.z, color.w), fmt, args); va_end(args); }
    void UI_TextWrapped(const Char* fmt, ...) { va_list args; va_start(args, fmt); ImGui::TextWrappedV(fmt, args); va_end(args); }
    void UI_BulletText(const Char* fmt, ...) { va_list args; va_start(args, fmt); ImGui::BulletTextV(fmt, args); va_end(args); }
    Bool UI_BeginTable(const Char* str_id, Int column, Int flags, Float outer_width, Float inner_width) { return ImGui::BeginTable(str_id, column, (ImGuiTableFlags)flags, ImVec2(outer_width, inner_width)); }
    void UI_EndTable() { ImGui::EndTable(); }
    void UI_TableNextRow() { ImGui::TableNextRow(); }
    void UI_TableNextColumn() { ImGui::TableNextColumn(); }
    void UI_TableHeadersRow() { ImGui::TableHeadersRow(); }
    Bool UI_BeginTabBar(const Char* str_id, Int flags) { return ImGui::BeginTabBar(str_id, (ImGuiTabBarFlags)flags); }
    void UI_EndTabBar() { ImGui::EndTabBar(); }
    Bool UI_BeginTabItem(const Char* label) { return ImGui::BeginTabItem(label); }
    void UI_EndTabItem() { ImGui::EndTabItem(); }
    void UI_SetCursorPosX(Float x) { ImGui::SetCursorPosX(x); }
    Float UI_GetWindowWidth() { return ImGui::GetWindowSize().x; }
    Bool UI_WantCaptureMouse() { return ImGui::GetIO().WantCaptureMouse; }
    Bool UI_WantCaptureKeyboard() { return ImGui::GetIO().WantCaptureKeyboard; }
    void UI_BeginDisabled(Bool disabled) {
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
    Bool UI_ImageButton_Flip(const Char* id, void* texture_id, Float width, Float height) {
        return ImGui::ImageButton(id, reinterpret_cast<ImTextureID>(texture_id), ImVec2(width, height), ImVec2(0, 1), ImVec2(1, 0));
    }
    void* UI_GetWindowDrawList() {
        return (void*)ImGui::GetWindowDrawList();
    }

    void UI_DrawList_AddText(void* draw_list, Float pos_x, Float pos_y, Uint col, const Char* text) {
        if (draw_list) {
            ImDrawList* list = (ImDrawList*)draw_list;
            list->AddText(ImVec2(pos_x, pos_y), col, text);
        }
    }

    Uint UI_GetColorU32(Int r, Int g, Int b, Int a) {
        return IM_COL32(r, g, b, a);
    }