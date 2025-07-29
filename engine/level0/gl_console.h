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
#ifndef GL_CONSOLE_H
#define GL_CONSOLE_H

 //----------------------------------------//
 // Brief: The console handling, crosshair and IMGUI Wrapper
 //----------------------------------------//

#include <SDL.h>
#include <stdbool.h>
#include "math_lib.h"
#include "level0_api.h"

#ifdef __cplusplus
extern "C" {
#endif

	typedef enum {
		CONSOLE_COLOR_WHITE,
		CONSOLE_COLOR_RED,
		CONSOLE_COLOR_YELLOW
	} ConsoleTextColor;

	LEVEL0_API void UI_Init(SDL_Window* window, SDL_GLContext context);
	LEVEL0_API void UI_Shutdown();
	LEVEL0_API void UI_ProcessEvent(SDL_Event* event);
	LEVEL0_API void UI_BeginFrame();
	LEVEL0_API void UI_EndFrame(SDL_Window* window);

	LEVEL0_API void Console_Toggle();
	LEVEL0_API void Console_Draw();
	LEVEL0_API void Console_Printf(const char* fmt, ...);
	LEVEL0_API void Console_Printf_Error(const char* fmt, ...);
	LEVEL0_API void Console_Printf_Warning(const char* fmt, ...);
	LEVEL0_API bool Console_IsVisible();
	LEVEL0_API void Console_ClearLog();
	LEVEL0_API void UI_RenderGameHUD(float fps, float px, float py, float pz, const float* fps_history, int history_size);
	LEVEL0_API void UI_RenderDeveloperOverlay(void);
	typedef void (*command_callback_t)(int argc, char** argv);
	LEVEL0_API void Console_SetCommandHandler(command_callback_t handler);

	LEVEL0_API void Log_Init(const char* filename);
	LEVEL0_API void Log_Shutdown(void);

	LEVEL0_API bool UI_Begin(const char* name, bool* p_open);
	LEVEL0_API bool UI_Begin_NoClose(const char* name);
	LEVEL0_API void UI_OpenPopup(const char* str_id);
	LEVEL0_API bool UI_BeginPopupModal(const char* name, bool* p_open, int flags);
	LEVEL0_API void UI_CloseCurrentPopup(void);
	LEVEL0_API void UI_End();
	LEVEL0_API bool UI_BeginMainMenuBar();
	LEVEL0_API void UI_EndMainMenuBar();
	LEVEL0_API bool UI_BeginMenu(const char* label, bool enabled);
	LEVEL0_API void UI_EndMenu();
	LEVEL0_API bool UI_MenuItem(const char* label, const char* shortcut, bool selected, bool enabled);
	LEVEL0_API void UI_Text(const char* fmt, ...);
	LEVEL0_API void UI_TextColored(Vec4 color, const char* fmt, ...);
	LEVEL0_API void UI_TextWrapped(const char* fmt, ...);
	LEVEL0_API void UI_BulletText(const char* fmt, ...);
	LEVEL0_API void UI_Separator();
	LEVEL0_API bool UI_CollapsingHeader(const char* label, int flags);
	LEVEL0_API bool UI_Selectable(const char* label, bool selected);
	LEVEL0_API bool UI_Button(const char* label);
	LEVEL0_API bool UI_DragFloat3(const char* label, float v[3], float v_speed, float v_min, float v_max);
	LEVEL0_API bool UI_DragFloat(const char* label, float* v, float v_speed, float v_min, float v_max);
	LEVEL0_API bool UI_DragFloat2(const char* label, float v[2], float v_speed, float v_min, float v_max);
	LEVEL0_API bool UI_DragInt(const char* label, int* v, float v_speed, int v_min, int v_max);
	LEVEL0_API bool UI_ColorEdit3(const char* label, float col[3]);
	LEVEL0_API void UI_Image(void* user_texture_id, float width, float height);
	LEVEL0_API bool UI_IsWindowFocused();
	LEVEL0_API bool UI_IsWindowHovered();
	LEVEL0_API bool UI_IsMouseDragging(int button);
	LEVEL0_API void UI_GetContentRegionAvail(float* w, float* h);
	LEVEL0_API void UI_GetWindowContentRegionMin(float* x, float* y);
	LEVEL0_API void UI_GetMousePos(float* x, float* y);
	LEVEL0_API void UI_GetWindowPos(float* x, float* y);
	LEVEL0_API void UI_GetWindowSize(float* w, float* h);
	LEVEL0_API void UI_PushStyleVar_WindowPadding(float val_x, float val_y);
	LEVEL0_API void UI_PopStyleVar(int count);
	LEVEL0_API void UI_InputText(const char* label, char* buf, size_t buf_size);

	LEVEL0_API bool UI_BeginChild(const char* str_id, float width, float height, bool border, int flags);
	LEVEL0_API bool UI_RadioButton(const char* label, bool active);
	LEVEL0_API bool UI_RadioButton_Int(const char* label, int* v, int v_button);
	LEVEL0_API bool UI_Combo(const char* label, int* current_item, const char* const items[], int items_count, int popup_max_height_in_items);
	LEVEL0_API void UI_SetNextWindowPos(float x, float y);
	LEVEL0_API void UI_SetNextWindowSize(float w, float h);
	LEVEL0_API void UI_EndChild();
	LEVEL0_API void UI_SameLine();

	LEVEL0_API bool UI_IsItemHovered();
	LEVEL0_API bool UI_ListBox(const char* label, int* current_item, const char* const* items, int items_count, int height_in_items);
	LEVEL0_API float UI_GetMouseWheel();
	LEVEL0_API void UI_GetMouseDragDelta(int button, float lock_threshold, float* dx, float* dy);
	LEVEL0_API void UI_ResetMouseDragDelta(int button);

	LEVEL0_API bool UI_IsItemActivated(void);
	LEVEL0_API bool UI_IsItemDeactivatedAfterEdit(void);
	LEVEL0_API bool UI_Checkbox(const char* label, bool* v);

	LEVEL0_API bool UI_ImageButton(const char* str_id, unsigned int user_texture_id, float width, float height);
	LEVEL0_API void UI_BeginTooltip();
	LEVEL0_API void UI_EndTooltip();
	LEVEL0_API float UI_GetWindowPos_X();
	LEVEL0_API float UI_GetWindowContentRegionMax_X();
	LEVEL0_API float UI_GetItemRectMax_X();
	LEVEL0_API float UI_GetStyle_ItemSpacing_X();
	LEVEL0_API void UI_PushID(int int_id);
	LEVEL0_API void UI_PopID();

	LEVEL0_API void UI_GetDisplaySize(float* w, float* h);
	LEVEL0_API void UI_Spacing();

	LEVEL0_API bool UI_BeginPopupContextItem(const char* str_id);
	LEVEL0_API void UI_EndPopup();

	LEVEL0_API void UI_SetNextItemWidth(float item_width);

	LEVEL0_API bool UI_BeginTabBar(const char* str_id, int flags);
	LEVEL0_API void UI_EndTabBar();
	LEVEL0_API bool UI_BeginTabItem(const char* label);
	LEVEL0_API void UI_EndTabItem();
	LEVEL0_API void UI_SetCursorPosX(float x);
	LEVEL0_API float UI_GetWindowWidth();

	LEVEL0_API bool UI_WantCaptureMouse();
	LEVEL0_API bool UI_WantCaptureKeyboard();

	LEVEL0_API bool UI_BeginTable(const char* str_id, int column, int flags, float outer_width, float inner_width);
	LEVEL0_API void UI_EndTable();
	LEVEL0_API void UI_TableNextRow();
	LEVEL0_API void UI_TableNextColumn();
	LEVEL0_API void UI_TableHeadersRow();

	LEVEL0_API void UI_BeginDisabled(bool disabled);
	LEVEL0_API void UI_EndDisabled(void);

#ifdef __cplusplus
}
#endif

#endif // GL_CONSOLE_H
