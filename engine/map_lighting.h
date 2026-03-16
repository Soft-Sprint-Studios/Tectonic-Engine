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
#ifndef MAP_LIGHTING_H
#define MAP_LIGHTING_H

#include "map.h"


    void Brush_GenerateLightmapAtlas(Brush* b, const Char* map_name_sanitized, Int brush_index, Int resolution);
    void SceneObject_LoadVertexLighting(SceneObject* obj, Int index, const Char* mapPath);
    void SceneObject_LoadVertexDirectionalLighting(SceneObject* obj, Int index, const Char* mapPath);
    void SceneObject_LoadLightmaps(SceneObject* obj, Int index, const Char* mapPath);
    void Decal_LoadLightmaps(Decal* decal, const Char* map_name_sanitized, Int decal_index);
    void Scene_LoadAmbientProbes(Scene* scene);


#endif // MAP_LIGHTING_H