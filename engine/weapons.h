/*
 * Copyright © 2025 Soft Sprint Studios
 * All rights reserved.
 *
 * This file is proprietary and confidential. Unauthorized reproduction,
 * modification, or distribution is strictly prohibited unless explicit
 * written permission is granted by Soft Sprint Studios.
 */
#ifndef WEAPONS_H
#define WEAPONS_H

//----------------------------------------//
// Brief: Very basic weapons
//----------------------------------------//

#include "map.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    WEAPON_NONE,
    WEAPON_PISTOL,
    WEAPON_COUNT
} WeaponType;

void Weapons_Init(void);
void Weapons_Shutdown(void);
void Weapons_Update(float deltaTime);
void Weapons_Switch(WeaponType newWeapon);
void Weapons_SwitchNext(void);
void Weapons_SwitchPrev(void);
void Weapons_TryFire(Engine* engine, Scene* scene);

#ifdef __cplusplus
}
#endif

#endif