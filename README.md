# Tectonic Engine
A modern, feature-rich 3D game engine written in pure C, inspired by classic FPS engines.

# Tectonic Engine - Feature Set
An overview of the core technologies and rendering capabilities of the Tectonic Engine.

# Rendering & Lighting
Physically-Based Rendering (PBR) Pipeline: Core rendering is built on a deferred PBR model using a metallic/roughness workflow, enabling realistic material interactions with light.

Normal Mapping: Supported on both world geometry (brushes) and models for high-frequency surface detail.

Parallax Occlusion Mapping: Height maps can be used to create the illusion of depth and self-shadowing on surfaces, providing a more convincing 3D effect than standard normal mapping.

# Dynamic Lighting System: Supports a large number of fully dynamic point and spot lights with configurable color, intensity, and radius.
Light Styles & Presets: Lights can utilize preset animation styles for effects such as flickering, strobing, and pulsing.

Light Cookies: Spotlights support projected textures (cookies) for gobo and patterned lighting effects.

Shadow Mapping: Real-time dynamic shadows are supported for all light types.

Omnidirectional Shadows: Point lights use cubemap shadow maps for 360-degree shadow coverage.

Projected Shadows: Spotlights and the sun use standard 2D shadow maps for efficient directional shadowing.

Percentage-Closer Filtering (PCF): Implemented for soft shadow edges.

Global Illumination via Virtual Point Lights (VPLs): The engine supports real-time, one-bounce global illumination by generating hundreds of virtual point lights from surfaces hit by direct light, providing realistic color bleeding and indirect lighting.

High-Dynamic Range (HDR) Lighting: The entire lighting pipeline operates in HDR.

Auto-Exposure: The final image exposure is automatically adjusted based on scene luminance, simulating the human eye's adaptation to light and dark areas.

Bloom: Bright areas of the scene realistically "bloom" into neighboring areas, enhancing the HDR effect.

Screen-Space Ambient Occlusion (SSAO): Adds contact shadows and depth to scenes by darkening creases, corners, and ambiently lit areas.

Volumetric Lighting: Supports "god rays" and light shafts from the sun and other dynamic lights, rendering visible beams of light through the environment.

Water Rendering: Features real-time environmental reflections using cubemaps, Fresnel effects for angle-dependent reflectivity, and animated surface normals for a dynamic appearance.

Reflection Probes: Static cubemaps can be baked from any point in the world and applied to nearby geometry for accurate, parallax-corrected local reflections on PBR materials and water.

Procedural Skybox: A dynamic skybox system with procedural atmospheric scattering (Rayleigh & Mie) and animated clouds, removing the need for static 2D skybox images.

# Post-Processing Stack
Includes a suite of configurable effects such as Film Grain, Chromatic Aberration, CRT screen curvature, Vignette, Sharpening, and Depth of Field.

# Engine & World
Brush-Based World Construction: World geometry is built using classic CSG (Constructive Solid Geometry) brush techniques, allowing for rapid and robust level design.

Advanced Particle System: Capable of emitting sprites with configurable properties like gravity, velocity, color-over-life, and size-over-life, loaded from simple .par definition files.

Physics Integration: Utilizes the Bullet Physics library for a robust simulation environment.

Collision Primitives: Supports player capsules, static triangle meshes, dynamic and static convex hulls.

Dynamic Objects: Brushes and models can be configured with mass to become fully dynamic physics objects.

Input/Output (I/O) Logic System: A powerful, trigger-based system inspired by Source's entity I/O allows for complex scripting and level interactivity without writing code. Entities can fire outputs (e.g., OnTrigger, OnTimer) which connect to the inputs of other entities (TurnOn, PlaySound).

Hierarchical Undo/Redo System: The integrated editor features a robust, multi-level undo/redo stack for all entity creation, deletion, and modification actions.

# Platform & Tools
Cross-Platform Support: The engine and its tooling are built with modern CMake for compilation on Windows (32/64-bit, MSVC) and Linux (GCC/Clang).

In-Engine Editor: A comprehensive, real-time "Hammer-style" editor is built directly into the engine.

Multi-Viewport Editing: Features classic 3D perspective, top, side, and front 2D orthographic views.

CSG Tools: Includes tools for brush creation, clipping, and vertex manipulation.

Vertex Painting & Sculpting: Terrain and complex brushes can be directly painted with vertex colors for texture blending or sculpted for organic shapes.

Developer Console & CVars: A Quake-style dropdown console provides direct access to engine commands and tunable "CVars" for runtime configuration.
