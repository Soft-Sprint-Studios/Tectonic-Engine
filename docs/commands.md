# Console Commands and Variables

The Tectonic Engine can be controlled and configured at runtime through its powerful console. The console can be opened by pressing the backquote (`) key.

## Console Commands

| Command                   | Description                                              |
|--------------------------|----------------------------------------------------------|
| `help`, `cmdlist`         | Shows a list of all available commands and cvars.       |
| `edit`                    | Toggles the real-time editor mode.                       |
| `quit`, `exit`            | Exits the engine.                                        |
| `setpos <x> <y> <z>`      | Teleports the player to a specified XYZ coordinate. (Cheat) |
| `noclip`                  | Toggles player collision and gravity. (Cheat)           |
| `bind "<key>" "<command>"`| Binds a key to a command or series of commands.          |
| `unbind "<key>"`          | Removes a key binding.                                   |
| `unbindall`               | Removes all key bindings.                               |
| `map <mapname>`           | Loads the specified map file.                            |
| `maps`                    | Lists all available `.map` files in the root directory. |
| `disconnect`              | Disconnects from the current map and returns to main menu. |
| `save`                    | Saves the current game state.                            |
| `load`                    | Loads a saved game state.                                |
| `build_lighting [res] [bounces]` | Builds static lighting for the scene.              |
| `download <url>`          | Downloads a file from a URL.                             |
| `ping <hostname>`         | Pings a network host to check connectivity.             |
| `build_cubemaps [res]`    | Builds cubemaps for all reflection probes.               |
| `screenshot`              | Saves a screenshot of the current view to disk.         |
| `exec <script>`           | Executes a script file from the root directory.          |
| `echo <message>`          | Prints a message to the console.                         |
| `clear`                   | Clears the console text.                                |

---

## Console Variables (Cvars)

### Rendering

| Cvar                 | Default | Description                                         |
|----------------------|---------|-----------------------------------------------------|
| `r_vsync`            | 1       | Enable vertical sync (0=off, 1=on).                 |
| `fps_max`            | 300     | Maximum frames per second. 0 = unlimited.           |
| `r_autoexposure`     | 1       | Enable auto-exposure (0=off, 1=on).                 |
| `r_autoexposure_speed` | 1.0    | Adaptation speed for auto-exposure.                  |
| `r_autoexposure_key` | 0.1     | Middle-grey value for auto-exposure.                 |
| `r_ssao`             | 1       | Enable Screen-Space Ambient Occlusion (0=off, 1=on).|
| `r_bloom`            | 1       | Enable bloom effect (0=off, 1=on).                   |
| `r_volumetrics`      | 1       | Enable volumetric lighting (0=off, 1=on).            |
| `r_faceculling`      | 1       | Enable back-face culling (0=off, 1=on).              |
| `r_zprepass`         | 1       | Enable Z-prepass (0=off, 1=on).                      |
| `r_wireframe`        | 0       | Render geometry in wireframe mode (0=off, 1=on).     |
| `r_shadows`          | 1       | Enable dynamic shadows (0=off, 1=on).                |
| `r_shadow_distance_max` | 100.0 | Max shadow casting distance.                         |
| `r_shadow_map_size`  | 1024    | Shadow map resolution (e.g., 512, 1024).             |
| `r_relief_mapping`   | 1       | Enable relief mapping (0=off, 1=on).                  |
| `r_colorcorrection`  | 1       | Enable color correction (0=off, 1=on).                |
| `r_vignette`         | 1       | Enable vignette effect (0=off, 1=on).                 |
| `r_chromaticabberation` | 1     | Enable chromatic aberration (0=off, 1=on).           |
| `r_dof`              | 1       | Enable depth of field (0=off, 1=on).                  |
| `r_scanline`         | 1       | Enable scanline effect (0=off, 1=on).                 |
| `r_filmgrain`        | 1       | Enable film grain effect (0=off, 1=on).               |
| `r_lensflare`        | 1       | Enable lens flare (0=off, 1=on).                       |
| `r_black_white`      | 1       | Enable black and white effect (0=off, 1=on).          |
| `r_sharpening`       | 1       | Enable sharpening (0=off, 1=on).                       |
| `r_fxaa`             | 1       | Enable depth-based anti-aliasing (0=off, 1=on).       |
| `r_skybox`           | 1       | Enable skybox rendering (0=off, 1=on).                 |
| `r_particles`        | 1       | Enable particles (0=off, 1=on).                        |
| `r_particles_cull_dist` | 75.0  | Particle culling distance.                             |
| `r_sprites`          | 1       | Enable sprites (0=off, 1=on).                          |
| `r_water`            | 1       | Enable water rendering (0=off, 1=on).                  |
| `r_lightmaps_bicubic`| 0       | Enable bicubic lightmap filtering (0=off, 1=on).      |
| `r_sun_shadow_distance` | 50.0  | Orthographic size for sun shadow map frustum.         |
| `r_texture_quality`  | 5       | Texture quality (1=very low to 5=very high).          |
| `fov_vertical`       | 55      | Vertical field of view in degrees.                     |
| `r_motionblur`       | 0       | Enable motion blur (0=off, 1=on).                      |

---

### Gameplay & UI

| Cvar          | Default | Description                                  |
|---------------|---------|----------------------------------------------|
| `noclip`      | 0       | Toggle player collision and gravity.         |
| `g_speed`     | 6.0     | Player walking speed.                         |
| `g_sprint_speed` | 8.0   | Player sprinting speed.                       |
| `g_accel`     | 15.0    | Player acceleration.                          |
| `g_friction`  | 5.0     | Player friction.                              |
| `gravity`     | 9.81    | Gravity strength.                             |
| `crosshair`   | 1       | Show crosshair (0=off, 1=on).                 |
| `show_fps`    | 0       | Show FPS counter (0=off, 1=on).               |
| `show_pos`    | 0       | Show player position (0=off, 1=on).           |
| `volume`      | 2.5     | Master volume (0.0 to 4.0).                    |
| `timescale`   | 1.0     | Game speed scale (1.0 = normal).               |
| `g_jump_force`| 350.0   | Player jump force.                             |
| `g_bob`      | 0.01    | Amount of view bobbing.                        |
| `g_bobcycle` | 0.8     | Speed of view bobbing.                         |
| `g_cheats`   | 0 or 1  | Enable cheats (0=off, 1=on).                   |
| `sensitivity` | 1.0     | Mouse sensitivity.                             |
| `p_disable_deactivation` | 0 | Disable physics object sleeping (0=off,1=on). |

---

### Debug Views

| Cvar                 | Default | Description                          |
|----------------------|---------|--------------------------------------|
| `r_debug_albedo`     | 0       | Show G-Buffer albedo buffer.          |
| `r_debug_normals`    | 0       | Show G-Buffer normals buffer.         |
| `r_debug_position`   | 0       | Show G-Buffer position buffer.        |
| `r_debug_metallic`   | 0       | Show metallic channel buffer.         |
| `r_debug_roughness`  | 0       | Show roughness channel buffer.        |
| `r_debug_ao`        | 0       | Show ambient occlusion buffer.         |
| `r_debug_velocity`   | 0       | Show motion velocity buffer.           |
| `r_debug_volumetric` | 0       | Show volumetric lighting buffer.       |
| `r_debug_bloom`      | 0       | Show bloom brightness mask.            |
| `r_debug_lightmaps`  | 0       | Show lightmap buffer.                   |
| `r_debug_lightmaps_directional` | 0 | Show directional lightmap buffer.   |
| `r_debug_vertex_light` | 0     | Show vertex lighting buffer.     |

---

