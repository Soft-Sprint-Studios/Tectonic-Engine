/*
 * MIT License
 *
 * Copyright (c) 2025 Soft Sprint Studios
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */
#pragma once
#ifndef GL_PARTICLE_SYSTEM_H
#define GL_PARTICLE_SYSTEM_H

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

#endif // GL_PARTICLE_SYSTEM_H