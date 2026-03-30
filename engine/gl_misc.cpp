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
#include "gl_misc.h"
#include "gl_console.h"
#include <stdlib.h>
#include <unordered_map>
#include <string>

Char* load_shader_source(const Char* path) {
    Char* buffer = nullptr;
    Long length = 0;
    FILE* f = fopen(path, "rb");
    if (f) {
        fseek(f, 0, SEEK_END);
        length = ftell(f);
        fseek(f, 0, SEEK_SET);

        buffer = new Char[length + 1];
        if (buffer) {
            Usize read_bytes = fread(buffer, 1, length, f);
            buffer[read_bytes] = '\0';
        }

        fclose(f);
    }
    else {
        Console::Printf_Error("Could not open shader file %s\n", path);
    }
    return buffer;
}

void load_and_register_named_shader_string(const Char* name, const Char* path) {
    Char* source = load_shader_source(path);
    if (source) {
        glNamedStringARB(GL_SHADER_INCLUDE_ARB, -1, name, -1, source);
        delete[] source;
    }
}

GLuint compileShader(GLenum type, const Char* src, const Char* pathHint) {
    GLuint shader = glCreateShader(type);
    const Char* header =
        "#version 460 core\n"
        "#extension GL_ARB_bindless_texture : require\n"
        "#extension GL_ARB_shading_language_include : require\n"
        "#include \"/common.h\"\n"
        "#include \"/pbr.h\"\n"
        "#line 1\n";
    const Char* sources[2] = { header, src };
    glShaderSource(shader, 2, sources, nullptr);
    glCompileShader(shader);
    GLint success;
    glGetShaderiv(shader, GL_COMPILE_STATUS, &success);
    if (!success) {
        GLchar infoLog[1024];
        glGetShaderInfoLog(shader, 1024, nullptr, infoLog);
        const Char* typeStr =
            type == GL_VERTEX_SHADER ? "VERTEX" :
            type == GL_FRAGMENT_SHADER ? "FRAGMENT" :
            type == GL_GEOMETRY_SHADER ? "GEOMETRY" :
            type == GL_TESS_CONTROL_SHADER ? "TESS CONTROL" :
            type == GL_TESS_EVALUATION_SHADER ? "TESS EVALUATION" :
            type == GL_COMPUTE_SHADER ? "COMPUTE" : "UNKNOWN";
        Console::Printf_Error("SHADER COMPILE ERROR [%s] in %s:\n%s\n", typeStr, pathHint ? pathHint : "Unknown Path", infoLog);
    }
    return shader;
}

GLuint createShaderProgram(const Char* vertPath, const Char* fragPath) {
    Char* vertSrc = load_shader_source(vertPath);
    Char* fragSrc = load_shader_source(fragPath);

    if (!vertSrc || !fragSrc) {
        delete[] vertSrc;
        delete[] fragSrc;
        return 0;
    }

    GLuint vert = compileShader(GL_VERTEX_SHADER, vertSrc, vertPath);
    GLuint frag = compileShader(GL_FRAGMENT_SHADER, fragSrc, fragPath);

    delete[] vertSrc;
    delete[] fragSrc;

    GLuint program = glCreateProgram();
    glAttachShader(program, vert);
    glAttachShader(program, frag);
    glLinkProgram(program);

    GLint success;
    glGetProgramiv(program, GL_LINK_STATUS, &success);
    if (!success) {
        GLchar infoLog[1024];
        glGetProgramInfoLog(program, 1024, nullptr, infoLog);
        Console::Printf_Error("SHADER LINK ERROR (VERTEX + FRAGMENT):\n%s\n", infoLog);
    }

    glDeleteShader(vert);
    glDeleteShader(frag);

    return program;
}

GLuint createShaderProgram(const Char* vertPath, const Char* geomPath, const Char* fragPath) {
    Char* vertSrc = load_shader_source(vertPath);
    Char* geomSrc = load_shader_source(geomPath);
    Char* fragSrc = load_shader_source(fragPath);

    if (!vertSrc || !geomSrc || !fragSrc) {
        delete[] vertSrc;
        delete[] geomSrc;
        delete[] fragSrc;
        return 0;
    }

    GLuint vert = compileShader(GL_VERTEX_SHADER, vertSrc, vertPath);
    GLuint geom = compileShader(GL_GEOMETRY_SHADER, geomSrc, geomPath);
    GLuint frag = compileShader(GL_FRAGMENT_SHADER, fragSrc, fragPath);

    delete[] vertSrc;
    delete[] geomSrc;
    delete[] fragSrc;

    GLuint program = glCreateProgram();
    glAttachShader(program, vert);
    glAttachShader(program, geom);
    glAttachShader(program, frag);
    glLinkProgram(program);

    GLint success;
    glGetProgramiv(program, GL_LINK_STATUS, &success);
    if (!success) {
        GLchar infoLog[1024];
        glGetProgramInfoLog(program, 1024, nullptr, infoLog);
        Console::Printf_Error("SHADER LINK ERROR (VERTEX + GEOMETRY + FRAGMENT):\n%s\n", infoLog);
    }

    glDeleteShader(vert);
    glDeleteShader(geom);
    glDeleteShader(frag);

    return program;
}

GLuint createShaderProgram(const Char* vertPath, const Char* tcsPath, const Char* tesPath, const Char* fragPath) {
    Char* vertSrc = load_shader_source(vertPath);
    Char* tcsSrc = load_shader_source(tcsPath);
    Char* tesSrc = load_shader_source(tesPath);
    Char* fragSrc = load_shader_source(fragPath);

    if (!vertSrc || !tcsSrc || !tesSrc || !fragSrc) {
        delete[] vertSrc;
        delete[] tcsSrc;
        delete[] tesSrc;
        delete[] fragSrc;
        return 0;
    }

    GLuint vert = compileShader(GL_VERTEX_SHADER, vertSrc, vertPath);
    GLuint tcs = compileShader(GL_TESS_CONTROL_SHADER, tcsSrc, tcsPath);
    GLuint tes = compileShader(GL_TESS_EVALUATION_SHADER, tesSrc, tesPath);
    GLuint frag = compileShader(GL_FRAGMENT_SHADER, fragSrc, fragPath);

    delete[] vertSrc;
    delete[] tcsSrc;
    delete[] tesSrc;
    delete[] fragSrc;

    GLuint program = glCreateProgram();
    glAttachShader(program, vert);
    glAttachShader(program, tcs);
    glAttachShader(program, tes);
    glAttachShader(program, frag);
    glLinkProgram(program);

    GLint success;
    glGetProgramiv(program, GL_LINK_STATUS, &success);
    if (!success) {
        GLchar infoLog[1024];
        glGetProgramInfoLog(program, 1024, nullptr, infoLog);
        Console::Printf_Error("SHADER LINK ERROR (VERTEX + TESS + FRAGMENT):\n%s\n", infoLog);
    }

    glDeleteShader(vert);
    glDeleteShader(tcs);
    glDeleteShader(tes);
    glDeleteShader(frag);

    return program;
}

GLuint createShaderProgram(const Char* computePath) {
    Char* computeSrc = load_shader_source(computePath);

    if (!computeSrc) {
        delete[] computeSrc;
        return 0;
    }

    GLuint compute = compileShader(GL_COMPUTE_SHADER, computeSrc, computePath);
    delete[] computeSrc;

    GLuint program = glCreateProgram();
    glAttachShader(program, compute);
    glLinkProgram(program);

    GLint success;
    glGetProgramiv(program, GL_LINK_STATUS, &success);
    if (!success) {
        GLchar infoLog[1024];
        glGetProgramInfoLog(program, 1024, nullptr, infoLog);
        Console::Printf_Error("SHADER LINK ERROR (COMPUTE):\n%s\n", infoLog);
    }

    glDeleteShader(compute);
    return program;
}

static unordered_map<GLuint, unordered_map<string_view, GLint>> g_uniform_cache;

GLint Shader_GetUniformLocation(GLuint program, const char* name) {
    auto& cache = g_uniform_cache[program];
    string_view key(name);

    auto it = cache.find(key);
    if (it != cache.end())
        return it->second;

    GLint loc = glGetUniformLocation(program, name);
    cache[key] = loc;
    return loc;
}

void Shader_Set(GLuint program, const Char* name, Int value) {
    glUniform1i(Shader_GetUniformLocation(program, name), value);
}

void Shader_Set(GLuint program, const Char* name, Float value) {
    glUniform1f(Shader_GetUniformLocation(program, name), value);
}

void Shader_Set(GLuint program, const Char* name, Vec2 value) {
    glUniform2f(Shader_GetUniformLocation(program, name), value.x, value.y);
}

void Shader_Set(GLuint program, const Char* name, Vec3 value) {
    glUniform3fv(Shader_GetUniformLocation(program, name), 1, &value.x);
}

void Shader_Set(GLuint program, const Char* name, Vec4 value) {
    glUniform4fv(Shader_GetUniformLocation(program, name), 1, &value.x);
}

void Shader_Set(GLuint program, const Char* name, const Mat4* value) {
    glUniformMatrix4fv(Shader_GetUniformLocation(program, name), 1, GL_FALSE, value->m);
}

void Shader_Set(GLuint program, const Char* name, uint64_t handle) {
    glUniformHandleui64ARB(Shader_GetUniformLocation(program, name), handle);
}