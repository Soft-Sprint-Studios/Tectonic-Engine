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
#include "gl_particle_system.h"
#include "map.h"
#include "gl_misc.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

static ParticleVertex vboData[MAX_PARTICLES_PER_SYSTEM];

ParticleSystem* ParticleSystem_Load(const char* path) {
    FILE* file = fopen(path, "r");
    if (!file) return NULL;

    ParticleSystem* ps = (ParticleSystem*)calloc(1, sizeof(ParticleSystem));
    if (!ps) { fclose(file); return NULL; }

    ps->maxParticles = 1000;
    ps->gravity = (Vec3){ 0.0f, -9.81f, 0.0f };
    ps->spawnRate = 100.0f;
    ps->lifetime = 2.0f;
    ps->startColor = (Vec4){ 1.0f, 1.0f, 1.0f, 1.0f };
    ps->endColor = (Vec4){ 1.0f, 1.0f, 1.0f, 0.0f };
    ps->startSize = 0.5f;
    ps->endSize = 0.1f;
    ps->material = &g_MissingMaterial;
    ps->blend_sfactor = GL_SRC_ALPHA;
    ps->blend_dfactor = GL_ONE_MINUS_SRC_ALPHA;

    char line[256];
    while (fgets(line, sizeof(line), file)) {
        char key[64], value[128];
        if (sscanf(line, "%s %s", key, value) != 2) continue;

        if (strcmp(key, "maxParticles") == 0) ps->maxParticles = atoi(value);
        else if (strcmp(key, "spawnRate") == 0) ps->spawnRate = atof(value);
        else if (strcmp(key, "lifetime") == 0) ps->lifetime = atof(value);
        else if (strcmp(key, "lifetimeVariation") == 0) ps->lifetimeVariation = atof(value);
        else if (strcmp(key, "startSize") == 0) ps->startSize = atof(value);
        else if (strcmp(key, "endSize") == 0) ps->endSize = atof(value);
        else if (strcmp(key, "startAngle") == 0) ps->startAngle = atof(value);
        else if (strcmp(key, "angleVariation") == 0) ps->angleVariation = atof(value);
        else if (strcmp(key, "startAngularVelocity") == 0) ps->startAngularVelocity = atof(value);
        else if (strcmp(key, "angularVelocityVariation") == 0) ps->angularVelocityVariation = atof(value);
        else if (strcmp(key, "texture") == 0) ps->material = TextureManager_FindMaterial(value);
        else if (strcmp(key, "gravity") == 0) sscanf(value, "%f,%f,%f", &ps->gravity.x, &ps->gravity.y, &ps->gravity.z);
        else if (strcmp(key, "startColor") == 0) sscanf(value, "%f,%f,%f,%f", &ps->startColor.x, &ps->startColor.y, &ps->startColor.z, &ps->startColor.w);
        else if (strcmp(key, "endColor") == 0) sscanf(value, "%f,%f,%f,%f", &ps->endColor.x, &ps->endColor.y, &ps->endColor.z, &ps->endColor.w);
        else if (strcmp(key, "startVelocity") == 0) sscanf(value, "%f,%f,%f", &ps->startVelocity.x, &ps->startVelocity.y, &ps->startVelocity.z);
        else if (strcmp(key, "velocityVariation") == 0) sscanf(value, "%f,%f,%f", &ps->velocityVariation.x, &ps->velocityVariation.y, &ps->velocityVariation.z);
        else if (strcmp(key, "blendFunc") == 0 && strcmp(value, "additive") == 0) {
            ps->blend_sfactor = GL_SRC_ALPHA; ps->blend_dfactor = GL_ONE;
        }
    }
    fclose(file);

    if (ps->maxParticles > MAX_PARTICLES_PER_SYSTEM) ps->maxParticles = MAX_PARTICLES_PER_SYSTEM;
    ps->shader = createShaderProgramGeom("shaders/particle.vert", "shaders/particle.geom", "shaders/particle.frag");
    return ps;
}

void ParticleSystem_Free(ParticleSystem* system) {
    if (!system) return;
    glDeleteProgram(system->shader);
    free(system);
}

static int find_unused_particle(ParticleEmitter* emitter) {
    for (int i = emitter->activeParticles; i < emitter->system->maxParticles; ++i) if (emitter->particles[i].life < 0.0f) return i;
    for (int i = 0; i < emitter->activeParticles; ++i) if (emitter->particles[i].life < 0.0f) return i;
    return -1;
}

static void respawn_particle(ParticleEmitter* emitter, Particle* p) {
    ParticleSystem* ps = emitter->system;
    p->position = emitter->pos;
    p->velocity.x = ps->startVelocity.x + rand_float_range(-ps->velocityVariation.x, ps->velocityVariation.x);
    p->velocity.y = ps->startVelocity.y + rand_float_range(-ps->velocityVariation.y, ps->velocityVariation.y);
    p->velocity.z = ps->startVelocity.z + rand_float_range(-ps->velocityVariation.z, ps->velocityVariation.z);
    p->color = ps->startColor;
    p->life = ps->lifetime + rand_float_range(-ps->lifetimeVariation, ps->lifetimeVariation);
    p->size = ps->startSize;
    p->angle = ps->startAngle + rand_float_range(-ps->angleVariation, ps->angleVariation);
    p->angularVelocity = ps->startAngularVelocity + rand_float_range(-ps->angularVelocityVariation, ps->angularVelocityVariation);
}

void ParticleEmitter_Init(ParticleEmitter* emitter, ParticleSystem* system, Vec3 position) {
    emitter->system = system;
    emitter->pos = position;
    emitter->is_on = emitter->on_by_default;
    emitter->activeParticles = 0;
    emitter->timeSinceLastSpawn = 0.0f;
    for (int i = 0; i < emitter->system->maxParticles; ++i) emitter->particles[i].life = -1.0f;
    glGenVertexArrays(1, &emitter->vao);
    glGenBuffers(1, &emitter->vbo);
    glBindVertexArray(emitter->vao);
    glBindBuffer(GL_ARRAY_BUFFER, emitter->vbo);
    glBufferData(GL_ARRAY_BUFFER, emitter->system->maxParticles * sizeof(ParticleVertex), NULL, GL_STREAM_DRAW);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(ParticleVertex), (void*)offsetof(ParticleVertex, position));
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 1, GL_FLOAT, GL_FALSE, sizeof(ParticleVertex), (void*)offsetof(ParticleVertex, size));
    glEnableVertexAttribArray(2);
    glVertexAttribPointer(2, 1, GL_FLOAT, GL_FALSE, sizeof(ParticleVertex), (void*)offsetof(ParticleVertex, angle));
    glEnableVertexAttribArray(3);
    glVertexAttribPointer(3, 4, GL_FLOAT, GL_FALSE, sizeof(ParticleVertex), (void*)offsetof(ParticleVertex, color));
    glBindVertexArray(0);
}

void ParticleEmitter_Update(ParticleEmitter* emitter, float deltaTime) {
    if (!emitter || !emitter->system) return;
    ParticleSystem* ps = emitter->system;

    if (emitter->is_on) {
        emitter->timeSinceLastSpawn += deltaTime;
        int particlesToSpawn = (int)(emitter->timeSinceLastSpawn * ps->spawnRate);
        if (particlesToSpawn > 0) {
            emitter->timeSinceLastSpawn = 0.0f;
        }

        for (int i = 0; i < particlesToSpawn; ++i) {
            int particleIndex = find_unused_particle(emitter);
            if (particleIndex != -1) {
                respawn_particle(emitter, &emitter->particles[particleIndex]);
            }
        }
    }

    emitter->activeParticles = 0;
    for (int i = 0; i < ps->maxParticles; ++i) {
        Particle* p = &emitter->particles[i];
        if (p->life > 0.0f) {
            p->life -= deltaTime;
            if (p->life > 0.0f) {
                p->velocity = vec3_add(p->velocity, vec3_muls(ps->gravity, deltaTime));
                p->position = vec3_add(p->position, vec3_muls(p->velocity, deltaTime));
                p->angle += p->angularVelocity * deltaTime;
                float lifeRatio = 1.0f - (p->life / (ps->lifetime + rand_float_range(-ps->lifetimeVariation, ps->lifetimeVariation)));
                p->color.x = ps->startColor.x + (ps->endColor.x - ps->startColor.x) * lifeRatio;
                p->color.y = ps->startColor.y + (ps->endColor.y - ps->startColor.y) * lifeRatio;
                p->color.z = ps->startColor.z + (ps->endColor.z - ps->startColor.z) * lifeRatio;
                p->color.w = ps->startColor.w + (ps->endColor.w - ps->startColor.w) * lifeRatio;
                p->size = ps->startSize + (ps->endSize - ps->startSize) * lifeRatio;
                if (emitter->activeParticles < ps->maxParticles) {
                    vboData[emitter->activeParticles].position = p->position;
                    vboData[emitter->activeParticles].size = p->size;
                    vboData[emitter->activeParticles].angle = p->angle;
                    vboData[emitter->activeParticles].color = p->color;
                    emitter->activeParticles++;
                }
            }
            else p->life = -1.0f;
        }
    }
    if (emitter->activeParticles > 0) {
        glBindBuffer(GL_ARRAY_BUFFER, emitter->vbo);
        glBufferSubData(GL_ARRAY_BUFFER, 0, emitter->activeParticles * sizeof(ParticleVertex), vboData);
        glBindBuffer(GL_ARRAY_BUFFER, 0);
    }
}

void ParticleEmitter_Render(ParticleEmitter* emitter, Mat4 view, Mat4 projection) {
    if (!emitter || !emitter->system || emitter->activeParticles == 0) return;
    ParticleSystem* ps = emitter->system;
    glUseProgram(ps->shader);
    glUniformMatrix4fv(glGetUniformLocation(ps->shader, "view"), 1, GL_FALSE, view.m);
    glUniformMatrix4fv(glGetUniformLocation(ps->shader, "projection"), 1, GL_FALSE, projection.m);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, ps->material->diffuseMap);
    glUniform1i(glGetUniformLocation(ps->shader, "particleTexture"), 0);
    glBlendFunc(ps->blend_sfactor, ps->blend_dfactor);
    glBindVertexArray(emitter->vao);
    glDrawArrays(GL_POINTS, 0, emitter->activeParticles);
    glBindVertexArray(0);
}

void ParticleEmitter_Free(ParticleEmitter* emitter) {
    if (!emitter) return;
    glDeleteVertexArrays(1, &emitter->vao);
    glDeleteBuffers(1, &emitter->vbo);
}