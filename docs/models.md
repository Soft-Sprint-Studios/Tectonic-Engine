# Working with Models

The Tectonic Engine uses the **glTF 2.0** format for 3D models, providing a modern and flexible pipeline for importing assets.

## Supported Formats

`.gltf` (JSON-based) is supported.

## File Location

All model files should be placed in the `models/` directory in the engine's root. The engine will scan this directory to populate the model browser in the editor.

## Physics and Collision

The engine can automatically generate collision geometry for models when they are placed in a map.

*   **Static Models (Mass = 0):** A static triangle mesh (`btBvhTriangleMeshShape`) is generated from the model's vertex data. This is ideal for static level geometry as it is highly accurate.
*   **Dynamic Models (Mass > 0):** A dynamic convex hull (`btConvexHullShape`) is generated. This is an approximation of the model's shape that is required for efficient physics simulation of movable objects.

## Model Properties in the Editor

When a model is selected in the editor, you can modify several properties in the Inspector panel:

*   **Targetname:** A unique name for I/O system scripting.
*   **Transform:** Position, Rotation, and Scale.
*   **Mass:** Determines if the object is static (0) or dynamic (>0).
*   **Physics Enabled:** Toggles whether the object collides with other physics objects.
*   **Tree Sway:** A simple vertex shader effect to simulate wind sway, useful for foliage.
*   **Fade Distance:** Allows models to fade out at a distance to improve performance.