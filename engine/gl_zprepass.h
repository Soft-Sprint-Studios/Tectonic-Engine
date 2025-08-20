#pragma once
#ifndef GL_ZPREPASS_H
#define GL_ZPREPASS_H

#include "map.h"

#ifdef __cplusplus
extern "C" {
#endif

void Zprepass_Init(Renderer* renderer);
void Zprepass_Shutdown(Renderer* renderer);
void Zprepass_Render(Renderer* renderer, Scene* scene, Engine* engine, const Mat4* view, const Mat4* projection);

#ifdef __cplusplus
}
#endif

#endif // GL_ZPREPASS_H