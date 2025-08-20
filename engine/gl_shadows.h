#pragma once
#ifndef GL_SHADOWS_H
#define GL_SHADOWS_H

#include "map.h"

#ifdef __cplusplus
extern "C" {
#endif

#define SUN_SHADOW_MAP_SIZE 4096

struct Engine;
struct Scene;
struct Renderer;

void Shadows_Init(Renderer* renderer);
void Shadows_Shutdown(Renderer* renderer);
void Shadows_RenderPointAndSpot(Renderer* renderer, Scene* scene, Engine* engine);
void Shadows_RenderSun(Renderer* renderer, Scene* scene, const Mat4* sunLightSpaceMatrix);

#ifdef __cplusplus
}
#endif

#endif // GL_SHADOWS_H