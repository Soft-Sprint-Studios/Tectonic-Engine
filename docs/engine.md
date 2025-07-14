# Engine Architecture

The Tectonic Engine is built around a set of core systems that manage everything from the main loop to developer-facing tools like the console and cvars.

## Main Loop

The engine operates on a standard game loop located in `main.c`, which follows this sequence on every frame:
1.  **Process Input:** Handles all user input from the mouse, keyboard, and other devices.
2.  **Update State:** Ticks the physics simulation, updates game logic, animations, and other time-dependent systems.
3.  **Render:** Executes all rendering passes to draw the scene to the screen.

The engine can run in several modes, including `MODE_GAME`, `MODE_EDITOR`, and `MODE_MAINMENU`.

## Console Variable System (Cvars)

The engine features a powerful console variable (Cvar) system that allows developers and users to tweak engine settings in real-time.

*   **Definition:** Cvars are defined in code using `Cvar_Register()`.
*   **Configuration:** At startup, the engine loads cvars from `cvars.txt`. Any changes made in the console are saved back to this file on exit.
*   **Usage:** Cvars can be modified from the in-game console. For example: `r_bloom 1`

## Command System

The engine includes a console command system for executing actions.

*   **Registration:** New commands are registered using `Commands_Register()` in `commands.c`.
*   **Execution:** Commands can be typed directly into the console. The system also processes key bindings from `binds.txt`.
*   **Example Commands:** `map <mapname>`, `noclip`, `edit`, `quit`.

## Key Binds

Key bindings are managed through the Binds system.

*   **Configuration:** Bindings are stored in `binds.txt`. Each line defines a key and the command it executes.
*   **Format:** `bind key_name command_to_execute`
*   **Example:** `bind v noclip`
*   **In-game Commands:** Use `bind` and `unbind` in the console to manage key bindings at runtime.

## Anti-Tamper Checksum

For release builds, the engine employs a checksum verification system to ensure the integrity of the executable.

1.  **Embedding:** After a successful build, a C++ utility named `Patcher` (`checksumpatcher.cpp`) is run.
2.  **Calculation:** The `Patcher` calculates a CRC32 checksum of the entire engine executable, with the checksum field itself temporarily zeroed out.
3.  **Injection:** The calculated checksum is then written into a predefined location (`.checksum_section`) inside the executable binary.
4.  **Verification:** On startup, the engine (`checksum.c`) performs the same CRC32 calculation on itself and compares the result to the embedded checksum. If they do not match, the application will exit, preventing it from running if tampered with.