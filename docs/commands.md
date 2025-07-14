# Console Commands and Variables

The Tectonic Engine can be controlled and configured at runtime through its powerful console. The console can be opened by pressing the backquote (`) key.

## Console Commands

These are actions that can be executed from the console.

| Command | Description |
| :--- | :--- |
| `help`, `cmdlist` | Shows a list of all available commands and cvars. |
| `edit` | Toggles the real-time editor mode. |
| `quit`, `exit` | Exits the engine. |
| `setpos <x> <y> <z>` | Teleports the player to a specified XYZ coordinate. |
| `noclip` | Toggles player collision and gravity, allowing free movement. |
| `bind "<key>" "<command>"`| Binds a key to a command or series of commands. |
| `unbind "<key>"` | Removes a key binding. |
| `unbindall` | Removes all key bindings. |
| `map <mapname>` | Loads the specified map file (e.g., `map mylevel`). |
| `maps` | Lists all available `.map` files in the root directory. |
| `disconnect` | Disconnects from the current map and returns to the main menu. |
| `download <url>` | Downloads a file from a URL into the `downloads/` folder. |
| `ping <hostname>` | Pings a network host to check connectivity. |
| `build_cubemaps` | Builds cubemaps for all reflection probes in the current map. |
| `screenshot` | Saves a screenshot of the current view to the `screenshots/` folder. |

## Console Variables (Cvars)

These are settings that can be viewed and changed from the console. To set a cvar, type `<cvar_name> <value>`.

### Rendering
| Cvar | Default | Description |
| :--- | :--- | :--- |
| `r_vsync` | 1 | Enable or disable vertical sync (0=off, 1=on). |
| `fps_max` | 300 | Maximum frames per second. 0 for unlimited. VSync overrides this. |
| `r_autoexposure` | 1 | Enable auto-exposure (tonemapping). |
| `r_autoexposure_speed` | 1.0 | Adaptation speed for auto-exposure. |
| `r_autoexposure_key` | 0.1 | The middle-grey value the scene luminance will adapt towards. |
| `r_ssao` | 1 | Enable Screen-Space Ambient Occlusion. |
| `r_bloom` | 1 | Enable or disable the bloom effect. |
| `r_volumetrics` | 1 | Enable or disable volumetric lighting. |
| `r_depth_aa` | 1 | Enable Depth/Normal based Anti-Aliasing. |
| `r_faceculling` | 1 | Enable back-face culling for main render pass. |
| `r_wireframe` | 0 | Render geometry in wireframe mode. |
| `r_shadows` | 1 | Master switch for all dynamic shadows. |
| `r_vpl` | 1 | Master switch for Virtual Point Light Global Illumination. |
| `r_vpl_point_count` | 64 | Number of VPLs to generate per point light. |
| `r_vpl_spot_count` | 64 | Number of VPLs to generate per spot light. |
| `r_vpl_static` | 0 | Generate VPLs only once on map load for static GI. |
| `r_shadow_map_size` | 1024 | Resolution for point/spot light shadow maps (e.g., 512, 1024). |
| `r_relief_mapping` | 1 | Enable relief mapping (parallax). |
| `r_colorcorrection` | 1 | Enable or disable color correction. |
| `r_sun_shadow_distance`| 50.0 | Orthographic size for the sun's shadow map frustum. |
| `r_texture_quality` | 5 | Texture quality (1=very low, 5=very high). |
| `fov_vertical` | 55 | The vertical field of view in degrees. |
| `r_motionblur` | 0 | Enable camera and object motion blur. |

### Gameplay & UI
| Cvar | Default | Description |
| :--- | :--- | :--- |
| `noclip` | 0 | Toggles player collision and gravity. |
| `g_speed` | 6.0 | Player walking speed. |
| `g_sprint_speed` | 8.0 | Player sprinting speed. |
| `g_accel` | 15.0 | Player acceleration. |
| `g_friction` | 5.0 | Player friction. |
| `gravity` | 9.81 | The strength of gravity. |
| `crosshair` | 1 | Show a crosshair in the center of the screen. |
| `show_fps` | 0 | Show FPS counter in the top-left corner. |
| `show_pos` | 0 | Show player position in the top-left corner. |
| `volume` | 2.5 | Master volume for the game (0.0 to 4.0). |
| `timescale` | 1.0 | Controls the speed of the game. 1.0 is normal speed. |

### Debug Views
| Cvar | Default | Description |
| :--- | :--- | :--- |
| `r_debug_albedo` | 0 | Show G-Buffer albedo. |
| `r_debug_normals` | 0 | Show G-Buffer view-space normals. |
| `r_debug_position` | 0 | Show G-Buffer view-space positions. |
| `r_debug_metallic` | 0 | Show PBR metallic channel. |
| `r_debug_roughness` | 0 | Show PBR roughness channel. |
| `r_debug_ao` | 0 | Show screen-space ambient occlusion buffer. |
| `r_debug_velocity` | 0 | Show motion vector velocity buffer. |
| `r_debug_volumetric`| 0 | Show volumetric lighting buffer. |
| `r_debug_bloom` | 0 | Show the bloom brightness mask texture. |
| `r_debug_vpl` | 0 | Show G-Buffer indirect illumination. |