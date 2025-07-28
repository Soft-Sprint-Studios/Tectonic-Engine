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
GLuint compileShader(GLenum type, const char* src, const char* pathHint);
GLuint createShaderProgram(const char* vertPath, const char* fragPath);
GLuint createShaderProgramGeom(const char* vertPath, const char* geomPath, const char* fragPath);
GLuint createShaderProgramTess(const char* vertPath, const char* tcsPath, const char* tesPath, const char* fragPath);
GLuint createShaderProgramCompute(const char* computePath);
void GL_InitDebugOutput(void);

#ifdef __cplusplus
}
#endif

#endif // GL_MISC_H