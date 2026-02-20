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
#include "gl_postprocess.h"
#include "gl_misc.h"
#include "cvar.h"
#include "io_system.h"

void PostProcess_RenderPass(Renderer* renderer, Scene* scene, Engine* engine, Mat4* view, Mat4* projection, GLuint sourceTexture, GLuint destFBO, Int width, Int height) {
    glBindFramebuffer(GL_FRAMEBUFFER, destFBO);
    glViewport(0, 0, width, height);
    if (Cvar_GetInt("r_clear")) {
        glClear(GL_COLOR_BUFFER_BIT);
    }
    glUseProgram(renderer->postProcessShader);
    Shader_Set(renderer->postProcessShader, "resolution", Vec2{ (Float)engine->width, (Float)engine->height });
    Shader_Set(renderer->postProcessShader, "time", engine->scaledTime);
    Shader_Set(renderer->postProcessShader, "u_exposure", renderer->currentExposure);
    Shader_Set(renderer->postProcessShader, "u_gamma", Cvar_GetFloat("r_gamma"));
    Shader_Set(renderer->postProcessShader, "u_red_flash_intensity", engine->red_flash_intensity);
    LogicEntity* fog_ent = FindActiveEntityByClass(scene, "env_fog");
    if (fog_ent) {
        Shader_Set(renderer->postProcessShader, "u_fogEnabled", 1);
        Vec3 fog_color;
        sscanf(LogicEntity_GetProperty(fog_ent, "color", "0.5 0.6 0.7"), "%f %f %f", &fog_color.x, &fog_color.y, &fog_color.z);
        Shader_Set(renderer->postProcessShader, "u_fogColor", fog_color);
        Shader_Set(renderer->postProcessShader, "u_fogStart", (Float)atof(LogicEntity_GetProperty(fog_ent, "start", "50.0")));
        Shader_Set(renderer->postProcessShader, "u_fogEnd", (Float)atof(LogicEntity_GetProperty(fog_ent, "end", "200.0")));
    }
    else {
        Shader_Set(renderer->postProcessShader, "u_fogEnabled", 0);
    }
    Shader_Set(renderer->postProcessShader, "u_postEnabled", (Int)scene->post.enabled);
    Shader_Set(renderer->postProcessShader, "u_crtCurvature", scene->post.crtCurvature);

    Float vignette_strength = Cvar_GetInt("r_vignette") ? scene->post.vignetteStrength : 0.0f;
    Float scanline_strength = Cvar_GetInt("r_scanline") ? scene->post.scanlineStrength : 0.0f;
    Float grain_intensity = Cvar_GetInt("r_filmgrain") ? scene->post.grainIntensity : 0.0f;
    Bool lensflare_enabled = Cvar_GetInt("r_lensflare") && scene->post.lensFlareEnabled;
    Bool ca_enabled = Cvar_GetInt("r_chromaticabberation") && scene->post.chromaticAberrationEnabled;
    Bool bw_enabled = Cvar_GetInt("r_black_white") && scene->post.bwEnabled;
    Bool sharpen_enabled = Cvar_GetInt("r_sharpening") && scene->post.sharpenEnabled;
    Bool invert_enabled = Cvar_GetInt("r_invert") && scene->post.invertEnabled;

    Shader_Set(renderer->postProcessShader, "u_vignetteStrength", vignette_strength);
    Shader_Set(renderer->postProcessShader, "u_vignetteRadius", scene->post.vignetteRadius);
    Shader_Set(renderer->postProcessShader, "u_lensFlareEnabled", (Int)lensflare_enabled);
    Shader_Set(renderer->postProcessShader, "u_lensFlareStrength", scene->post.lensFlareStrength);
    Shader_Set(renderer->postProcessShader, "u_scanlineStrength", scanline_strength);
    Shader_Set(renderer->postProcessShader, "u_grainIntensity", grain_intensity);
    Shader_Set(renderer->postProcessShader, "u_chromaticAberrationEnabled", (Int)ca_enabled);
    Shader_Set(renderer->postProcessShader, "u_chromaticAberrationStrength", scene->post.chromaticAberrationStrength);
    Shader_Set(renderer->postProcessShader, "u_sharpenEnabled", (Int)sharpen_enabled);
    Shader_Set(renderer->postProcessShader, "u_sharpenAmount", scene->post.sharpenAmount);
    Shader_Set(renderer->postProcessShader, "u_bwEnabled", (Int)bw_enabled);
    Shader_Set(renderer->postProcessShader, "u_bwStrength", scene->post.bwStrength);
    Shader_Set(renderer->postProcessShader, "u_invertEnabled", (Int)invert_enabled);
    Shader_Set(renderer->postProcessShader, "u_invertStrength", scene->post.invertStrength);
    Shader_Set(renderer->postProcessShader, "u_isUnderwater", (Int)scene->post.isUnderwater);
    Shader_Set(renderer->postProcessShader, "u_underwaterColor", scene->post.underwaterColor);
    Shader_Set(renderer->postProcessShader, "u_bloomEnabled", Cvar_GetInt("r_bloom"));
    Shader_Set(renderer->postProcessShader, "u_volumetricsEnabled", Cvar_GetInt("r_volumetrics"));
    Shader_Set(renderer->postProcessShader, "u_fadeActive", (Int)scene->post.fade_active);
    Shader_Set(renderer->postProcessShader, "u_fadeAlpha", scene->post.fade_alpha);
    Shader_Set(renderer->postProcessShader, "u_fadeColor", scene->post.fade_color);
    Bool cc_enabled = Cvar_GetInt("r_colorcorrection") && scene->colorCorrection.enabled && scene->colorCorrection.lutTexture != 0;
    Shader_Set(renderer->postProcessShader, "u_colorCorrectionEnabled", (Int)cc_enabled);
    if (cc_enabled) {
        glActiveTexture(GL_TEXTURE6);
        glBindTexture(GL_TEXTURE_2D, scene->colorCorrection.lutTexture);
        Shader_Set(renderer->postProcessShader, "colorCorrectionLUT", 6);
    }
    Vec2 light_pos_on_screen = { -2.0, -2.0 }; Float flare_intensity = 0.0;
    if (scene->numActiveLights > 0) {
        Vec3 light_world_pos = scene->lights[0].pos; Mat4 view_proj; Math::mat4_multiply(&view_proj, projection, view); Float clip_space_pos[4]; Float w = 1.0f;
        clip_space_pos[0] = view_proj.m[0] * light_world_pos.x + view_proj.m[4] * light_world_pos.y + view_proj.m[8] * light_world_pos.z + view_proj.m[12] * w;
        clip_space_pos[1] = view_proj.m[1] * light_world_pos.x + view_proj.m[5] * light_world_pos.y + view_proj.m[9] * light_world_pos.z + view_proj.m[13] * w;
        clip_space_pos[2] = view_proj.m[2] * light_world_pos.x + view_proj.m[6] * light_world_pos.y + view_proj.m[10] * light_world_pos.z + view_proj.m[14] * w;
        clip_space_pos[3] = view_proj.m[3] * light_world_pos.x + view_proj.m[7] * light_world_pos.y + view_proj.m[11] * light_world_pos.z + view_proj.m[15] * w;
        Float clip_w = clip_space_pos[3];
        if (clip_w > 0) {
            Float ndc_x = clip_space_pos[0] / clip_w; Float ndc_y = clip_space_pos[1] / clip_w;
            if (ndc_x > -1.0 && ndc_x < 1.0 && ndc_y > -1.0 && ndc_y < 1.0) { light_pos_on_screen.x = ndc_x * 0.5 + 0.5; light_pos_on_screen.y = ndc_y * 0.5 + 0.5; flare_intensity = 1.0; }
            Shader_Set(renderer->postProcessShader, "u_flareLightWorldPos", light_world_pos);
            Shader_Set(renderer->postProcessShader, "u_view", view);
        }
    }
    Shader_Set(renderer->postProcessShader, "lightPosOnScreen", light_pos_on_screen);
    Shader_Set(renderer->postProcessShader, "flareIntensity", flare_intensity);
    glActiveTexture(GL_TEXTURE0); glBindTexture(GL_TEXTURE_2D, sourceTexture);
    glActiveTexture(GL_TEXTURE1); glBindTexture(GL_TEXTURE_2D, renderer->pingpongColorbuffers[0]);
    glActiveTexture(GL_TEXTURE2); glBindTexture(GL_TEXTURE_2D, renderer->gPosition);
    glActiveTexture(GL_TEXTURE3); glBindTexture(GL_TEXTURE_2D, renderer->volPingpongTextures[0]);
    if (Cvar_GetInt("r_ssao")) {
        glActiveTexture(GL_TEXTURE4);
        glBindTexture(GL_TEXTURE_2D, renderer->ssaoBlurColorBuffer);
    }
    Shader_Set(renderer->postProcessShader, "u_fxaa_enabled", Cvar_GetInt("r_fxaa"));
    Shader_Set(renderer->postProcessShader, "sceneTexture", 0);
    Shader_Set(renderer->postProcessShader, "bloomBlur", 1);
    Shader_Set(renderer->postProcessShader, "gPosition", 2);
    Shader_Set(renderer->postProcessShader, "volumetricTexture", 3);
    Shader_Set(renderer->postProcessShader, "ssao", 4);
    Shader_Set(renderer->postProcessShader, "u_ssaoEnabled", Cvar_GetInt("r_ssao"));
    glBindVertexArray(renderer->quadVAO);
    glDrawArrays(GL_TRIANGLES, 0, 6);
}