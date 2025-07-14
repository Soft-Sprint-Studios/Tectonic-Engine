# Material System

The Tectonic Engine uses a text-based definition file, `materials.def`, to describe all physical materials available in the engine. This system allows for easy creation and modification of materials without recompiling the engine.

## Material Definition File (`materials.def`)

Materials are defined in blocks. Each block starts with the material name in quotes, followed by properties enclosed in curly braces `{}`.

### Example Definition

```
"brick/brickwall01"
{
diffuse = "brick/brickwall01_d.png"
normal = "brick/brickwall01_n.png"
arm = "brick/brickwall01_arm.png"
height = "brick/brickwall01_h.png"
heightScale = 0.04
}
```

## Texture Maps

All texture paths are relative to the `textures/` directory.

*   `diffuse`: The base color (albedo) of the material. This is an sRGB texture.
*   `normal`: The normal map. This is a linear (non-sRGB) texture.
*   `arm`: A packed texture containing Ambient Occlusion, Roughness, and Metallic values in its R, G, and B channels, respectively. This is a linear texture.
*   `height`: A grayscale displacement map used for parallax effects. This is a linear texture.
*   `detail`: A secondary diffuse texture that can be tiled at a different scale for fine detail.

## Material Properties

*   `heightScale`: Controls the intensity of the parallax mapping effect. A value of `0.0` disables it.
*   `detailscale`: Sets the tiling frequency for the `detail` texture.
*   `roughness`: Overrides the roughness value from the `arm` texture. A value of `-1.0` (or omitting the property) means the texture's value is used.
*   `metalness`: Overrides the metallic value from the `arm` texture. A value of `-1.0` (or omitting the property) means the texture's value is used.

## Texture Blending on Brushes

A single brush face can blend up to four different materials. This is controlled by the vertex colors of the brush.

*   **Base Layer:** The primary material assigned to the face. Its influence is determined by `1.0 - (R + G + B)`.
*   **Red Channel:** The second material is blended in based on the **red** vertex color value.
*   **Green Channel:** The third material is blended in based on the **green** vertex color value.
*   **Blue Channel:** The fourth material is blended in based on the **blue** vertex color value.