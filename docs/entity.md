# Entities in Tectonic Engine

In the Tectonic Engine, an "entity" is any object in the world that has a specific purpose or behavior beyond being simple, static level geometry. This includes everything from models and lights to interactive triggers and complex logic controllers. They are the interactive and dynamic elements that bring a scene to life, and they are primarily controlled via the Input/Output (I/O) system.

## Common Entity Properties

Most entities share a common set of properties that can be edited in the Inspector panel:

*   **Targetname:** A unique name assigned to an entity instance. This name is crucial for the **I/O System**, as it acts as the target for inputs. For example, an output from a `func_button` can target a `light` entity by its `targetname`.
*   **Position & Rotation:** Defines the entity's location and orientation in the world.

---

## Brush-Based Entities

Brushes are the primary building blocks for level geometry. By assigning a special `classname` to a brush, it becomes a brush entity, gaining unique properties, behaviors, and the ability to participate in the I/O system.

### `func_button`
*   **Description:** A brush that acts as a pressable button. When the player looks at it and presses the 'use' key, it fires an `OnPressed` output. It can also be locked to prevent use.
*   **Properties:**
| Key | Default | Description |
| :--- | :--- | :--- |
| `locked` | `0` | If set to `1`, the button is locked. Attempting to use it will fire `OnUseLocked` instead of `OnPressed`. |
*   **Inputs:**
| Input Name | Parameter | Description |
| :--- | :--- | :--- |
| `Lock` | None | Locks the button, preventing it from being used by the player. |
| `Unlock` | None | Unlocks the button, allowing it to be used by the player. |
| `Press` | None | Simulates the button being pressed, firing the `OnPressed` output. |
*   **Outputs:**
| Output Name | Parameter | Description |
| :--- | :--- | :--- |
| `OnPressed` | None | Fires when an unlocked button is used by the player or triggered by the `Press` input. |
| `OnUseLocked`| None | Fires when the player attempts to use the button while it is locked. |

### Triggers (`trigger_multiple`, `trigger_once`, `trigger_autosave`, `trigger_gravity`, `trigger_dspzone`)
*   **Description:** Triggers are invisible volumes that detect the player's presence and fire outputs. While each trigger type has a specific purpose, they all share a common set of inputs and outputs for activation and scripting.
    *   `trigger_multiple`: The standard trigger. It fires its outputs every time the player enters or leaves its volume.
    *   `trigger_once`: As the name implies, this trigger is intended to fire its outputs only the first time the player enters it, and then it deactivates itself.
    *   `trigger_autosave`: Its primary purpose is to signal the game to perform a quick save when the player touches it. It can still fire outputs for other logic.
    *   `trigger_gravity`: The main function of this volume is to change the player's gravity while they are inside it. The new gravity value is set in its properties.
    *   `trigger_dspzone`: This volume applies a Digital Signal Processing (DSP) audio effect (like reverb or echo) to all sounds heard by the player while they are inside it.
*   **Shared Inputs:**
| Input Name | Parameter | Description |
| :--- | :--- | :--- |
| `Enable` | None | Activates the trigger, allowing it to detect the player and fire outputs. |
| `Disable`| None | Deactivates the trigger. It will ignore the player. |
| `Toggle` | None | Toggles the trigger's enabled/disabled state. |
*   **Shared Outputs:**
| Output Name | Parameter | Description |
| :--- | :--- | :--- |
| `OnStartTouch` | None | Fires when the player first enters the volume. |
| `OnEndTouch` | None | Fires when the player leaves the volume. |

### `func_clip`
*   **Description:** An invisible, solid brush that blocks players and other physics objects. It is commonly used to smooth out complex geometry, prevent players from getting stuck, or restrict access to certain areas without adding visible geometry. It has no I/O connections.

### `func_conveyor`
*   **Description:** A brush that pushes physics objects (including the player) along a specific direction and at a set speed. The direction and speed are defined in its properties. It has no I/O connections.

### `func_friction`
*   **Description:** A volume that alters the friction of any entity that touches it. It can be used to create slippery surfaces (like ice) or high-friction surfaces (like mud). The friction modifier is set in its properties. It has no I/O connections.

### `func_illusionary`
*   **Description:** A brush that appears solid and is rendered normally, but has no collision. Players and objects can pass right through it. It is the opposite of `func_clip`. It has no I/O connections.

### `func_ladder`
*   **Description:** A volume that defines a climbable surface. When the player is inside this volume and facing the "forward" direction of the ladder, they can move up and down. It has no I/O connections.

### `func_lod`
*   **Description:** A brush that acts as a Level of Detail (LOD) proxy. This brush will only be rendered when the camera is far away, replacing more complex geometry to improve performance. It is configured via its properties. It has no I/O connections.

### `func_water`
*   **Description:** A volume that simulates water. It affects player physics (buoyancy, muffled movement), applies a visual fog/tint, and can play splashing sounds. It is configured via its properties. It has no I/O connections.

### `env_glass`
*   **Description:** A brush entity used to create transparent but solid glass surfaces. It has no I/O connections.

### `env_reflectionprobe`
*   **Description:** A special volume that captures a 360-degree image of its surroundings. This image is then used to create realistic reflections on metallic or shiny surfaces within its area of influence. It has no I/O connections.

---

## Point, Mesh, and Emitter Entities

These entities are placed at a single point in the world and are typically represented by a gizmo in the editor.

### Model
*   **Description:** An instance of a 3D model (`.gltf`) in the scene. This is used for props, characters, and any complex visual element.
*   **Inputs:**
| Input Name | Parameter | Description |
| :--- | :--- | :--- |
| `EnablePhysics` | None | Activates the model's physics, allowing it to collide and react to forces. |
| `DisablePhysics`| None | Deactivates the model's physics, freezing it in place. |
| `PlayAnimation`| `string` | Plays an animation by name. If the animation name includes "noloop", it will play only once; otherwise, it will loop. |

### Light
*   **Description:** A point or spot light that illuminates the scene.
*   **Inputs:**
| Input Name | Parameter | Description |
| :--- | :--- | :--- |
| `TurnOn` | None | Turns the light on. |
| `TurnOff`| None | Turns the light off. |
| `Toggle` | None | Toggles the light's current state (on/off). |

### Sound Entity
*   **Description:** An entity that emits a sound from a specific point in 3D space.
*   **Inputs:**
| Input Name | Parameter | Description |
| :--- | :--- | :--- |
| `PlaySound` | None | Plays the assigned sound from the beginning. |
| `StopSound` | None | Stops the sound if it is currently playing. |
| `EnableLoop`| None | Sets the sound to loop continuously when played. |
| `DisableLoop`| None | Prevents the sound from looping. |
| `ToggleLoop`| None | Toggles the looping state of the sound. |

### Particle Emitter
*   **Description:** An entity that spawns a stream of particles (e.g., fire, smoke, sparks) based on a particle definition file (`.par`).
*   **Inputs:**
| Input Name | Parameter | Description |
| :--- | :--- | :--- |
| `TurnOn` | None | Starts the particle emission. |
| `TurnOff`| None | Stops spawning new particles. Existing particles will fade out naturally. |
| `Toggle` | None | Toggles the emitter's state (on/off). |

### Sprite
*   **Description:** A 2D image that is rendered in the 3D world and always faces the camera (a "billboard").
*   **Inputs:**
| Input Name | Parameter | Description |
| :--- | :--- | :--- |
| `TurnOn` | None | Makes the sprite visible. |
| `TurnOff`| None | Makes the sprite invisible. |
| `Toggle` | None | Toggles the sprite's visibility. |

### Video Player
*   **Description:** A simple plane in the world that can play video files as a texture.
*   **Inputs:**
| Input Name | Parameter | Description |
| :--- | :--- | :--- |
| `startvideo` | None | Starts video playback from the beginning. |
| `stopvideo` | None | Stops playback and resets the video to its first frame. |
| `restartvideo`| None | Stops playback and immediately starts it again from the beginning. |

---

## Environmental Entities

These entities affect the scene's rendering or the player's state on a global or large-scale basis. They are typically invisible.

### `env_blackhole`
*   **Description:** A visual effect entity. When enabled, it displays a rotating visual distortion effect.
*   **Properties:**
| Key | Default | Description |
| :--- | :--- | :--- |
| `rotationspeed`| `10.0` | The speed at which the effect rotates, in degrees per second. |
*   **Inputs:**
| Input Name | Parameter | Description |
| :--- | :--- | :--- |
| `Enable` | None | Activates the entity, making it visible and starting its rotation. |
| `Disable`| None | Deactivates the entity, hiding it. |

### `env_fade`
*   **Description:** Manages full-screen color fades, essential for cinematic sequences, level transitions, or effects like blinking.
*   **Properties:**
| Key | Default | Description |
| :--- | :--- | :--- |
| `duration` | `2.0` | The time in seconds the fade (in or out) takes to complete. |
| `holdtime` | `1.0` | When using the `Fade` input, this is the time in seconds to hold the screen at full black before fading back in. |
| `renderamt`| `255` | The final opacity of the fade, from 0 (transparent) to 255 (fully opaque). |
*   **Inputs:**
| Input Name | Parameter | Description |
| :--- | :--- | :--- |
| `FadeIn` | None | Fades the screen in (from black to clear). |
| `FadeOut`| None | Fades the screen out (from clear to black). |
| `Fade` | None | A combination input that fades out, holds for `holdtime`, and then fades back in. |

### `env_fog`
*   **Description:** A point entity that controls the scene's global fog settings (density, color, start/end distance). Only one `env_fog` can be active at a time.
*   **Inputs:**
| Input Name | Parameter | Description |
| :--- | :--- | :--- |
| `Enable` | None | Activates this entity's fog settings, overriding any other active fog. |
| `Disable`| None | Deactivates this entity's fog settings. |

### `env_shake`
*   **Description:** Creates a camera shake effect, simulating an earthquake, explosion, or large impact. The shake can be global or limited to a radius.
*   **Properties:**
| Key | Default | Description |
| :--- | :--- | :--- |
| `GlobalShake`| `0` | If `1`, the shake affects the player anywhere on the map. If `0`, it only affects players within the `radius`. |
| `radius` | `500.0`| The radius of the effect if `GlobalShake` is `0`. |
| `amplitude` | `4.0` | The intensity or magnitude of the shake. Higher values are more violent. |
| `frequency` | `40.0` | The speed or frequency of the shake. Higher values are faster and more jittery. |
| `duration` | `1.0` | The time in seconds the shake effect lasts. |
*   **Inputs:**
| Input Name | Parameter | Description |
| :--- | :--- | :--- |
| `StartShake` | None | Begins the camera shake with the entity's defined properties. |
| `StopShake` | None | Immediately stops any active camera shake caused by this entity. |

---

## Logic Entities

Logic entities are the core of the engine's scripting system. They are invisible in-game and exist solely to process I/O events, control timing, and manage game state.

### `game_end`
*   **Description:** An entity used to end the current gameplay session.
*   **Inputs:**
| Input Name | Parameter | Description |
| :--- | :--- | :--- |
| `EndGame` | None | Disconnects the player from the server and returns them to the main menu. |

### `logic_auto`
*   **Description:** A special entity that automatically fires an output when the map is loaded. This is the primary starting point for any map-initialization logic (e.g., starting ambient sounds, setting up initial entity states). There can be only one in a level.
*   **Outputs:**
| Output Name | Parameter | Description |
| :--- | :--- | :--- |
| `OnMapSpawn` | None | Fires once, as soon as the map has finished loading and the game begins. |

### `logic_compare`
*   **Description:** Compares an internal value against a set "compare" value and fires an output based on the result (`<`, `==`, `!=`, `>`). This is useful for creating conditional logic.
*   **Properties:**
| Key | Default | Description |
| :--- | :--- | :--- |
| `InitialValue` | `0` | The starting value for the internal input value. |
| `CompareValue` | `0` | The value to compare against. Can be changed at runtime via the `SetCompareValue` input. |
*   **Inputs:**
| Input Name | Parameter | Description |
| :--- | :--- | :--- |
| `SetValue` | `float` | Sets the internal value to the parameter, but does *not* perform a comparison. |
| `SetValueCompare`| `float` | Sets the internal value to the parameter and immediately performs a comparison. |
| `SetCompareValue`| `float` | Sets the `CompareValue` property to the parameter. |
| `Compare` | None | Forces a comparison using the current internal value and `CompareValue`. |
*   **Outputs:**
| Output Name | Parameter | Description |
| :--- | :--- | :--- |
| `OnLessThan` | `float` | Fired when `InputValue < CompareValue`. The current `InputValue` is sent as the parameter. |
| `OnEqualTo` | `float` | Fired when `InputValue == CompareValue`. The current `InputValue` is sent as the parameter. |
| `OnNotEqualTo`| `float` | Fired when `InputValue != CompareValue`. The current `InputValue` is sent as the parameter. |
| `OnGreaterThan`| `float` | Fired when `InputValue > CompareValue`. The current `InputValue` is sent as the parameter. |

### `logic_random`
*   **Description:** When enabled, this entity fires an output at random time intervals, chosen from a defined range.
*   **Properties:**
| Key | Default | Description |
| :--- | :--- | :--- |
| `min_time` | `1.0` | The minimum time in seconds to wait before the next random trigger. |
| `max_time` | `5.0` | The maximum time in seconds to wait before the next random trigger. |
*   **Inputs:**
| Input Name | Parameter | Description |
| :--- | :--- | :--- |
| `Enable` | None | Starts the random timer. |
| `Disable`| None | Stops the random timer. |
*   **Outputs:**
| Output Name | Parameter | Description |
| :--- | :--- | :--- |
| `OnRandom` | None | Fires after a random amount of time (between `min_time` and `max_time`) has passed. |

### `logic_relay`
*   **Description:** A simple but essential I/O utility. It receives a `Trigger` input and passes it along to its `OnTrigger` output. It can be enabled or disabled, acting as a gate or "kill switch" for a chain of events. It is also useful for activating multiple outputs from a single input event.
*   **Inputs:**
| Input Name | Parameter | Description |
| :--- | :--- | :--- |
| `Trigger` | None | If the relay is enabled, this input causes it to fire its `OnTrigger` output. |
| `Enable` | None | Enables the relay, allowing it to pass `Trigger` events. |
| `Disable`| None | Disables the relay, blocking all `Trigger` events. |
| `Toggle` | None | Toggles the enabled/disabled state of the relay. |
*   **Outputs:**
| Output Name | Parameter | Description |
| :--- | :--- | :--- |
| `OnTrigger`| None | Fires when the relay receives the `Trigger` input while it is enabled. |

### `logic_timer`
*   **Description:** A versatile timer that fires an output after a specified delay. It can be configured to fire only once or to repeat a set number of times (or infinitely).
*   **Properties:**
| Key | Default | Description |
| :--- | :--- | :--- |
| `delay` | `1.0` | The time in seconds to wait before firing the `OnTimer` output. |
| `repeat`| `1` | Number of times to fire. Set to `1` for a single, non-repeating fire. Set to `-1` for infinite repeats. |
*   **Inputs:**
| Input Name | Parameter | Description |
| :--- | :--- | :--- |
| `StartTimer` | None | Starts the timer. If the timer is already running, it resets it. |
| `StopTimer` | None | Stops the timer and resets its countdown. |
| `ToggleTimer`| None | Toggles the timer between its running and stopped states. |
*   **Outputs:**
| Output Name | Parameter | Description |
| :--- | :--- | :--- |
| `OnTimer` | None | Fires each time the timer's delay is reached. |

### `math_counter`
*   **Description:** Stores a numerical value and can perform basic math operations on it. It can fire outputs when its value reaches a defined minimum or maximum. Useful for tracking player actions, kill counts, or puzzle steps.
*   **Properties:**
| Key | Default | Description |
| :--- | :--- | :--- |
| `min` | `0` | The minimum value to check against for the `OnHitMin` output. |
| `max` | `10`| The maximum value to check against for the `OnHitMax` output. |
*   **Inputs:**
| Input Name | Parameter | Description |
| :--- | :--- | :--- |
| `Add` | `number`| Adds the parameter value to the counter's current value. Defaults to `1` if no parameter is given. |
| `Subtract` | `number`| Subtracts the parameter value from the counter. Defaults to `1`. |
| `Multiply` | `number`| Multiplies the counter's value by the parameter. Defaults to `1`. |
| `Divide` | `number`| Divides the counter's value by the parameter. Defaults to `1`. |
*   **Outputs:**
| Output Name | Parameter | Description |
| :--- | :--- | :--- |
| `OnHitMax` | None | Fires when the counter's value becomes greater than or equal to its `max` property. |
| `OnHitMin` | None | Fires when the counter's value becomes less than or equal to its `min` property. |

### `point_servercommand`
*   **Description:** A powerful debugging and scripting entity that can execute any console command.
*   **Inputs:**
| Input Name | Parameter | Description |
| :--- | :--- | :--- |
| `Command` | `string` | Executes the string provided in the input's parameter as a console command (e.g., `noclip 1`, `map my_next_map`). |