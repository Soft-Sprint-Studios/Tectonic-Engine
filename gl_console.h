/*
 * Copyright © 2025 Soft Sprint Studios
 * All rights reserved.
 *
 * This file is proprietary and confidential. Unauthorized reproduction,
 * modification, or distribution is strictly prohibited unless explicit
 * written permission is granted by Soft Sprint Studios.
 */
#ifndef GL_CONSOLE_H
#define GL_CONSOLE_H

#include <SDL.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

	void UI_Init(SDL_Window* window, SDL_GLContext context);
	void UI_Shutdown();
	void UI_ProcessEvent(SDL_Event* event);
	void UI_BeginFrame();
	void UI_EndFrame(SDL_Window* window);

	void Console_Toggle();
	void Console_Draw();
	void Console_Printf(const char* fmt, ...);
	bool Console_IsVisible();
	void UI_RenderGameHUD(float fps, float px, float py, float pz);
	typedef void (*command_callback_t)(int argc, char** argv);
	void Console_SetCommandHandler(command_callback_t handler);

	bool UI_Begin(const char* name, bool* p_open);
	void UI_End();
	bool UI_BeginMainMenuBar();
	void UI_EndMainMenuBar();
	bool UI_BeginMenu(const char* label, bool enabled);
	void UI_EndMenu();
	bool UI_MenuItem(const char* label, const char* shortcut, bool selected, bool enabled);
	void UI_Text(const char* fmt, ...);
	void UI_Separator();
	bool UI_CollapsingHeader(const char* label, int flags);
	bool UI_Selectable(const char* label, bool selected);
	bool UI_Button(const char* label);
	bool UI_DragFloat3(const char* label, float v[3], float v_speed, float v_min, float v_max);
	bool UI_DragFloat(const char* label, float* v, float v_speed, float v_min, float v_max);
	bool UI_DragFloat2(const char* label, float v[2], float v_speed, float v_min, float v_max);
	bool UI_DragInt(const char* label, int* v, float v_speed, int v_min, int v_max);
	bool UI_ColorEdit3(const char* label, float col[3]);
	void UI_Image(void* user_texture_id, float width, float height);
	bool UI_IsWindowFocused();
	bool UI_IsWindowHovered();
	bool UI_IsMouseDragging(int button);
	void UI_GetContentRegionAvail(float* w, float* h);
	void UI_GetWindowContentRegionMin(float* x, float* y);
	void UI_GetMousePos(float* x, float* y);
	void UI_GetWindowPos(float* x, float* y);
	void UI_GetWindowSize(float* w, float* h);
	void UI_PushStyleVar_WindowPadding(float val_x, float val_y);
	void UI_PopStyleVar(int count);
	void UI_InputText(const char* label, char* buf, size_t buf_size);

	bool UI_BeginChild(const char* str_id, float width, float height, bool border, int flags);
	bool UI_RadioButton(const char* label, bool active);
	bool UI_Combo(const char* label, int* current_item, const char* const items[], int items_count, int popup_max_height_in_items);
	void UI_SetNextWindowPos(float x, float y);
	void UI_SetNextWindowSize(float w, float h);
	void UI_EndChild();
	void UI_SameLine();

	bool UI_IsItemHovered();
	bool UI_ListBox(const char* label, int* current_item, const char* const* items, int items_count, int height_in_items);
	float UI_GetMouseWheel();
	void UI_GetMouseDragDelta(int button, float lock_threshold, float* dx, float* dy);
	void UI_ResetMouseDragDelta(int button);

	bool UI_IsItemActivated(void);
	bool UI_IsItemDeactivatedAfterEdit(void);
    bool UI_Checkbox(const char* label, bool* v);

	bool UI_ImageButton(const char* str_id, unsigned int user_texture_id, float width, float height);
	void UI_BeginTooltip();
	void UI_EndTooltip();
	float UI_GetWindowPos_X();
	float UI_GetWindowContentRegionMax_X();
	float UI_GetItemRectMax_X();
	float UI_GetStyle_ItemSpacing_X();
	void UI_PushID(int int_id);
	void UI_PopID();

    void UI_GetDisplaySize(float* w, float* h);
	void UI_Spacing();

	bool UI_BeginPopupContextItem(const char* str_id);
	void UI_EndPopup();

	void UI_SetNextItemWidth(float item_width);

#ifdef __cplusplus
}
#endif

#endif // GL_CONSOLE_H