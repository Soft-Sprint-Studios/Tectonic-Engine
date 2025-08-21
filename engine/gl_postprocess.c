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
#include "gl_postprocess.h"
#include "cvar.h"
#include "io_system.h"

void PostProcess_RenderPass(Renderer* renderer, Scene* scene, Engine* engine, Mat4* view, Mat4* projection) {
    glBindFramebuffer(GL_FRAMEBUFFER, renderer->finalRenderFBO);
    glViewport(0, 0, engine->width, engine->height);
    if (Cvar_GetInt("r_clear")) {
        glClear(GL_COLOR_BUFFER_BIT);
    }
    glUseProgram(renderer->postProcessShader);
    glUniform2f(glGetUniformLocation(renderer->postProcessShader, "resolution"), engine->width, engine->height);
    glUniform1f(glGetUniformLocation(renderer->postProcessShader, "time"), engine->scaledTime);
    glUniform1f(glGetUniformLocation(renderer->postProcessShader, "u_exposure"), renderer->currentExposure);
    glUniform1f(glGetUniformLocation(renderer->postProcessShader, "u_red_flash_intensity"), engine->red_flash_intensity);
    LogicEntity* fog_ent = FindActiveEntityByClass(scene, "env_fog");
    if (fog_ent) {
        glUniform1i(glGetUniformLocation(renderer->postProcessShader, "u_fogEnabled"), 1);
        Vec3 fog_color;
        sscanf(LogicEntity_GetProperty(fog_ent, "color", "0.5 0.6 0.7"), "%f %f %f", &fog_color.x, &fog_color.y, &fog_color.z);
        glUniform3fv(glGetUniformLocation(renderer->postProcessShader, "u_fogColor"), 1, &fog_color.x);
        glUniform1f(glGetUniformLocation(renderer->postProcessShader, "u_fogStart"), atof(LogicEntity_GetProperty(fog_ent, "start", "50.0")));
        glUniform1f(glGetUniformLocation(renderer->postProcessShader, "u_fogEnd"), atof(LogicEntity_GetProperty(fog_ent, "end", "200.0")));
    }
    else {
        glUniform1i(glGetUniformLocation(renderer->postProcessShader, "u_fogEnabled"), 0);
    }
    glUniform1i(glGetUniformLocation(renderer->postProcessShader, "u_postEnabled"), scene->post.enabled);
    glUniform1f(glGetUniformLocation(renderer->postProcessShader, "u_crtCurvature"), scene->post.crtCurvature);

    float vignette_strength = Cvar_GetInt("r_vignette") ? scene->post.vignetteStrength : 0.0f;
    float scanline_strength = Cvar_GetInt("r_scanline") ? scene->post.scanlineStrength : 0.0f;
    float grain_intensity = Cvar_GetInt("r_filmgrain") ? scene->post.grainIntensity : 0.0f;
    bool lensflare_enabled = Cvar_GetInt("r_lensflare") && scene->post.lensFlareEnabled;
    bool ca_enabled = Cvar_GetInt("r_chromaticabberation") && scene->post.chromaticAberrationEnabled;
    bool bw_enabled = Cvar_GetInt("r_black_white") && scene->post.bwEnabled;
    bool sharpen_enabled = Cvar_GetInt("r_sharpening") && scene->post.sharpenEnabled;
    bool invert_enabled = Cvar_GetInt("r_invert") && scene->post.invertEnabled;

    glUniform1f(glGetUniformLocation(renderer->postProcessShader, "u_vignetteStrength"), vignette_strength);
    glUniform1f(glGetUniformLocation(renderer->postProcessShader, "u_vignetteRadius"), scene->post.vignetteRadius);
    glUniform1i(glGetUniformLocation(renderer->postProcessShader, "u_lensFlareEnabled"), lensflare_enabled);
    glUniform1f(glGetUniformLocation(renderer->postProcessShader, "u_lensFlareStrength"), scene->post.lensFlareStrength);
    glUniform1f(glGetUniformLocation(renderer->postProcessShader, "u_scanlineStrength"), scanline_strength);
    glUniform1f(glGetUniformLocation(renderer->postProcessShader, "u_grainIntensity"), grain_intensity);
    glUniform1i(glGetUniformLocation(renderer->postProcessShader, "u_chromaticAberrationEnabled"), ca_enabled);
    glUniform1f(glGetUniformLocation(renderer->postProcessShader, "u_chromaticAberrationStrength"), scene->post.chromaticAberrationStrength);
    glUniform1i(glGetUniformLocation(renderer->postProcessShader, "u_sharpenEnabled"), sharpen_enabled);
    glUniform1f(glGetUniformLocation(renderer->postProcessShader, "u_sharpenAmount"), scene->post.sharpenAmount);
    glUniform1i(glGetUniformLocation(renderer->postProcessShader, "u_bwEnabled"), bw_enabled);
    glUniform1f(glGetUniformLocation(renderer->postProcessShader, "u_bwStrength"), scene->post.bwStrength);
    glUniform1i(glGetUniformLocation(renderer->postProcessShader, "u_invertEnabled"), invert_enabled);
    glUniform1f(glGetUniformLocation(renderer->postProcessShader, "u_invertStrength"), scene->post.invertStrength);
    glUniform1i(glGetUniformLocation(renderer->postProcessShader, "u_isUnderwater"), scene->post.isUnderwater);
    glUniform3fv(glGetUniformLocation(renderer->postProcessShader, "u_underwaterColor"), 1, &scene->post.underwaterColor.x);
    glUniform1i(glGetUniformLocation(renderer->postProcessShader, "u_bloomEnabled"), Cvar_GetInt("r_bloom"));
    glUniform1i(glGetUniformLocation(renderer->postProcessShader, "u_volumetricsEnabled"), Cvar_GetInt("r_volumetrics"));
    glUniform1i(glGetUniformLocation(renderer->postProcessShader, "u_fadeActive"), scene->post.fade_active);
    glUniform1f(glGetUniformLocation(renderer->postProcessShader, "u_fadeAlpha"), scene->post.fade_alpha);
    glUniform3fv(glGetUniformLocation(renderer->postProcessShader, "u_fadeColor"), 1, &scene->post.fade_color.x);
    bool cc_enabled = Cvar_GetInt("r_colorcorrection") && scene->colorCorrection.enabled && scene->colorCorrection.lutTexture != 0;
    glUniform1i(glGetUniformLocation(renderer->postProcessShader, "u_colorCorrectionEnabled"), cc_enabled);
    if (cc_enabled) {
        glActiveTexture(GL_TEXTURE6);
        glBindTexture(GL_TEXTURE_2D, scene->colorCorrection.lutTexture);
        glUniform1i(glGetUniformLocation(renderer->postProcessShader, "colorCorrectionLUT"), 6);
    }
    Vec2 light_pos_on_screen = { -2.0, -2.0 }; float flare_intensity = 0.0;
    if (scene->numActiveLights > 0) {
        Vec3 light_world_pos = scene->lights[0].position; Mat4 view_proj; mat4_multiply(&view_proj, projection, view); float clip_space_pos[4]; float w = 1.0f;
        clip_space_pos[0] = view_proj.m[0] * light_world_pos.x + view_proj.m[4] * light_world_pos.y + view_proj.m[8] * light_world_pos.z + view_proj.m[12] * w;
        clip_space_pos[1] = view_proj.m[1] * light_world_pos.x + view_proj.m[5] * light_world_pos.y + view_proj.m[9] * light_world_pos.z + view_proj.m[13] * w;
        clip_space_pos[2] = view_proj.m[2] * light_world_pos.x + view_proj.m[6] * light_world_pos.y + view_proj.m[10] * light_world_pos.z + view_proj.m[14] * w;
        clip_space_pos[3] = view_proj.m[3] * light_world_pos.x + view_proj.m[7] * light_world_pos.y + view_proj.m[11] * light_world_pos.z + view_proj.m[15] * w;
        float clip_w = clip_space_pos[3];
        if (clip_w > 0) {
            float ndc_x = clip_space_pos[0] / clip_w; float ndc_y = clip_space_pos[1] / clip_w;
            if (ndc_x > -1.0 && ndc_x < 1.0 && ndc_y > -1.0 && ndc_y < 1.0) { light_pos_on_screen.x = ndc_x * 0.5 + 0.5; light_pos_on_screen.y = ndc_y * 0.5 + 0.5; flare_intensity = 1.0; }
            glUniform3fv(glGetUniformLocation(renderer->postProcessShader, "u_flareLightWorldPos"), 1, &light_world_pos.x);
            glUniformMatrix4fv(glGetUniformLocation(renderer->postProcessShader, "u_view"), 1, GL_FALSE, view->m);
        }
    }
    glUniform2fv(glGetUniformLocation(renderer->postProcessShader, "lightPosOnScreen"), 1, &light_pos_on_screen.x);
    glUniform1f(glGetUniformLocation(renderer->postProcessShader, "flareIntensity"), flare_intensity);
    glActiveTexture(GL_TEXTURE0); glBindTexture(GL_TEXTURE_2D, renderer->gLitColor);
    glActiveTexture(GL_TEXTURE1); glBindTexture(GL_TEXTURE_2D, renderer->pingpongColorbuffers[0]);
    glActiveTexture(GL_TEXTURE2); glBindTexture(GL_TEXTURE_2D, renderer->gPosition);
    glActiveTexture(GL_TEXTURE3); glBindTexture(GL_TEXTURE_2D, renderer->volPingpongTextures[0]);
    if (Cvar_GetInt("r_ssao")) {
        glActiveTexture(GL_TEXTURE4);
        glBindTexture(GL_TEXTURE_2D, renderer->ssaoBlurColorBuffer);
    }
    glUniform1i(glGetUniformLocation(renderer->postProcessShader, "u_fxaa_enabled"), Cvar_GetInt("r_fxaa"));
    glUniform1i(glGetUniformLocation(renderer->postProcessShader, "sceneTexture"), 0);
    glUniform1i(glGetUniformLocation(renderer->postProcessShader, "bloomBlur"), 1);
    glUniform1i(glGetUniformLocation(renderer->postProcessShader, "gPosition"), 2);
    glUniform1i(glGetUniformLocation(renderer->postProcessShader, "volumetricTexture"), 3);
    glUniform1i(glGetUniformLocation(renderer->postProcessShader, "ssao"), 4);
    glUniform1i(glGetUniformLocation(renderer->postProcessShader, "u_ssaoEnabled"), Cvar_GetInt("r_ssao"));
    glBindVertexArray(renderer->quadVAO);
    glDrawArrays(GL_TRIANGLES, 0, 6);
}