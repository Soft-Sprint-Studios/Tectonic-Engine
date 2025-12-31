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
#ifndef EDITOR_WINDOWS_H
#define EDITOR_WINDOWS_H

#include "editor_internal.h"

#ifdef __cplusplus
extern "C" {
#endif

	void ScanModelFiles();
	void ScanDocFiles();
	void ScanSoundFiles();
	void ScanMapFiles();
	void FreeModelBrowserEntries();
	void FreeDocFileList();
	void FreeSoundFileList();
	void FreeMapFileList();

	void Editor_RenderModelBrowser(Scene* scene, Engine* engine, Renderer* renderer);
	void Editor_RenderSoundBrowser(Scene* scene);
	void Editor_RenderHelpWindow();
	void Editor_RenderVertexToolsWindow(Scene* scene);
	void Editor_RenderSculptNoisePopup(Scene* scene);
	void Editor_RenderSprinkleToolWindow();
	void Editor_RenderBuildCubemapsWindow(Renderer* renderer, Scene* scene, Engine* engine);
	void Editor_RenderBakeLightingWindow(Scene* scene, Engine* engine);
	void Editor_RenderAboutWindow();
	void Editor_RenderTextureBrowser(Scene* scene);
	void Editor_RenderReplaceTexturesUI(Scene* scene);
	void Editor_RenderFaceEditSheet(Scene* scene, Engine* engine);
	void Editor_RenderArchPropertiesWindow(Scene* scene, Engine* engine);
	void Editor_RenderMapInfoWindow(Scene* scene);
	void Editor_RenderTransformWindow(Scene* scene, Engine* engine);
	void Editor_RenderGoToCoordinatesWindow();
	void Editor_RenderStatusBar();
	void Editor_RenderArchPreview();
	void RenderIOEditor(EntityType type, int index);

#ifdef __cplusplus
}
#endif

#endif