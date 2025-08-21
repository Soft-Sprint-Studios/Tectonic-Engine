# Tectonic Engine
A modern, feature-rich 3D game engine written in C, inspired by classic FPS engines.

# Tectonic Engine - Feature Set
An overview of the core technologies and rendering capabilities of the Tectonic Engine.

# Rendering & Lighting
- Physically-Based Rendering (PBR) Pipeline: Core rendering is built on a deferred PBR model using a metallic/roughness workflow, enabling realistic material interactions with light.

- Normal Mapping: Supported on both world geometry (brushes) and models for high-frequency surface detail.

- Relief Mapping: Height maps can be used to create the illusion of depth and self-shadowing on surfaces, providing a more convincing 3D effect than standard normal mapping.

- Tesselation Displacement Mapping: Height maps can be used to create the procedual geometry on surfaces using tesselation.

# Dynamic Lighting System: Supports a large number of fully dynamic point and spot lights with configurable color, intensity, and radius.
- Light Styles & Presets: Lights can utilize preset animation styles for effects such as flickering, strobing, and pulsing.

- Light Cookies: Spotlights support projected textures (cookies) for gobo and patterned lighting effects.

- Shadow Mapping: Real-time dynamic shadows are supported for all light types.

- Omnidirectional Shadows: Point lights use cubemap shadow maps for 360-degree shadow coverage.

- Projected Shadows: Spotlights and the sun use standard 2D shadow maps for efficient directional shadowing.

- Percentage-Closer Filtering (PCF): Implemented for soft shadow edges.

- High-Dynamic Range (HDR) Lighting: The entire lighting pipeline operates in HDR.

- Auto-Exposure: The final image exposure is automatically adjusted based on scene luminance, simulating the human eye's adaptation to light and dark areas.

- Bloom: Bright areas of the scene realistically "bloom" into neighboring areas, enhancing the HDR effect.

- Screen-Space Ambient Occlusion (SSAO): Adds contact shadows and depth to scenes by darkening creases, corners, and ambiently lit areas.

- Volumetric Lighting: Supports "god rays" and light shafts from the sun and other dynamic lights, rendering visible beams of light through the environment.

- Water Rendering: Features environmental reflections using Planar Reflections, Fresnel effects for angle-dependent reflectivity, and flowmapping for a dynamic appearance.

- Glass & Refraction: Supports translucent glass surfaces that refract the scene behind them, creating a realistic distortion effect. along with a reflective glass variant that uses Planar Reflections

- Reflection Probes: Static cubemaps can be baked from any point in the world and applied to nearby geometry for accurate, parallax-corrected local reflections on PBR materials and water.

- Procedural Skybox: A dynamic skybox system with procedural atmospheric scattering (Rayleigh & Mie) and animated clouds, removing the need for static 2D skybox images.

# Post-Processing Stack
- Includes a suite of configurable effects such as Film Grain, Chromatic Aberration, CRT screen curvature, Vignette, Sharpening, Black and White, Invert Colors and Depth of Field.

# Lightmapping
- **Intel Embree-Powered Ray Tracing:** Static lightmaps for world geometry are baked using Intel Embree’s high-performance ray tracing kernels for accurate lighting.  

- **Bounced Lighting:** Supports up to **4 light bounces** per surface, simulating realistic indirect illumination.  

- **Directional Lightmaps:** Stores per-texel normal and directional information to enhance PBR shading on static brushes.  

- **Vertex Lightmaps for Models:** Static models use **vertex lightmaps** instead of full texture lightmaps, providing precomputed ambient lighting while remaining efficient. 
 
- **Integration with PBR Materials:** Baked lightmaps include diffuse and specular contributions, fully compatible with the engine’s physically-based rendering pipeline.

# Engine & World
- Brush-Based World Construction: World geometry is built using classic CSG (Constructive Solid Geometry) brush techniques, allowing for rapid and robust level design.

- Advanced Particle System: Capable of emitting sprites with configurable properties like gravity, velocity, color-over-life, and size-over-life, loaded from simple .par definition files.

- Physics Integration: Utilizes the Bullet Physics library for a robust simulation environment.

- Collision Primitives: Supports player capsules, static triangle meshes, dynamic and static convex hulls.

- Dynamic Objects: Brushes and models can be configured with mass to become fully dynamic physics objects.

- Input/Output (I/O) Logic System: A powerful, trigger-based system allows for complex scripting and level interactivity without writing code. Entities can fire outputs (e.g., OnTrigger, OnTimer) which connect to the inputs of other entities (TurnOn, PlaySound).

- Hierarchical Undo/Redo System: The integrated editor features a robust, multi-level undo/redo stack for all entity creation, deletion, and modification actions.

# Platform & Tools
- Cross-Platform Support: The engine and its tooling are built with modern CMake for compilation on Windows (32/64-bit, MSVC) and Linux (GCC).

- In-Engine Editor: A comprehensive, real-time "Hammer-style" editor is built directly into the engine.

- Multi-Viewport Editing: Features classic 3D perspective, top, side, and front 2D orthographic views.

- CSG Tools: Includes tools for brush creation, clipping, and vertex manipulation.

- Splat texture mapping Painting & Sculpting: brushes can be directly painted with splat texture mapping for texture blending or sculpted for organic shapes.

- Developer Console & CVars: A console provides direct access to engine commands and tunable "CVars" for runtime configuration.

# How to get running

Build the engine or get files from github runner
After that copy all files from x64/release (contains base game files) into directory with the compiled dlls and exe, and then paste the dlls from x64_dlls (or x86 dlls if on 32 bit and linux if you are on linux)
Run launcher.exe

# Tech demos

Are in the playlist at https://www.youtube.com/playlist?list=PLW7ec7Brk4oSenQSdZH5fZax2Hn5XUrUO

# License

Only the following folders are under the main project license (**MIT**):

- `engine/`
- `launcher/`
- `x64/`
- `Tools/`
- `docs/`

All other folders are third-party dependencies and fall under their respective open-source licenses.

---

## Third-Party Libraries and Licenses

| Folder                   | Library / Purpose                          | License                            |
|--------------------------|--------------------------------------------|-------------------------------------|
| `glew-2.1.0/`            | OpenGL Extension Wrangler                  | BSD 3-Clause                        |
| `imgui-master/`          | Dear ImGui GUI Library                     | MIT License                         |
| `SDL2_image-2.8.8/`      | SDL2 Image Addon                           | zlib License                        |
| `SDL2_ttf-2.24.0/`       | SDL2 TrueType Font Addon                   | zlib License                        |
| `sdl/`                   | SDL2 Core                                  | zlib License                        |
| `bullet/`                | Bullet Physics                             | zlib License                        |
| `cgltf/`                 | GLTF Loader                                | MIT License                         |
| `mikktspace/`            | Tangent Space Generator                    | zlib License                        |
| `minimp3/`               | MP3 Decoder                                | Unlicense or MIT                    |
| `minivorbis/`            | Ogg Vorbis Decoder                         | BSD-like                            |
| `embree-4.4.0/`          | Ray Tracing Kernels (Intel Embree)         | Apache 2.0 License                  |
| `oidn-2.3.3/`            | Intel Open Image Denoise                   | Apache 2.0 License                  |
| `pl_mpeg/`               | MPEG1 Video Decoder                        | MIT License                         |
| `openal-soft-1.24.3/`    | Audio Library                              | LGPL 2.1                            |
| `stb/`                   | Image, Audio, and Font Utilities           | Public Domain or MIT                |
| `sentry/`                | Crash Reporting SDK                        | MIT License                         |
| `fltk/`                  | Fast Light Toolkit GUI Library             | LGPL                                |
