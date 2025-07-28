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