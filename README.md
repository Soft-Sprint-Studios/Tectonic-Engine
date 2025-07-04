# Tectonic Engine
A modern, feature-rich 3D game engine written in pure C, inspired by classic FPS engines.

# About The Project
Tectonic Engine is a proprietary, closed-source game engine developed by Soft Sprint Studios. Built from the ground up in C, it combines a performant, old-school architectural approach with a modern, physically-based deferred rendering pipeline.
The core philosophy is to provide a powerful, data-driven framework for creating games, supported by a robust, integrated 3D editor for a seamless content creation workflow.

# Rendering & Visuals
Deferred Rendering Pipeline: Efficiently handles scenes with a large number of dynamic lights by decoupling geometry and lighting passes.
Physically-Based Rendering (PBR): Supports metallic/roughness workflow for realistic material definition.

# Advanced Lighting:
Dynamic Lighting: Supports hundreds of dynamic point and spot lights.
Global Illumination: Uses Virtual Point Lights (VPLs) for real-time, single-bounce indirect lighting.
Dynamic Sun & Sky: Procedural skybox with atmospheric scattering and a dynamic sun light.

# Advanced Shadows:
Dynamic Shadows: Real-time shadows for all light types (Point, Spot, and Sun).
PCF Filtering: Basic percentage-closer filtering for soft shadow edges.

# Post-Processing Stack:
Auto-Exposure (Eye Adaptation): Automatically adjusts scene brightness based on luminance.
Bloom: High-quality bloom for emissive surfaces and bright lights.
Volumetric Lighting: Creates visible light shafts and "god rays" from light sources.
Screen-Space Ambient Occlusion (SSAO): Adds contact shadows and depth to scenes.
Artistic Effects: Includes customizable film grain, scanlines, chromatic aberration, and vignette effects.

# Advanced Mapping Techniques:
Contact Refinment Parallax Mapping: Adds illusion of 3d surfaces
Parallax Interiors: Creates the illusion of a full 3D room behind a simple quad, perfect for window details.
Dynamic Water: Renders realistic water surfaces with waves, reflections, and lighting interaction.

# Integrated Editor
Full 3D Level Editor: Create and edit levels directly within the engine.
Multi-Viewport Interface: Classic four-pane view (3D Perspective, Top, Front, Side) for precise object placement.
Brush-Based Geometry: Build complex level geometry using CSG-style additive brushes, similar to editors like Valve Hammer or id Radiant.

# Real-Time Manipulation:
Standard translation, rotation, and scale gizmos for all entities.
Direct vertex and face manipulation tools for fine-tuning brush geometry.
Asset Browsers: In-editor browsers for previewing and placing models and applying textures.
Entity Inspector: A detailed properties panel to edit any selected entity in real-time.
Robust Undo/Redo System: A comprehensive undo stack makes editing safe and non-destructive.

# Core Engine & Gameplay
Cross-Platform: Architected for and tested on both Windows and Linux.
Physics Engine: Powered by Bullet Physics for robust collision detection and rigid body simulation. Supports dynamic objects, static geometry, and a capsule-based character controller.
I/O and Trigger System: A powerful input/output system allows for creating complex entity interactions and game logic without changing engine code.
CVar System: Quake-style console with a fully-featured CVar (console variable) system for tweaking engine parameters at runtime.
Particle System: Data-driven particle emitters for effects like fire, smoke, and explosions.
