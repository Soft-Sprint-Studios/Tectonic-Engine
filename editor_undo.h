/*
 * Copyright © 2025 Soft Sprint Studios
 * All rights reserved.
 *
 * This file is proprietary and confidential. Unauthorized reproduction,
 * modification, or distribution is strictly prohibited unless explicit
 * written permission is granted by Soft Sprint Studios.
 */
#ifndef EDITOR_UNDO_H
#define EDITOR_UNDO_H

#include "map.h"

#ifdef __cplusplus
extern "C" {
#endif

void Undo_Init();
void Undo_Shutdown();
void Undo_PerformUndo(Scene* scene, Engine* engine);
void Undo_PerformRedo(Scene* scene, Engine* engine);
void Undo_BeginEntityModification(Scene* scene, EntityType type, int index);
void Undo_EndEntityModification(Scene* scene, EntityType type, int index, const char* description);
void Undo_PushCreateEntity(Scene* scene, EntityType type, int index, const char* description);
void Undo_PushDeleteEntity(Scene* scene, EntityType type, int index, const char* description);

#ifdef __cplusplus
}
#endif

#endif // EDITOR_UNDO_H