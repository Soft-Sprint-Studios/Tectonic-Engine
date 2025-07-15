/*
 * Copyright © 2025 Soft Sprint Studios
 * All rights reserved.
 *
 * This file is proprietary and confidential. Unauthorized reproduction,
 * modification, or distribution is strictly prohibited unless explicit
 * written permission is granted by Soft Sprint Studios.
 */
#ifndef GL_MISC_H
#define GL_MISC_H

//----------------------------------------//
// Brief: The OpenGL shader compilation
//----------------------------------------//

#include <SDL.h>
#include <GL/glew.h>
#include <SDL_opengl.h>
#include <stdio.h>

#ifdef __cplusplus
extern "C" {
#endif

char* load_shader_source(const char* path);
GLuint compileShader(GLenum type, const char* src);
GLuint createShaderProgram(const char* vertPath, const char* fragPath);
GLuint createShaderProgramGeom(const char* vertPath, const char* geomPath, const char* fragPath);
GLuint createShaderProgramTess(const char* vertPath, const char* tcsPath, const char* tesPath, const char* fragPath);
GLuint createShaderProgramCompute(const char* computePath);

#ifdef __cplusplus
}
#endif

#endif // GL_MISC_H