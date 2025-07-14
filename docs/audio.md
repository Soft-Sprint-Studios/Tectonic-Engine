# Audio System

The Tectonic Engine features a comprehensive 3D audio system built on the OpenAL backend, designed for creating immersive soundscapes.

## Sound Sources

The primary way to add sound to a scene is through a **Sound Entity**. This can be added from the editor's Hierarchy panel.

Key properties of a Sound Entity:
*   **Sound Path:** The path to the audio file, relative to the `sounds/` directory.
*   **Supported Formats:** `.wav` and `.mp3` files are supported.
*   **Positional Audio:** The sound is spatialized in 3D space based on the entity's position.
*   **Properties:** Volume, pitch, and max distance can all be configured in the editor.
*   **Looping:** Sounds can be set to loop continuously.

## Real-time DSP Reverb

The engine features a multi-threaded Digital Signal Processing (DSP) system capable of generating high-quality environmental reverb without impacting the main game thread.

### DSP Zones

To use the reverb system, you create a special type of brush in the editor called a **DSP Zone**.

1.  Create a standard brush that encompasses the area where you want the reverb to be active (e.g., a cave, a long hallway).
2.  In the Inspector, check the `Is DSP Zone` property.
3.  Select a `Reverb Preset` from the dropdown menu. Presets include:
    *   Small/Medium/Large Room
    *   Hall
    *   Cave

Any sound source (including the player) that enters the bounds of this DSP Zone will automatically have the selected reverb effect applied to it. The engine smoothly handles transitions between different zones.