# Physics System

The Tectonic Engine integrates the powerful and widely-used **Bullet Physics** library to handle all collision detection and physics simulations.

## Physics World

*   **Creation:** A physics world is created for each scene, managing all rigid bodies and their interactions.
*   **Gravity:** The global gravity value can be configured via the `gravity` cvar.
*   **Simulation:** The physics world is stepped forward on every frame in the `update_state` function.

## Rigid Bodies and Collision Shapes

The engine uses several types of collision shapes depending on the object's purpose:

*   **Player Controller:** A `btCapsuleShape` is used for the player. This shape is ideal for smooth movement over small obstacles and up stairs. Its angular factor is locked to prevent it from tipping over.
*   **Static Brushes and Models:** For static level geometry, a `btBvhTriangleMeshShape` is used. This provides the most accurate collision representation possible but cannot be used for dynamic (moving) objects.
*   **Dynamic Objects:** For any object with a `mass` greater than 0, a `btConvexHullShape` is generated. This is an approximation of the object's mesh that is required by Bullet for efficient dynamic simulation.

## Interacting with Physics

*   **Editor:** Physics properties like `mass` and `isPhysicsEnabled` can be set directly on models and brushes in the editor.
*   **Noclip:** The `noclip` command completely disables gravity and collision response for the player, allowing free movement through the world for debugging or development.
*   **Forces:** The engine can apply forces and impulses to dynamic objects through the I/O system or directly in code, allowing for physically-based interactions like explosions or impacts.
*   **Buoyancy:** Any brush marked as a `Water` volume will automatically apply buoyancy and drag forces to any dynamic object that enters it.