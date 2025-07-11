/*
 * Copyright © 2025 Soft Sprint Studios
 * All rights reserved.
 *
 * This file is proprietary and confidential. Unauthorized reproduction,
 * modification, or distribution is strictly prohibited unless explicit
 * written permission is granted by Soft Sprint Studios.
 */
#include "gl_misc.h"
#include "gl_console.h"
#include <stdlib.h>

char* load_shader_source(const char* path) {
    char* buffer = NULL;
    long length;
    FILE* f = fopen(path, "rb");
    if (f) {
        fseek(f, 0, SEEK_END);
        length = ftell(f);
        fseek(f, 0, SEEK_SET);
        buffer = malloc(length + 1);
        if (buffer) {
            fread(buffer, 1, length, f);
            buffer[length] = '\0';
        }
        fclose(f);
    }
    else {
        Console_Printf("Error: Could not open shader file %s\n", path);
    }
    return buffer;
}

GLuint compileShader(GLenum type, const char* src) {
    GLuint shader = glCreateShader(type);
    glShaderSource(shader, 1, &src, NULL);
    glCompileShader(shader);
    GLint success;
    glGetShaderiv(shader, GL_COMPILE_STATUS, &success);
    if (!success) {
        GLchar infoLog[1024];
        glGetShaderInfoLog(shader, 1024, NULL, infoLog);
        Console_Printf("SHADER COMPILE ERROR: %s\n", infoLog);
    }
    return shader;
}

GLuint createShaderProgram(const char* vertPath, const char* fragPath) {
    char* vertSrc = load_shader_source(vertPath);
    char* fragSrc = load_shader_source(fragPath);
    if (!vertSrc || !fragSrc) {
        free(vertSrc);
        free(fragSrc);
        return 0;
    }
    GLuint vert = compileShader(GL_VERTEX_SHADER, vertSrc);
    GLuint frag = compileShader(GL_FRAGMENT_SHADER, fragSrc);
    free(vertSrc);
    free(fragSrc);
    GLuint program = glCreateProgram();
    glAttachShader(program, vert);
    glAttachShader(program, frag);
    glLinkProgram(program);
    GLint success;
    glGetProgramiv(program, GL_LINK_STATUS, &success);
    if (!success) {
        GLchar infoLog[512];
        glGetProgramInfoLog(program, 512, NULL, infoLog);
        Console_Printf("SHADER LINK ERROR: %s\n", infoLog);
    }
    glDeleteShader(vert);
    glDeleteShader(frag);
    return program;
}

GLuint createShaderProgramGeom(const char* vertPath, const char* geomPath, const char* fragPath) {
    char* vertSrc = load_shader_source(vertPath);
    char* geomSrc = load_shader_source(geomPath);
    char* fragSrc = load_shader_source(fragPath);
    if (!vertSrc || !geomSrc || !fragSrc) {
        free(vertSrc);
        free(geomSrc);
        free(fragSrc);
        return 0;
    }
    GLuint vert = compileShader(GL_VERTEX_SHADER, vertSrc);
    GLuint geom = compileShader(GL_GEOMETRY_SHADER, geomSrc);
    GLuint frag = compileShader(GL_FRAGMENT_SHADER, fragSrc);
    free(vertSrc);
    free(geomSrc);
    free(fragSrc);

    GLuint program = glCreateProgram();
    glAttachShader(program, vert);
    glAttachShader(program, geom);
    glAttachShader(program, frag);
    glLinkProgram(program);

    GLint success;
    glGetProgramiv(program, GL_LINK_STATUS, &success);
    if (!success) {
        GLchar infoLog[1024];
        glGetProgramInfoLog(program, 1024, NULL, infoLog);
        Console_Printf("SHADER LINK ERROR: %s\n", infoLog);
    }
    glDeleteShader(vert);
    glDeleteShader(geom);
    glDeleteShader(frag);
    return program;
}

GLuint createShaderProgramTess(const char* vertPath, const char* tcsPath, const char* tesPath, const char* fragPath) {
    char* vertSrc = load_shader_source(vertPath);
    char* tcsSrc = load_shader_source(tcsPath);
    char* tesSrc = load_shader_source(tesPath);
    char* fragSrc = load_shader_source(fragPath);
    if (!vertSrc || !tcsSrc || !tesSrc || !fragSrc) {
        free(vertSrc); 
        free(tcsSrc); 
        free(tesSrc); 
        free(fragSrc);
        return 0;
    }
    GLuint vert = compileShader(GL_VERTEX_SHADER, vertSrc);
    GLuint tcs = compileShader(GL_TESS_CONTROL_SHADER, tcsSrc);
    GLuint tes = compileShader(GL_TESS_EVALUATION_SHADER, tesSrc);
    GLuint frag = compileShader(GL_FRAGMENT_SHADER, fragSrc);
    free(vertSrc); 
    free(tcsSrc); 
    free(tesSrc); 
    free(fragSrc);

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
        glGetProgramInfoLog(program, 1024, NULL, infoLog);
        Console_Printf("SHADER LINK ERROR: %s\n", infoLog);
    }
    glDeleteShader(vert);
    glDeleteShader(tcs);
    glDeleteShader(tes);
    glDeleteShader(frag);
    return program;
}

GLuint createShaderProgramCompute(const char* computePath) {
    char* computeSrc = load_shader_source(computePath);
    if (!computeSrc) {
        free(computeSrc);
        return 0;
    }

    GLuint compute = compileShader(GL_COMPUTE_SHADER, computeSrc);
    free(computeSrc);

    GLuint program = glCreateProgram();
    glAttachShader(program, compute);
    glLinkProgram(program);

    GLint success;
    glGetProgramiv(program, GL_LINK_STATUS, &success);
    if (!success) {
        GLchar infoLog[1024];
        glGetProgramInfoLog(program, 1024, NULL, infoLog);
        Console_Printf("SHADER LINK ERROR: %s\n", infoLog);
    }

    glDeleteShader(compute);
    return program;
}