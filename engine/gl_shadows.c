#include "gl_shadows.h"
#include "gl_misc.h"
#include "cvar.h"

void render_object(GLuint shader, SceneObject* obj, bool is_baking_pass, const Frustum* frustum);
void render_brush(GLuint shader, Brush* b, bool is_baking_pass, const Frustum* frustum);

void Shadows_RenderPointAndSpot(Renderer* renderer, Scene* scene, Engine* engine) {
    glEnable(GL_DEPTH_TEST);
    glCullFace(GL_FRONT);
    int shadow_map_size = Cvar_GetInt("r_shadow_map_size");
    if (shadow_map_size <= 0) {
        shadow_map_size = 1024;
    }
    float max_shadow_dist = Cvar_GetFloat("r_shadow_distance_max");
    float max_shadow_dist_sq = max_shadow_dist * max_shadow_dist;
    glViewport(0, 0, shadow_map_size, shadow_map_size);

    for (int i = 0; i < scene->numActiveLights; ++i) {
        Light* light = &scene->lights[i];
        if (light->is_static_shadow && light->has_rendered_static_shadow) {
            continue;
        }
        if (light->is_static) continue;
        if (light->intensity <= 0.0f) continue;
        if (vec3_length_sq(vec3_sub(light->position, engine->camera.position)) > max_shadow_dist_sq) continue;
        
        glBindFramebuffer(GL_FRAMEBUFFER, light->shadowFBO);
        glClear(GL_DEPTH_BUFFER_BIT);
        GLuint current_shader;
        if (light->type == LIGHT_POINT) {
            current_shader = renderer->pointDepthShader;
            glUseProgram(current_shader);
            Mat4 shadowProj = mat4_perspective(90.0f * M_PI / 180.0f, 1.0f, 1.0f, light->shadowFarPlane);
            Mat4 shadowTransforms[6];
            shadowTransforms[0] = mat4_lookAt(light->position, vec3_add(light->position, (Vec3) { 1, 0, 0 }), (Vec3) { 0, -1, 0 });
            shadowTransforms[1] = mat4_lookAt(light->position, vec3_add(light->position, (Vec3) { -1, 0, 0 }), (Vec3) { 0, -1, 0 });
            shadowTransforms[2] = mat4_lookAt(light->position, vec3_add(light->position, (Vec3) { 0, 1, 0 }), (Vec3) { 0, 0, 1 });
            shadowTransforms[3] = mat4_lookAt(light->position, vec3_add(light->position, (Vec3) { 0, -1, 0 }), (Vec3) { 0, 0, -1 });
            shadowTransforms[4] = mat4_lookAt(light->position, vec3_add(light->position, (Vec3) { 0, 0, 1 }), (Vec3) { 0, -1, 0 });
            shadowTransforms[5] = mat4_lookAt(light->position, vec3_add(light->position, (Vec3) { 0, 0, -1 }), (Vec3) { 0, -1, 0 });
            for (int j = 0; j < 6; ++j) {
                mat4_multiply(&shadowTransforms[j], &shadowProj, &shadowTransforms[j]);
                char uName[64];
                sprintf(uName, "shadowMatrices[%d]", j);
                glUniformMatrix4fv(glGetUniformLocation(current_shader, uName), 1, GL_FALSE, shadowTransforms[j].m);
            }
            glUniform1f(glGetUniformLocation(current_shader, "far_plane"), light->shadowFarPlane);
            glUniform3fv(glGetUniformLocation(current_shader, "lightPos"), 1, &light->position.x);
        }
        else {
            current_shader = renderer->spotDepthShader;
            glUseProgram(current_shader);
            float angle_rad = acosf(fmaxf(-1.0f, fminf(1.0f, light->cutOff))); if (angle_rad < 0.01f) angle_rad = 0.01f;
            Mat4 lightProjection = mat4_perspective(angle_rad * 2.0f, 1.0f, 1.0f, light->shadowFarPlane);
            Vec3 up_vector = (Vec3){ 0, 1, 0 }; if (fabs(vec3_dot(light->direction, up_vector)) > 0.99f) { up_vector = (Vec3){ 1, 0, 0 }; }
            Mat4 lightView = mat4_lookAt(light->position, vec3_add(light->position, light->direction), up_vector);
            Mat4 lightSpaceMatrix; mat4_multiply(&lightSpaceMatrix, &lightProjection, &lightView);
            glUniformMatrix4fv(glGetUniformLocation(current_shader, "lightSpaceMatrix"), 1, GL_FALSE, lightSpaceMatrix.m);
        }
        for (int j = 0; j < scene->numObjects; ++j) {
            if (!scene->objects[j].casts_shadows) continue;
            render_object(current_shader, &scene->objects[j], false, NULL);
        }
        for (int j = 0; j < scene->numBrushes; ++j) {
            render_brush(current_shader, &scene->brushes[j], false, NULL);
        }
        if (light->is_static_shadow) {
            light->has_rendered_static_shadow = true;
        }
    }
    glCullFace(GL_BACK);
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

void Shadows_RenderSun(Renderer* renderer, Scene* scene, const Mat4* sunLightSpaceMatrix) {
    glEnable(GL_DEPTH_TEST);
    glCullFace(GL_FRONT);
    glViewport(0, 0, SUN_SHADOW_MAP_SIZE, SUN_SHADOW_MAP_SIZE);
    glBindFramebuffer(GL_FRAMEBUFFER, renderer->sunShadowFBO);
    glClear(GL_DEPTH_BUFFER_BIT);

    glUseProgram(renderer->spotDepthShader);
    glUniformMatrix4fv(glGetUniformLocation(renderer->spotDepthShader, "lightSpaceMatrix"), 1, GL_FALSE, sunLightSpaceMatrix->m);

    for (int j = 0; j < scene->numObjects; ++j) {
        if (!scene->objects[j].casts_shadows) continue;
        render_object(renderer->spotDepthShader, &scene->objects[j], false, NULL);
    }
    for (int j = 0; j < scene->numBrushes; ++j) {
        Brush* b = &scene->brushes[j];
        if (strcmp(b->classname, "func_wall_toggle") == 0 && !b->runtime_is_visible) {
            continue;
        }
        if (strcmp(scene->brushes[j].classname, "env_reflectionprobe") == 0) continue;
        render_brush(renderer->spotDepthShader, &scene->brushes[j], false, NULL);
    }

    glCullFace(GL_BACK);
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

void Shadows_Init(Renderer* renderer) {
    renderer->pointDepthShader = createShaderProgramGeom("shaders/depth_point.vert", "shaders/depth_point.geom", "shaders/depth_point.frag");
    renderer->spotDepthShader = createShaderProgram("shaders/depth_spot.vert", "shaders/depth_spot.frag");
}

void Shadows_Shutdown(Renderer* renderer) {
    glDeleteProgram(renderer->pointDepthShader);
    glDeleteProgram(renderer->spotDepthShader);
}