/*
 * Copyright © 2025 Soft Sprint Studios
 * All rights reserved.
 *
 * This file is proprietary and confidential. Unauthorized reproduction,
 * modification, or distribution is strictly prohibited unless explicit
 * written permission is granted by Soft Sprint Studios.
 */
#ifndef PARTICLE_SYSTEM_H
#define PARTICLE_SYSTEM_H

//----------------------------------------//
// Brief: Particles rendering and update
//----------------------------------------//

#include "math_lib.h"
#include "texturemanager.h"
#include <GL/glew.h>

#ifdef __cplusplus
extern "C" {
#endif

#define MAX_PARTICLES_PER_SYSTEM 8192

typedef struct {
    Vec3 position;
    Vec3 velocity;
    Vec4 color;
    float size;
    float life;
    float angle;
    float angularVelocity;
} Particle;

typedef struct {
    char name[64];
    Vec3 gravity;
    float spawnRate;
    float lifetime;
    float lifetimeVariation;
    Vec4 startColor;
    Vec4 endColor;
    float startSize;
    float endSize;
    float startAngle;
    float angleVariation;
    float startAngularVelocity;
    float angularVelocityVariation;
    Vec3 startVelocity;
    Vec3 velocityVariation;
    int maxParticles;
    Material* material;
    GLuint shader;
    GLenum blend_sfactor;
    GLenum blend_dfactor;
} ParticleSystem;

typedef struct {
    Vec3 position;
    float size;
    float angle;
    Vec4 color;
} ParticleVertex;

struct ParticleEmitter;

ParticleSystem* ParticleSystem_Load(const char* path);
void ParticleSystem_Free(ParticleSystem* system);
void ParticleEmitter_Init(struct ParticleEmitter* emitter, ParticleSystem* system, Vec3 position);
void ParticleEmitter_Update(struct ParticleEmitter* emitter, float deltaTime);
void ParticleEmitter_Render(struct ParticleEmitter* emitter, Mat4 view, Mat4 projection);
void ParticleEmitter_Free(struct ParticleEmitter* emitter);

#ifdef __cplusplus
}
#endif

#endif // PARTICLE_SYSTEM_H