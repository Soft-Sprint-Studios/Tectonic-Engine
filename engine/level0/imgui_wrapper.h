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
#pragma once
#ifndef IMGUI_WRAPPER_H
#define IMGUI_WRAPPER_H

#include <SDL.h>
#include <stdbool.h>
#include "math_lib.h"
#include "level0_api.h"


    LEVEL0_API void UI_Init(SDL_Window* window, SDL_GLContext context);
    LEVEL0_API void UI_Shutdown();
    LEVEL0_API void UI_ProcessEvent(SDL_Event* event);
    LEVEL0_API void UI_BeginFrame();
    LEVEL0_API void UI_EndFrame(SDL_Window* window);

    LEVEL0_API Bool UI_Begin(const Char* name, Bool* p_open);
    LEVEL0_API Bool UI_Begin_NoBringToFront(const Char* name, Bool* p_open);
    LEVEL0_API Bool UI_Begin_NoClose(const Char* name);
    LEVEL0_API Bool UI_Begin_NoTitlebar_NoResize_NoMove(const Char* name, Bool* p_open);
    LEVEL0_API Bool UI_Begin_WithFlags(const Char* name, Bool* p_open, Int flags);
    LEVEL0_API Bool UI_IsWindowOpen(const Char* name);
    LEVEL0_API void UI_OpenPopup(const Char* str_id);
    LEVEL0_API Bool UI_BeginPopupModal(const Char* name, Bool* p_open, Int flags);
    LEVEL0_API void UI_CloseCurrentPopup(void);
    LEVEL0_API void UI_End();

    LEVEL0_API Bool UI_BeginMainMenuBar();
    LEVEL0_API void UI_EndMainMenuBar();
    LEVEL0_API Bool UI_BeginMenu(const Char* label, Bool enabled);
    LEVEL0_API void UI_EndMenu();
    LEVEL0_API Bool UI_MenuItem(const Char* label, const Char* shortcut, Bool selected, Bool enabled);

    LEVEL0_API void UI_Text(const Char* fmt, ...);
    LEVEL0_API void UI_TextColored(Vec4 color, const Char* fmt, ...);
    LEVEL0_API void UI_TextWrapped(const Char* fmt, ...);
    LEVEL0_API void UI_BulletText(const Char* fmt, ...);
    LEVEL0_API void UI_Separator();
    LEVEL0_API void UI_SeparatorEx(Int flags);
    LEVEL0_API Bool UI_CollapsingHeader(const Char* label, Int flags);
    LEVEL0_API void UI_Spacing();
    LEVEL0_API void UI_SameLine();
    LEVEL0_API void UI_BeginGroup(void);
    LEVEL0_API void UI_EndGroup(void);

    LEVEL0_API Bool UI_Selectable(const Char* label, Bool selected);
    LEVEL0_API Bool UI_Button(const Char* label);
    LEVEL0_API Bool UI_DragFloat3(const Char* label, Float v[3], Float v_speed, Float v_min, Float v_max);
    LEVEL0_API Bool UI_DragFloat(const Char* label, Float* v, Float v_speed, Float v_min, Float v_max);
    LEVEL0_API Bool UI_DragFloat2(const Char* label, Float v[2], Float v_speed, Float v_min, Float v_max);
    LEVEL0_API Bool UI_DragInt(const Char* label, Int* v, Float v_speed, Int v_min, Int v_max);
    LEVEL0_API Bool UI_InputFloat(const Char* label, Float* v, Float step, Float step_fast, const Char* format);
    LEVEL0_API Bool UI_ColorEdit3(const Char* label, Float col[3]);
    LEVEL0_API void UI_Image(void* user_texture_id, Float width, Float height);
    LEVEL0_API Bool UI_ImageButton(const Char* str_id, Uint user_texture_id, Float width, Float height);
    LEVEL0_API Bool UI_ImageButton_Flip(const Char* id, void* texture_id, Float width, Float height);
    LEVEL0_API void UI_InputText(const Char* label, Char* buf, Usize buf_size);
    LEVEL0_API Bool UI_InputText_Flags(const Char* label, Char* buf, Usize buf_size, Int flags);
    LEVEL0_API Bool UI_Checkbox(const Char* label, Bool* v);
    LEVEL0_API Bool UI_RadioButton(const Char* label, Bool active);
    LEVEL0_API Bool UI_RadioButton_Int(const Char* label, Int* v, Int v_button);
    LEVEL0_API Bool UI_Combo(const Char* label, Int* current_item, const Char* const items[], Int items_count, Int popup_max_height_in_items);
    LEVEL0_API Bool UI_ListBox(const Char* label, Int* current_item, const Char* const* items, Int items_count, Int height_in_items);

    LEVEL0_API Bool UI_IsWindowFocused();
    LEVEL0_API Bool UI_IsWindowHovered();
    LEVEL0_API Bool UI_IsMouseDragging(Int button);
    LEVEL0_API Bool UI_IsItemHovered();
    LEVEL0_API Bool UI_IsItemActivated(void);
    LEVEL0_API Bool UI_IsItemDeactivatedAfterEdit(void);
    LEVEL0_API Bool UI_WantCaptureMouse();
    LEVEL0_API Bool UI_WantCaptureKeyboard();

    LEVEL0_API void UI_GetContentRegionAvail(Float* w, Float* h);
    LEVEL0_API void UI_GetWindowContentRegionMin(Float* x, Float* y);
    LEVEL0_API void UI_GetMousePos(Float* x, Float* y);
    LEVEL0_API void UI_GetWindowPos(Float* x, Float* y);
    LEVEL0_API void UI_GetWindowSize(Float* w, Float* h);
    LEVEL0_API void UI_PushStyleVar_WindowPadding(Float val_x, Float val_y);
    LEVEL0_API void UI_PopStyleVar(Int count);
    LEVEL0_API Bool UI_BeginChild(const Char* str_id, Float width, Float height, Bool border, Int flags);
    LEVEL0_API void UI_EndChild();
   
    LEVEL0_API void UI_SetNextWindowPos(Float x, Float y);
    LEVEL0_API void UI_SetNextWindowSize(Float w, Float h);
    LEVEL0_API void UI_SetNextItemWidth(Float item_width);
    LEVEL0_API void UI_SetCursorPosX(Float x);

    LEVEL0_API Float UI_GetMouseWheel();
    LEVEL0_API void UI_GetMouseDragDelta(Int button, Float lock_threshold, Float* dx, Float* dy);
    LEVEL0_API void UI_ResetMouseDragDelta(Int button);

    LEVEL0_API void UI_GetDisplaySize(Float* w, Float* h);

    LEVEL0_API Bool UI_BeginPopupContextItem(const Char* str_id);
    LEVEL0_API void UI_EndPopup();
    LEVEL0_API void UI_BeginTooltip();
    LEVEL0_API void UI_EndTooltip();

    LEVEL0_API Bool UI_BeginTabBar(const Char* str_id, Int flags);
    LEVEL0_API void UI_EndTabBar();
    LEVEL0_API Bool UI_BeginTabItem(const Char* label);
    LEVEL0_API void UI_EndTabItem();

    LEVEL0_API Bool UI_BeginTable(const Char* str_id, Int column, Int flags, Float outer_width, Float inner_width);
    LEVEL0_API void UI_EndTable();
    LEVEL0_API void UI_TableNextRow();
    LEVEL0_API void UI_TableNextColumn();
    LEVEL0_API void UI_TableHeadersRow();

    LEVEL0_API void UI_BeginDisabled(Bool disabled);
    LEVEL0_API void UI_EndDisabled(void);

    LEVEL0_API void UI_PushID(Int int_id);
    LEVEL0_API void UI_PopID();

    LEVEL0_API Float UI_GetWindowPos_X();
    LEVEL0_API Float UI_GetWindowContentRegionMax_X();
    LEVEL0_API Float UI_GetWindowWidth();
    LEVEL0_API Float UI_GetItemRectMax_X();
    LEVEL0_API Float UI_GetStyle_ItemSpacing_X();

    LEVEL0_API void* UI_GetWindowDrawList();
    LEVEL0_API void UI_DrawList_AddText(void* draw_list, Float pos_x, Float pos_y, Uint col, const Char* text);
    LEVEL0_API Uint UI_GetColorU32(Int r, Int g, Int b, Int a);


#endif // IMGUI_WRAPPER_H