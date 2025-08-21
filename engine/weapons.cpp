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
#include "weapons.h"
#include "gl_console.h"
#include "sound_system.h"
#include <vector>
#include <memory>
#include <string>

class Weapon {
public:
    Weapon(string name, float damage, float range, float fireRate, unsigned int fireSound)
        : m_name(name), m_damage(damage), m_range(range), m_fireRate(fireRate), m_fireSound(fireSound) {
    }
    virtual ~Weapon() = default;

    virtual void Fire(Engine* engine, Scene* scene) = 0;

    float GetFireRate() const { return m_fireRate; }
    unsigned int GetFireSound() const { return m_fireSound; }

protected:
    string m_name;
    float m_damage;
    float m_range;
    float m_fireRate;
    unsigned int m_fireSound;
};

class Hands : public Weapon {
public:
    Hands() : Weapon("Hands", 0.0f, 0.0f, 0.0f, 0) {}
    void Fire(Engine* engine, Scene* scene) override {}
};

class Pistol : public Weapon {
public:
    Pistol() : Weapon("Pistol", 25.0f, 1000.0f, 0.3f, 0) {
        m_fireSound = SoundSystem_LoadSound("sounds/pistol_fire.mp3");
    }
    ~Pistol() {
        if (m_fireSound != 0) {
            SoundSystem_DeleteBuffer(m_fireSound);
        }
    }

    void Fire(Engine* engine, Scene* scene) override {
        Vec3 rayStart = engine->camera.position;
        Vec3 forward = {
            cosf(engine->camera.pitch) * sinf(engine->camera.yaw),
            sinf(engine->camera.pitch),
            -cosf(engine->camera.pitch) * cosf(engine->camera.yaw)
        };
        vec3_normalize(&forward);
        Vec3 rayEnd = vec3_add(rayStart, vec3_muls(forward, m_range));

        RaycastHitInfo hitInfo;
        if (Physics_Raycast(engine->physicsWorld, rayStart, rayEnd, &hitInfo)) {
            if (hitInfo.hitBody) {
                Physics_ApplyImpulse(hitInfo.hitBody, vec3_muls(forward, 1.0f), hitInfo.point);
            }
        }
    }
};

class WeaponSystem {
public:
    void Init() {
        m_weapons.clear();
        m_weapons.push_back(make_unique<Hands>());
        m_weapons.push_back(make_unique<Pistol>());

        m_currentWeaponIndex = WEAPON_NONE;
        m_lastFireTime = 0.0f;
    }

    void Shutdown() {
        m_weapons.clear();
    }

    void Update(float deltaTime) {
        if (m_lastFireTime > 0.0f) {
            m_lastFireTime -= deltaTime;
        }
    }

    void Switch(WeaponType newWeapon) {
        if (newWeapon >= WEAPON_COUNT) return;
        m_currentWeaponIndex = newWeapon;
    }

    void SwitchNext() {
        m_currentWeaponIndex = (m_currentWeaponIndex + 1) % WEAPON_COUNT;
    }

    void SwitchPrev() {
        m_currentWeaponIndex = (m_currentWeaponIndex - 1 + WEAPON_COUNT) % WEAPON_COUNT;
    }

    void TryFire(Engine* engine, Scene* scene) {
        if (m_currentWeaponIndex == WEAPON_NONE || m_lastFireTime > 0.0f) {
            return;
        }

        Weapon* currentWeapon = m_weapons[m_currentWeaponIndex].get();
        if (!currentWeapon) return;

        m_lastFireTime = currentWeapon->GetFireRate();

        unsigned int fireSound = currentWeapon->GetFireSound();
        if (fireSound != 0) {
            SoundSystem_PlaySound(fireSound, engine->camera.position, 1.0f, 1.0f, 100.0f, false);
        }

        currentWeapon->Fire(engine, scene);
    }

private:
    vector<unique_ptr<Weapon>> m_weapons;
    int m_currentWeaponIndex;
    float m_lastFireTime;
};

static WeaponSystem g_weaponSystem;

extern "C" {
    void Weapons_Init(void) {
        g_weaponSystem.Init();
    }

    void Weapons_Shutdown(void) {
        g_weaponSystem.Shutdown();
    }

    void Weapons_Update(float deltaTime) {
        g_weaponSystem.Update(deltaTime);
    }

    void Weapons_Switch(WeaponType newWeapon) {
        g_weaponSystem.Switch(newWeapon);
    }

    void Weapons_SwitchNext(void) {
        g_weaponSystem.SwitchNext();
    }

    void Weapons_SwitchPrev(void) {
        g_weaponSystem.SwitchPrev();
    }

    void Weapons_TryFire(Engine* engine, Scene* scene) {
        g_weaponSystem.TryFire(engine, scene);
    }
}