# Entities in Tectonic Engine

In the Tectonic Engine, an "entity" is any object in the world that has a specific purpose or behavior beyond being simple, static level geometry. This includes everything from models and lights to interactive triggers and complex logic controllers. They are the interactive and dynamic elements that bring a scene to life.

## Common Entity Properties

Most entities share a common set of properties that can be edited in the Inspector panel:

*   **Targetname:** A unique name assigned to an entity instance. This name is crucial for the **I/O System**, as it acts as the target for inputs and outputs.
*   **Position & Rotation:** Defines the entity's location and orientation in the world.

---

## Brush-Based Entities

Brushes are the primary building blocks for level geometry, but they can also be configured to act as specialized entity volumes. A brush becomes an entity when one of its special properties is enabled.

### Trigger
*   **Description:** An invisible volume that can detect when the player enters or leaves it. This is the cornerstone of scripting level events like opening doors, setting off traps, or starting a cinematic.
*   **How to Create:** Check the `Is Trigger` property on a brush.
*   **Outputs:**
| Output Name | Parameter | Description |
| :--- | :--- | :--- |
| `OnTouch` | None | Fires when the player first enters the volume. |
| `OnEndTouch` | None | Fires when the player leaves the volume. |
| `OnUse` | None | Fires when the player presses the 'use' key while looking at the trigger. |

### Reflection Probe, DSP Zone, Water & Glass Volumes
*   **Description:** These are specialized visual or audio volumes. They do not have inputs or outputs and are configured entirely through their properties in the Inspector.
*   **How to Create:** Check the `Is Reflection Probe`, `Is DSP Zone`, `Is Water`, or `Is Glass` property on a brush.

---

## Mesh, Point, and Emitter Entities

These are entities that are typically represented by a gizmo in the editor and have a specific function at their point of origin.

### Model
*   **Description:** An instance of a glTF model in the scene. Models are the primary visual assets for props, characters, and complex static geometry.
*   **Inputs:**
| Input Name | Parameter | Description |
| :--- | :--- | :--- |
| `EnablePhysics` | None | Activates physics simulation for this model. |
| `DisablePhysics`| None | Deactivates physics simulation for this model. |

### Light
*   **Description:** An entity that emits light into the scene.
*   **Types:** Point and Spot.
*   **Inputs:**
| Input Name | Parameter | Description |
| :--- | :--- | :--- |
| `TurnOn` | None | Turns the light on. |
| `TurnOff` | None | Turns the light off. |
| `Toggle` | None | Toggles the light's current state (on/off). |

### Sound Entity
*   **Description:** An entity that emits a `.wav` or `.mp3` sound from a specific point in 3D space.
*   **Inputs:**
| Input Name | Parameter | Description |
| :--- | :--- | :--- |
| `PlaySound` | None | Plays the sound from the beginning. |
| `StopSound` | None | Stops the sound if it is currently playing. |
| `EnableLoop` | None | Sets the sound to loop. |
| `DisableLoop` | None | Stops the sound from looping. |
| `ToggleLoop` | None | Toggles the looping state of the sound. |

### Particle Emitter
*   **Description:** Spawns particles (fire, smoke, etc.) based on a `.par` definition file.
*   **Inputs:**
| Input Name | Parameter | Description |
| :--- | :--- | :--- |
| `TurnOn` | None | Starts spawning particles. |
| `TurnOff` | None | Stops spawning new particles. |
| `Toggle` | None | Toggles the emitter's state (on/off). |

### Sprite
*   **Description:** A billboard entity that displays a 2D image always facing the camera.
*   **Inputs:**
| Input Name | Parameter | Description |
| :--- | :--- | :--- |
| `TurnOn` | None | Makes the sprite visible. |
| `TurnOff` | None | Makes the sprite invisible. |
| `Toggle` | None | Toggles the sprite's visibility on or off. |

### Video Player
*   **Description:** A plane in the world that can play video files.
*   **Inputs:**
| Input Name | Parameter | Description |
| :--- | :--- | :--- |
| `startvideo` | None | Starts playback from the beginning. |
| `stopvideo` | None | Stops playback and resets to the first frame. |
| `restartvideo` | None | Stops and then immediately starts playback. |

### Decal, Parallax Room, Player Start
*   **Description:** These are primarily visual or helper entities and do not have any I/O inputs.

---

## Logic Entities

Logic entities are the foundation of the engine's scripting system. They are invisible in-game and are used to create complex sequences of events.

### `logic_timer`
*   **Description:** Triggers an output after a specified delay.
*   **Properties:**
| Key | Default | Description |
| :--- | :--- | :--- |
| `delay` | `1.0` | The time in seconds to wait before firing the `OnTimer` output. |
*   **Inputs:**
| Input Name | Parameter | Description |
| :--- | :--- | :--- |
| `StartTimer` | None | Starts the timer. Resets if already running. |
| `StopTimer` | None | Stops and resets the timer. |
| `ToggleTimer` | None | Toggles the timer's state (on/off). |
*   **Outputs:**
| Output Name | Parameter | Description |
| :--- | :--- | :--- |
| `OnTimer` | None | Fires when the timer reaches zero. |

### `math_counter`
*   **Description:** Stores a numerical value and performs math operations on it. Fires outputs when the value reaches a minimum or maximum threshold.
*   **Properties:**
| Key | Default | Description |
| :--- | :--- | :--- |
| `min` | `0` | The minimum value to check against for the `OnHitMin` output. |
| `max` | `10` | The maximum value to check against for the `OnHitMax` output. |
*   **Inputs:**
| Input Name | Parameter | Description |
| :--- | :--- | :--- |
| `Add` | `number` | Adds the parameter value to the counter. Defaults to `1`. |
| `Subtract` | `number` | Subtracts the parameter value from the counter. Defaults to `1`. |
| `Multiply` | `number` | Multiplies the counter by the parameter value. Defaults to `1`. |
| `Divide` | `number` | Divides the counter by the parameter value. Defaults to `1`. |
*   **Outputs:**
| Output Name | Parameter | Description |
| :--- | :--- | :--- |
| `OnHitMax` | None | Fires when the counter's value is greater than or equal to its `max` property. |
| `OnHitMin` | None | Fires when the counter's value is less than or equal to its `min` property. |

### `logic_random`
*   **Description:** When enabled, fires an output at random time intervals.
*   **Properties:**
| Key | Default | Description |
| :--- | :--- | :--- |
| `min_time` | `1.0` | The minimum time in seconds before the next trigger. |
| `max_time` | `5.0` | The maximum time in seconds before the next trigger. |
*   **Inputs:**
| Input Name | Parameter | Description |
| :--- | :--- | :--- |
| `Enable` | None | Starts the random timer. |
| `Disable` | None | Stops the random timer. |
*   **Outputs:**
| Output Name | Parameter | Description |
| :--- | :--- | :--- |
| `OnRandom` | None | Fires after a random amount of time has passed (between `min_time` and `max_time`). |

### `logic_relay`
*   **Description:** A simple pass-through entity that can be enabled or disabled. When triggered, it fires its outputs. This is useful for grouping multiple outputs, or for creating a "kill switch" for events.
*   **Properties:** None. Its state (`enabled`/`disabled`) is controlled via inputs.
*   **Inputs:**
| Input Name | Parameter | Description |
| :--- | :--- | :--- |
| `Trigger` | None | Fires the `OnTrigger` output if the relay is enabled. |
| `Enable` | None | Enables the relay, allowing it to fire outputs when `Trigger`ed. |
| `Disable` | None | Disables the relay, preventing it from firing outputs. |
| `Toggle` | None | Toggles the enabled/disabled state of the relay. |
*   **Outputs:**
| Output Name | Parameter | Description |
| :--- | :--- | :--- |
| `OnTrigger` | None | Fires when the relay receives the `Trigger` input and is enabled. |

### `point_servercommand`
*   **Description:** Executes a server console command or cvar when it receives the `Command` input. This is a powerful debugging and scripting tool.
*   **Properties:** None. The command to execute is provided by the *parameter* of the incoming input.
*   **Inputs:**
| Input Name | Parameter | Description |
| :--- | :--- | :--- |
| `Command` | `string` | Executes the string provided in the parameter as a console command. E.g., `noclip 1`, `map mymap`. |
*   **Outputs:** None.

### `logic_compare`
*   **Description:** Compares two internal floating-point values and fires an output based on the result of the comparison (`<`, `=`, `!=`, `>`).
*   **Properties:**
| Key | Default | Description |
| :--- | :--- | :--- |
| `InitialValue` | `0` | The starting value for the internal input value. |
| `CompareValue` | `0` | The value to compare against. |
*   **Inputs:**
| Input Name | Parameter | Description |
| :--- | :--- | :--- |
| `SetValue` | `float` | Sets the internal input value to the parameter and does not perform a comparison. |
| `SetValueCompare` | `float` | Sets the internal input value to the parameter and immediately performs a comparison. |
| `SetCompareValue` | `float` | Sets the internal compare value (property `CompareValue`) to the parameter. |
| `Compare` | None | Forces a comparison of the current input value with the current compare value. |
*   **Outputs:**
| Output Name | Parameter | Description |
| :--- | :--- | :--- |
| `OnLessThan` | `float` | Fired when the input value is less than the compare value. Sends the input value as data. |
| `OnEqualTo` | `float` | Fired when the input value is equal to the compare value. Sends the input value as data. |
| `OnNotEqualTo` | `float` | Fired when the input value is not equal to the compare value. Sends the input value as data. |
| `OnGreaterThan` | `float` | Fired when the input value is greater than the compare value. Sends the input value as data. |