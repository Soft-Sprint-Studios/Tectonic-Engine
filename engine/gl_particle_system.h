/*
 * MIT License
 *
 * Copyright (c) 2025-2026 Soft Sprint Studios
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


constexpr int MAX_PARTICLES_PER_SYSTEM = 8192;

    typedef struct {
        Vec3 position;
        Vec3 velocity;
        Vec4 color;
        Float size;
        Float life;
        Float angle;
        Float angularVelocity;
    } Particle;

    typedef struct {
        Char name[64];
        Vec3 gravity;
        Float spawnRate;
        Float lifetime;
        Float lifetimeVariation;
        Vec4 startColor;
        Vec4 endColor;
        Float startSize;
        Float endSize;
        Float startAngle;
        Float angleVariation;
        Float startAngularVelocity;
        Float angularVelocityVariation;
        Float softness;
        Vec3 startVelocity;
        Vec3 velocityVariation;
        Int maxParticles;
        Material* material;
        GLuint shader;
        GLenum blend_sfactor;
        GLenum blend_dfactor;
        Bool useLighting;
    } ParticleSystem;

    typedef struct {
        Vec3 position;
        Float size;
        Float angle;
        Vec4 color;
    } ParticleVertex;

    struct ParticleEmitter;

    ParticleSystem* ParticleSystem_Load(const Char* path);
    void ParticleSystem_Free(ParticleSystem* system);
    void ParticleEmitter_Init(struct ParticleEmitter* emitter, ParticleSystem* system, Vec3 position);
    void ParticleEmitter_Update(struct ParticleEmitter* emitter, Float deltaTime);
    void ParticleEmitter_Render(struct ParticleEmitter* emitter, void* scene, void* engine, Mat4 view, Mat4 projection, GLuint gPosition, Float screenWidth, Float screenHeight);
    void ParticleEmitter_Free(struct ParticleEmitter* emitter);


#endif // GL_PARTICLE_SYSTEM_H