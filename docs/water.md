# Water System

The Tectonic Engine features a dynamic water system that provides visually appealing and physically interactive water surfaces. This system is integrated with both the renderer and the physics engine.

## Creating Water

Creating a water volume is straightforward using the editor's brush tools:

1.  **Create a Brush:** Create a brush entity that defines the volume of your body of water. The top surface of the brush will become the water surface.
2.  **Enable the Water Property:** With the brush selected, go to the Inspector panel and check the **`Is Water`** property.
3.  **Assign a Water Definition:** In the Inspector, a new dropdown will appear allowing you to select a **Water Definition**. This definition controls the visual appearance of the water.

## Water Definitions (`waters.def`)

The appearance of water is controlled by definitions in the `waters.def` file. This allows you to create and manage different types of water for your project.

### Example Definition

```
"ocean"
{
    normal = "water/ocean_n.png"
    dudv = "water/dudv_water.png"
    flowmap = "water/ocean_flow.png"
    flowspeed = 0.05
}
```

### Properties

*   `normal`: The normal map used to create the appearance of waves and ripples on the water surface.
*   `dudv`: A DUDV (Distortion/Displacement) map used to create the refractive distortion effect. It's typically a grayscale texture that perturbs the texture coordinates used for rendering what's under the water.
*   `flowmap`: An optional RGB texture used to control the direction and speed of water flow across the surface. The Red and Green channels are typically used to encode the U and V flow directions.
*   `flowspeed`: A multiplier that controls how fast the flow map influences the water animation.

## Visuals and Rendering

The water shader combines several effects to create a realistic look:

*   **Reflections:** The water surface reflects the skybox and level geometry. For accurate local reflections, place a **Reflection Probe** above the water surface.
*   **Refraction:** The water distorts the view of objects beneath its surface, an effect controlled by the `dudv` map.
*   **Animated Normals:** The surface normals are animated using a combination of the normal map, DUDV map, and flow map to simulate moving waves.

## Physics Interaction

Water volumes are fully integrated with the physics system.

*   **Buoyancy:** Any dynamic rigid body (an object with `mass` > 0) that enters a water volume will have an upward buoyancy force applied to it, causing it to float.
*   **Drag:** Objects moving through the water will experience drag, slowing down both their linear and angular velocity for a more realistic water resistance effect.