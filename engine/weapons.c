/*
 * Copyright © 2025 Soft Sprint Studios
 * All rights reserved.
 *
 * This file is proprietary and confidential. Unauthorized reproduction,
 * modification, or distribution is strictly prohibited unless explicit
 * written permission is granted by Soft Sprint Studios.
 */
#include "weapons.h"
#include "gl_console.h"
#include "sound_system.h"
#include <string.h>

typedef struct {
    const char* name;
    float damage;
    float range;
    float fireRate;
    unsigned int fireSound;
} WeaponDef;

typedef struct {
    WeaponType currentWeapon;
    float lastFireTime;
} PlayerWeaponState;

static WeaponDef g_WeaponDefs[WEAPON_COUNT];
static PlayerWeaponState g_PlayerWeaponState;

static void Weapons_FireRaycast(Engine* engine, Scene* scene, const WeaponDef* weapon) {
    Vec3 rayStart = engine->camera.position;
    Vec3 forward = { 
        cosf(engine->camera.pitch) * sinf(engine->camera.yaw), 
        sinf(engine->camera.pitch), 
        -cosf(engine->camera.pitch) * cosf(engine->camera.yaw) 
    };
    vec3_normalize(&forward);
    Vec3 rayEnd = vec3_add(rayStart, vec3_muls(forward, weapon->range));

    RaycastHitInfo hitInfo;
    bool hit = Physics_Raycast(engine->physicsWorld, rayStart, rayEnd, &hitInfo);

    if (hit) {
        if (hitInfo.hitBody) {
            Physics_ApplyImpulse(hitInfo.hitBody, vec3_muls(forward, 1.0f), hitInfo.point);
        }
    }
}

void Weapons_Init(void) {
    g_PlayerWeaponState.currentWeapon = WEAPON_NONE;
    g_PlayerWeaponState.lastFireTime = 0.0f;

    g_WeaponDefs[WEAPON_NONE].name = "Hands";
    g_WeaponDefs[WEAPON_NONE].fireSound = 0;

    g_WeaponDefs[WEAPON_PISTOL].name = "Pistol";
    g_WeaponDefs[WEAPON_PISTOL].damage = 25.0f;
    g_WeaponDefs[WEAPON_PISTOL].range = 1000.0f;
    g_WeaponDefs[WEAPON_PISTOL].fireRate = 0.3f;
    g_WeaponDefs[WEAPON_PISTOL].fireSound = SoundSystem_LoadSound("sounds/pistol_fire.mp3");
}

void Weapons_Shutdown(void) {
    SoundSystem_DeleteBuffer(g_WeaponDefs[WEAPON_PISTOL].fireSound);
}

void Weapons_Update(float deltaTime) {
    if (g_PlayerWeaponState.lastFireTime > 0.0f) {
        g_PlayerWeaponState.lastFireTime -= deltaTime;
    }
}

void Weapons_Switch(WeaponType newWeapon) {
    if (newWeapon >= WEAPON_COUNT || newWeapon == g_PlayerWeaponState.currentWeapon) {
        return;
    }
    g_PlayerWeaponState.currentWeapon = newWeapon;
}

void Weapons_SwitchNext(void) {
    int next_weapon = (g_PlayerWeaponState.currentWeapon + 1) % WEAPON_COUNT;
    Weapons_Switch((WeaponType)next_weapon);
}

void Weapons_SwitchPrev(void) {
    int prev_weapon = (g_PlayerWeaponState.currentWeapon - 1 + WEAPON_COUNT) % WEAPON_COUNT;
    Weapons_Switch((WeaponType)prev_weapon);
}

void Weapons_TryFire(Engine* engine, Scene* scene) {
    if (g_PlayerWeaponState.currentWeapon == WEAPON_NONE) {
        return;
    }
    
    if (g_PlayerWeaponState.lastFireTime > 0.0f) {
        return;
    }

    const WeaponDef* weapon = &g_WeaponDefs[g_PlayerWeaponState.currentWeapon];
    g_PlayerWeaponState.lastFireTime = weapon->fireRate;

    if (weapon->fireSound) {
        SoundSystem_PlaySound(weapon->fireSound, engine->camera.position, 1.0f, 1.0f, 100.0f, false);
    }

    Weapons_FireRaycast(engine, scene, weapon);
}