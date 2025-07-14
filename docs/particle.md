
The Tectonic Engine uses a flexible, text-file-based particle system to create a wide variety of visual effects, such as smoke, fire, sparks, and magical effects.

## Particle Definition Files (`.par`)

The behavior of a particle system is defined in a `.par` file, which should be placed in the `particles/` directory. This file uses a simple key-value format.

### Example Definition (`fire.par`)

```
maxParticles = 1000
spawnRate = 200.0
lifetime = 1.5
lifetimeVariation = 0.5
texture = "particles/fire_atlas.png"
blendFunc = additive
startSize = 0.2
endSize = 1.0
startAngle = 0.0
angleVariation = 360.0
startAngularVelocity = 10.0
angularVelocityVariation = 5.0
startColor = "1.0, 0.5, 0.2, 1.0"
endColor = "1.0, 0.2, 0.0, 0.0"
gravity = "0.0, 1.0, 0.0"
startVelocity = "0.0, 0.8, 0.0"
velocityVariation = "0.3, 0.5, 0.3"
```

### Particle Properties

| Property | Description |
| :--- | :--- |
| `maxParticles` | The maximum number of particles that can be alive at once in an emitter. |
| `spawnRate` | The number of particles to spawn per second. |
| `lifetime` | The base lifetime of a particle in seconds. |
| `lifetimeVariation`| A random value between +/- this number is added to `lifetime`. |
| `texture` | The material definition to use for the particle's texture. |
| `blendFunc` | Can be `default` (alpha blend) or `additive` for effects like fire. |
| `startSize` | The initial size of a particle when it spawns. |
| `endSize` | The final size of a particle just before it dies. |
| `startColor` | The initial color tint of the particle (RGBA). |
| `endColor` | The final color of the particle. The particle will linearly interpolate between the start and end colors over its lifetime. |
| `startAngle` | The initial rotation of the particle in degrees. |
| `angleVariation` | A random value between +/- this number is added to the starting angle. |
| `startAngularVelocity`| The initial speed of rotation in degrees per second. |
| `angularVelocityVariation`| A random value between +/- this number is added to the angular velocity. |
| `gravity` | A constant acceleration vector applied to all particles. |
| `startVelocity` | The initial base velocity of a particle. |
| `velocityVariation`| A random vector between +/- this value is added to the start velocity. |

## Particle Emitter Entity

To use a particle system in a map, you must place a **Particle Emitter** entity. This entity is responsible for spawning and managing the particles defined in its associated `.par` file. Emitters can be controlled via the I/O system, allowing them to be turned on or off by triggers or other game events.

## Rendering

Particles are rendered as camera-facing billboards (quads) using a geometry shader. This ensures they always face the player, creating a convincing volumetric effect. The `blendFunc` property is crucial for how the particles interact with the scene behind them.