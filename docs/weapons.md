# Weapon System

**(Note: This system is currently under development and subject to change.)**

The Tectonic Engine includes a foundational weapon system designed for handling player-held items and their actions, such as firing projectiles.

## System Overview

The weapon system, managed by `weapons.c`, defines the properties and behaviors of different weapons available to the player. It handles weapon switching, firing rates, and the immediate effects of firing, such as raycasting for hit detection.

## Weapon Definitions

Weapons are defined internally in `Weapons_Init()`. Each weapon has a set of core properties that dictate its behavior in the game.

| Weapon | Damage | Range | Fire Rate (delay) |
| :--- | :--- | :--- | :--- |
| **Hands** | 0 | 0 | N/A |
| **Pistol** | 25 | 1000 | 0.3s |

*   **Damage:** The amount of damage dealt upon a successful hit.
*   **Range:** The maximum distance the weapon's attack can reach.
*   **Fire Rate:** The delay in seconds between consecutive shots.

## Firing Mechanism

When `Weapons_TryFire()` is called, the system performs the following actions:

1.  **Fire Rate Check:** It checks if enough time has passed since the last shot, based on the weapon's `fireRate`.
2.  **Sound:** It plays the weapon's defined `fireSound` at the player's location.
3.  **Raycast:** The system performs an instantaneous raycast from the camera's position forward into the scene, up to the weapon's `range`.
4.  **Hit Detection:**
    *   If the raycast hits a physics-enabled object, it applies a small impulse to simulate an impact force.

## Player State

The player's current weapon state is managed by a `PlayerWeaponState` struct, which keeps track of:

*   `currentWeapon`: The weapon currently equipped by the player.
*   `lastFireTime`: A cooldown timer to enforce the weapon's fire rate.

## Switching Weapons

The player can switch between available weapons using dedicated functions:

*   `Weapons_SwitchNext()`: Cycles to the next weapon in the list.
*   `Weapons_SwitchPrev()`: Cycles to the previous weapon.
*   These are typically bound to the mouse wheel or number keys.