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
#ifndef GL_MISC_H
#define GL_MISC_H

//----------------------------------------//
// Brief: The OpenGL shader compilation
//----------------------------------------//

#include <SDL.h>
#include <GL/glew.h>
#include <SDL_opengl.h>
#include <stdio.h>
#include <math_lib.h>


	void load_and_register_named_shader_string(const Char* name, const Char* path);
	Char* load_shader_source(const Char* path);

	GLuint compileShader(GLenum type, const Char* src, const Char* pathHint);
	GLuint createShaderProgram(const Char* vertPath, const Char* fragPath);
	GLuint createShaderProgram(const Char* vertPath, const Char* geomPath, const Char* fragPath);
	GLuint createShaderProgram(const Char* vertPath, const Char* tcsPath, const Char* tesPath, const Char* fragPath);
	GLuint createShaderProgram(const Char* computePath);

	GLint Shader_GetUniformLocation(GLuint program, const Char* name);
	void Shader_Set(GLuint program, const Char* name, Int value);
	void Shader_Set(GLuint program, const Char* name, Float value);
	void Shader_Set(GLuint program, const Char* name, Vec2 value);
	void Shader_Set(GLuint program, const Char* name, Vec3 value);
	void Shader_Set(GLuint program, const Char* name, Vec4 value);
	void Shader_Set(GLuint program, const Char* name, const Mat4* value);
	void Shader_Set(GLuint program, const Char* name, uint64_t handle);


#endif // GL_MISC_H