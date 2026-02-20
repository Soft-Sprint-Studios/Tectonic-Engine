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
#include "cvar.h"
#include "gl_particle_system.h"
#include "map.h"
#include "gl_misc.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <float.h>

static ParticleVertex vboData[MAX_PARTICLES_PER_SYSTEM];

ParticleSystem* ParticleSystem_Load(const Char* path) {
    FILE* file = fopen(path, "r");
    if (!file) return nullptr;

    ParticleSystem* ps = new ParticleSystem();
    if (!ps) { fclose(file); return nullptr; }

    ps->maxParticles = 1000;
    ps->gravity = Vec3{ 0.0f, -9.81f, 0.0f };
    ps->spawnRate = 100.0f;
    ps->lifetime = 2.0f;
    ps->softness = 1.0f;
    ps->startColor = Vec4{ 1.0f, 1.0f, 1.0f, 1.0f };
    ps->endColor = Vec4{ 1.0f, 1.0f, 1.0f, 0.0f };
    ps->startSize = 0.5f;
    ps->endSize = 0.1f;
    ps->material = &g_MissingMaterial;
    ps->blend_sfactor = GL_SRC_ALPHA;
    ps->blend_dfactor = GL_ONE_MINUS_SRC_ALPHA;
    ps->useLighting = true;

    Char line[256];
    while (fgets(line, sizeof(line), file)) {
        Char key[64], value[128];
        if (sscanf(line, "%s %s", key, value) != 2) continue;

        if (strcmp(key, "maxParticles") == 0) ps->maxParticles = atoi(value);
        else if (strcmp(key, "spawnRate") == 0) ps->spawnRate = atof(value);
        else if (strcmp(key, "softness") == 0) ps->softness = atof(value);
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
        else if (strcmp(key, "useLighting") == 0) ps->useLighting = (atoi(value) != 0);
    }
    fclose(file);

    if (ps->maxParticles > MAX_PARTICLES_PER_SYSTEM) ps->maxParticles = MAX_PARTICLES_PER_SYSTEM;
    ps->shader = createShaderProgram("shaders/particle.vert", "shaders/particle.geom", "shaders/particle.frag");
    return ps;
}

void ParticleSystem_Free(ParticleSystem* system) {
    if (!system) return;
    glDeleteProgram(system->shader);
    delete system;
}

static Int find_unused_particle(ParticleEmitter* emitter) {
    for (Int i = emitter->activeParticles; i < emitter->system->maxParticles; ++i) if (emitter->particles[i].life < 0.0f) return i;
    for (Int i = 0; i < emitter->activeParticles; ++i) if (emitter->particles[i].life < 0.0f) return i;
    return -1;
}

static void respawn_particle(ParticleEmitter* emitter, Particle* p) {
    ParticleSystem* ps = emitter->system;
    p->position = emitter->pos;
    p->velocity.x = ps->startVelocity.x + Math::rand_float_range(-ps->velocityVariation.x, ps->velocityVariation.x);
    p->velocity.y = ps->startVelocity.y + Math::rand_float_range(-ps->velocityVariation.y, ps->velocityVariation.y);
    p->velocity.z = ps->startVelocity.z + Math::rand_float_range(-ps->velocityVariation.z, ps->velocityVariation.z);
    p->color = ps->startColor;
    p->life = ps->lifetime + Math::rand_float_range(-ps->lifetimeVariation, ps->lifetimeVariation);
    p->size = ps->startSize;
    p->angle = ps->startAngle + Math::rand_float_range(-ps->angleVariation, ps->angleVariation);
    p->angularVelocity = ps->startAngularVelocity + Math::rand_float_range(-ps->angularVelocityVariation, ps->angularVelocityVariation);
}

void ParticleEmitter_Init(ParticleEmitter* emitter, ParticleSystem* system, Vec3 position) {
    emitter->system = system;
    emitter->pos = position;
    emitter->is_on = emitter->on_by_default;
    emitter->activeParticles = 0;
    emitter->timeSinceLastSpawn = 0.0f;
    for (Int i = 0; i < emitter->system->maxParticles; ++i) emitter->particles[i].life = -1.0f;
    glGenVertexArrays(1, &emitter->vao);
    glGenBuffers(1, &emitter->vbo);
    glBindVertexArray(emitter->vao);
    glBindBuffer(GL_ARRAY_BUFFER, emitter->vbo);
    glBufferData(GL_ARRAY_BUFFER, emitter->system->maxParticles * sizeof(ParticleVertex), nullptr, GL_STREAM_DRAW);
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

void ParticleEmitter_Update(ParticleEmitter* emitter, Float deltaTime) {
    if (!emitter || !emitter->system) return;
    ParticleSystem* ps = emitter->system;

    if (emitter->is_on) {
        emitter->timeSinceLastSpawn += deltaTime;
        Int particlesToSpawn = (Int)(emitter->timeSinceLastSpawn * ps->spawnRate);
        if (particlesToSpawn > 0) {
            emitter->timeSinceLastSpawn = 0.0f;
        }

        for (Int i = 0; i < particlesToSpawn; ++i) {
            Int particleIndex = find_unused_particle(emitter);
            if (particleIndex != -1) {
                respawn_particle(emitter, &emitter->particles[particleIndex]);
            }
        }
    }

    emitter->activeParticles = 0;
    for (Int i = 0; i < ps->maxParticles; ++i) {
        Particle* p = &emitter->particles[i];
        if (p->life > 0.0f) {
            p->life -= deltaTime;
            if (p->life > 0.0f) {
                p->velocity = Math::vec3_add(p->velocity, Math::vec3_muls(ps->gravity, deltaTime));
                p->position = Math::vec3_add(p->position, Math::vec3_muls(p->velocity, deltaTime));
                p->angle += p->angularVelocity * deltaTime;
                Float lifeRatio = 1.0f - (p->life / (ps->lifetime + Math::rand_float_range(-ps->lifetimeVariation, ps->lifetimeVariation)));
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

void ParticleEmitter_Render(ParticleEmitter* emitter, void* scene_ptr, void* engine_ptr, Mat4 view, Mat4 projection, GLuint gPosition, Float screenWidth, Float screenHeight) {
    if (!emitter || !emitter->system || emitter->activeParticles == 0) return;

    Scene* scene = (Scene*)scene_ptr;
    Engine* engine = (Engine*)engine_ptr;
    ParticleSystem* ps = emitter->system;

    glUseProgram(ps->shader);
    Shader_Set(ps->shader, "view", &view);
    Shader_Set(ps->shader, "projection", &projection);
    Shader_Set(ps->shader, "viewPos", engine->camera.position);
    Shader_Set(ps->shader, "u_useLighting", (Int)ps->useLighting);

    Shader_Set(ps->shader, "sun.enabled", (Int)scene->sun.enabled);
    Shader_Set(ps->shader, "sun.direction", scene->sun.direction);
    Shader_Set(ps->shader, "sun.color", scene->sun.color);
    Shader_Set(ps->shader, "sun.intensity", scene->sun.intensity);

    Shader_Set(ps->shader, "flashlight.enabled", (Int)engine->flashlight_on);
    if (engine->flashlight_on) {
        Vec3 forward = { cosf(engine->camera.pitch) * sinf(engine->camera.yaw), sinf(engine->camera.pitch), -cosf(engine->camera.pitch) * cosf(engine->camera.yaw) };
        Math::vec3_normalize(&forward);
        Shader_Set(ps->shader, "flashlight.position", engine->camera.position);
        Shader_Set(ps->shader, "flashlight.direction", forward);
    }

    Shader_Set(ps->shader, "u_numAmbientProbes", scene->num_ambient_probes);
    if (scene->num_ambient_probes > 0) {
        AmbientProbe* nearest_probes[8] = { nullptr };
        Float distances[8];
        for (Int k = 0; k < 8; ++k) distances[k] = FLT_MAX;

        for (Int p_idx = 0; p_idx < scene->num_ambient_probes; ++p_idx) {
            Float d = Math::vec3_length_sq(Math::vec3_sub(engine->camera.position, scene->ambient_probes[p_idx].position));
            for (Int k = 0; k < 8; ++k) {
                if (d < distances[k]) {
                    for (Int l = 7; l > k; --l) {
                        distances[l] = distances[l - 1];
                        nearest_probes[l] = nearest_probes[l - 1];
                    }
                    distances[k] = d;
                    nearest_probes[k] = &scene->ambient_probes[p_idx];
                    break;
                }
            }
        }

        for (Int k = 0; k < 8; ++k) {
            Char buf[64];
            if (nearest_probes[k]) {
                sprintf(buf, "u_probes[%d].position", k);
                Shader_Set(ps->shader, buf, nearest_probes[k]->position);
                for (Int f = 0; f < 6; ++f) {
                    sprintf(buf, "u_probes[%d].colors[%d]", k, f);
                    Shader_Set(ps->shader, buf, nearest_probes[k]->colors[f]);
                }
            }
        }
    }

    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, ps->material->diffuseMap);
    Shader_Set(ps->shader, "particleTexture", 0);

    glActiveTexture(GL_TEXTURE1);
    glBindTexture(GL_TEXTURE_2D, gPosition);
    Shader_Set(ps->shader, "gPosition", 1);
    Shader_Set(ps->shader, "screenSize", Vec2{ screenWidth, screenHeight });
    Shader_Set(ps->shader, "softness", (Cvar_GetInt("r_particles_soft") ? (ps->softness < 0.001f ? 0.001f : ps->softness) : 0.001f));

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