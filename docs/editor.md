# Tectonic Editor: A Practical Guide

The Tectonic Editor is a powerful, real-time toolset integrated directly into the engine. This guide provides a practical walkthrough of its core features and workflows for level design and scene creation.

## Getting Started

*   **Launch the Editor:** Press `F5` or type `edit` in the console.
*   **Camera Controls:**
    *   **Movement:** Use `W`, `A`, `S`, `D` to move. `Q` and `E` move up and down. Hold `Shift` to move faster.
    *   **Looking:** Hold the **Right Mouse Button** in the perspective viewport to look around.
    *   **Z-Mode:** Press `Z` to lock the mouse for FPS-style "fly" navigation. Press `Z` again to exit.
    *   **Focus:** Select an object in the Hierarchy and press `F` to focus the camera on it.
*   **Grid Controls:**
    *   **Snap:** Toggled with the "Snapping" button in the Inspector. When on, objects and vertices will snap to the grid.
    *   **Size:** Adjust the grid size with the `[` and `]` keys or the "Grid Size" slider.

---

## 1. Creating Level Geometry with Brushes

Brushes are the foundation of level geometry in Tectonic. They are convex shapes that can be combined to build complex environments.

### Creating a Brush

1.  **Select a Shape:** In the Inspector panel, under "Creation Tools," select a primitive shape (Block, Cylinder, Wedge, or Spike).
2.  **Draw in a 2D View:** Click and drag in a 2D viewport (Top, Front, or Side) to define the initial footprint of the brush. A yellow preview will appear.
3.  **Define the Third Dimension:**
    *   **With Mouse:** Move to another 2D viewport and click and drag one of the yellow handles to set the brush's height/depth.
    *   **With Gizmo:** Once the preview brush exists, you can use the Move/Scale gizmo to adjust it.
4.  **Confirm Creation:** Press **Enter** to finalize the brush and add it to the scene. Press **Escape** to cancel.

### Manipulating Brushes

*   **Transform:** Select a brush and use the **Move (1)**, **Rotate (2)**, or **Scale (3)** gizmos to position it.
*   **Resize:** Use the handles that appear on a selected brush in the 2D viewports to resize it along an axis.
*   **Texture Lock:** (`Tools -> Texture Lock`) When enabled, moving a brush will slide the geometry under the texture, preventing texture swimming and stretching.

---

## 2. Advanced Brush & Face Manipulation

For more complex shapes, you can directly edit a brush's geometry.

### Face Edit Sheet

1.  **Select a Face:** In the perspective view, click on a brush to select it. The face you clicked will be highlighted. An orange "Face Edit Sheet" window will appear.
2.  **Apply Materials:** Use the material buttons in the sheet to open the Texture Browser and assign materials to the base layer or any of the three blend layers (controlled by vertex colors).
3.  **Adjust UVs:** Use the `Shift`, `Scale`, and `Rotation` controls to adjust texture mapping on the selected face. The `Fit` button automatically scales the texture to fit the face dimensions.
4.  **Flip Face Normal:** If a face appears inside-out, click this button to reverse its vertex winding order, which flips the direction the face is pointing.
5.  **Delete Face:** Permanently removes the selected face from the brush. This is useful for creating concave shapes or hollowing out brushes, but use with caution as it cannot be undone without creating new faces.
6.  **Subdivide:** Increase the `Subdivisions U` and `V` values and click "Subdivide Selected Face" to add more vertices to a face, preparing it for more detailed sculpting.

### Vertex Tools

This mode allows you to treat a brush like digital clay.
1.  Select a brush.
2.  Press **`9`** to enter **Sculpt Mode** or **`0`** to enter **Paint Mode**. A "Vertex Tools" window will appear.
3.  **Sculpt:** Hold the **Left Mouse Button** in a viewport to raise vertices. Hold **Shift** while clicking to lower them. Adjust the brush `Radius` and `Strength` in the tools window.
4.  **Paint:** Paint vertex colors (R, G, B) to blend up to four different materials on a single brush face.

### Clipping Tool

The clipping tool is essential for creating complex angles and custom shapes.
1.  Select a brush you want to cut.
2.  Press **`C`** to activate the clip tool.
3.  **Define the Clip Plane:** In a 2D viewport, click two points to define a cutting line.
4.  **Choose the Side to Keep:** Click on one side of the line. A yellow indicator will show the normal of the cut plane, pointing towards the half that will be kept.
5.  **Confirm the Cut:** Press **`C`** again. The engine will slice the brush and cap the new face, creating two separate brushes.

---

## 3. Working with Entities and Logic

Entities like models, lights, and logic controllers are placed from the **Hierarchy** panel and configured in the **Inspector**.

### Placing Models & Point Entities

1.  In the Hierarchy panel, find the appropriate category (e.g., "Models", "Lights").
2.  Click the "Add" button for that category (e.g., **"Add Model"**).
3.  For models, the Model Browser will appear. Select a model and click **"Add to Scene"**.
4.  For other entities like lights, a new entity with default properties will be created at the editor camera's position.
5.  Use the transform gizmos to position the new entity.

### Creating Logic Entities

Logic entities are the core of the engine's scripting system.

1.  In the Hierarchy panel, expand the **"Logic Entities"** section.
2.  Click **"Add Logic Entity"**. A new `logic_timer` entity will be created by default.
3.  With the new entity selected, go to the Inspector.
4.  Use the **Classname** dropdown to change its type (e.g., to `math_counter` or `logic_random`).
5.  Assign it a unique **Targetname** so other entities can reference it.
6.  Configure its specific properties (e.g., set the `delay` for a `logic_timer`).

### Connecting Entities with the I/O System

The I/O system makes entities interact. This example will make a `logic_timer` turn on a light after 5 seconds.

1.  **Create the Entities:**
    *   Create a **Light** and give it the Targetname `my_light`. Turn it off by default.
    *   Create a **`logic_timer`** and give it the Targetname `my_timer`. In its properties, set its `delay` to `5.0`.
    *   Create a **Trigger** brush (as described in the Brush Entities section).
2.  **Create the First Connection (Trigger -> Timer):**
    *   Select the Trigger brush.
    *   In the Inspector, under the **Outputs** section for the `OnTouch` output, click **"Add Connection"**.
    *   In the new connection fields, set:
        *   **Target Name:** `my_timer`
        *   **Input Name:** `StartTimer`
3.  **Create the Second Connection (Timer -> Light):**
    *   Select the `logic_timer` entity.
    *   In the Inspector, under the **Outputs** section for the `OnTimer` output, click **"Add Connection"**.
    *   Set the new connection's fields:
        *   **Target Name:** `my_light`
        *   **Input Name:** `TurnOn`

Now, when the player touches the trigger, it will start the timer. After 5 seconds, the timer will fire its output, which in turn tells the light to turn on.