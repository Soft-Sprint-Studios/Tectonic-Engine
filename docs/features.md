# Tectonic Engine Features

This is a high-level overview of the key features and capabilities of the Tectonic Engine.

## Rendering

*   **Deferred Rendering Pipeline:** Allows for advanced effects like SSAO.
*   **Physically Based Rendering (PBR):** Uses a metallic/roughness workflow for realistic material definition.
*   **Dynamic Lighting:** Supports dynamic point lights, spot lights, and a global sun.
*   **Advanced Shadows:** Includes shadow maps for spot/point lights and cascaded shadow mapping for the sun.
*   **Global Illumination:** A real-time GI solution using Virtual Point Lights (VPLs).
*   **Post-Processing:** A full stack of effects including:
    *   Auto-Exposure (HDR)
    *   Bloom
    *   Screen-Space Ambient Occlusion (SSAO)
    *   Volumetric Lighting
    *   Depth of Field (DoF)
    *   Camera Motion Blur
    *   Color Correction (LUTs)
    *   CRT/Vignette/Grain/Lens Flare effects
*   **Advanced Mapping:** Supports Relief mapping for detailed surfaces.
*   **Water Rendering:** Includes refraction, reflection, and animated normal maps.
*   **Glass Rendering:** Simulates realistic refraction for glass surfaces.

## Editor

*   **Real-Time, In-Engine:** Edit the world while the game is running.
*   **CSG-Style Brushes:** Quickly block out levels using primitive brush geometry (blocks, cylinders, wedges).
*   **Vertex and Face Manipulation:** Fine-tune brush geometry down to the individual vertex.
*   **Comprehensive Inspector:** Edit properties for any selected entity, from models to lights.
*   **Map I/O:** Save and load scenes to the engine's `.map` format.
*   **Specialized Tools:** Includes a texture browser, model browser, clipping tool, vertex painting/sculpting, and a "sprinkle" tool for placing foliage.

## Physics

*   **Bullet Physics:** Integrated with the powerful open-source Bullet Physics library.
*   **Collision Shapes:** Supports capsules (player), convex hulls (brushes, dynamic models), and triangle meshes (static models).
*   **Dynamic and Static Objects:** Easily configure objects to be movable or part of the static world.
*   **Buoyancy:** Dynamic objects realistically float in water volumes.

## Audio

*   **OpenAL Backend:** A robust, cross-platform 3D audio system.
*   **Format Support:** Natively loads `.wav` and `.mp3` files.
*   **Real-Time DSP:** A multi-threaded Digital Signal Processing system provides environmental reverb. DSP Zones can be placed in the editor to define reverb presets for different areas.

## Core Systems

*   **Cross-Platform:** Supports Windows and Linux.
*   **Input/Output (I/O) System:** A flexible, targetname-based system for creating complex entity interactions and game logic without traditional scripting.
*   **Cvar and Command System:** A powerful console for runtime configuration and commands.
*   **Developer Tools:** Includes Discord RPC integration and Sentry crash reporting.