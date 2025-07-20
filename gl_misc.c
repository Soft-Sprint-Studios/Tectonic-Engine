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
        Console_Printf_Error("Error: Could not open shader file %s\n", path);
    }
    return buffer;
}

GLuint compileShader(GLenum type, const char* src, const char* pathHint) {
    GLuint shader = glCreateShader(type);
    glShaderSource(shader, 1, &src, NULL);
    glCompileShader(shader);
    GLint success;
    glGetShaderiv(shader, GL_COMPILE_STATUS, &success);
    if (!success) {
        GLchar infoLog[1024];
        glGetShaderInfoLog(shader, 1024, NULL, infoLog);
        const char* typeStr =
            type == GL_VERTEX_SHADER ? "VERTEX" :
            type == GL_FRAGMENT_SHADER ? "FRAGMENT" :
            type == GL_GEOMETRY_SHADER ? "GEOMETRY" :
            type == GL_TESS_CONTROL_SHADER ? "TESS CONTROL" :
            type == GL_TESS_EVALUATION_SHADER ? "TESS EVALUATION" :
            type == GL_COMPUTE_SHADER ? "COMPUTE" : "UNKNOWN";
        Console_Printf_Error("SHADER COMPILE ERROR [%s] in %s:\n%s\n", typeStr, pathHint ? pathHint : "Unknown Path", infoLog);
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
    GLuint vert = compileShader(GL_VERTEX_SHADER, vertSrc, vertPath);
    GLuint frag = compileShader(GL_FRAGMENT_SHADER, fragSrc, fragPath);
    free(vertSrc);
    free(fragSrc);
    GLuint program = glCreateProgram();
    glAttachShader(program, vert);
    glAttachShader(program, frag);
    glLinkProgram(program);
    GLint success;
    glGetProgramiv(program, GL_LINK_STATUS, &success);
    if (!success) {
        GLchar infoLog[1024];
        glGetProgramInfoLog(program, 1024, NULL, infoLog);
        Console_Printf_Error("SHADER LINK ERROR (VERTEX + FRAGMENT):\n%s\n", infoLog);
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
    GLuint vert = compileShader(GL_VERTEX_SHADER, vertSrc, vertPath);
    GLuint geom = compileShader(GL_GEOMETRY_SHADER, geomSrc, geomPath);
    GLuint frag = compileShader(GL_FRAGMENT_SHADER, fragSrc, fragPath);
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
        Console_Printf_Error("SHADER LINK ERROR (VERTEX + GEOMETRY + FRAGMENT):\n%s\n", infoLog);
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
    GLuint vert = compileShader(GL_VERTEX_SHADER, vertSrc, vertPath);
    GLuint tcs = compileShader(GL_TESS_CONTROL_SHADER, tcsSrc, tcsPath);
    GLuint tes = compileShader(GL_TESS_EVALUATION_SHADER, tesSrc, tesPath);
    GLuint frag = compileShader(GL_FRAGMENT_SHADER, fragSrc, fragPath);
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
        Console_Printf_Error("SHADER LINK ERROR (VERTEX + TESS + FRAGMENT):\n%s\n", infoLog);
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
    GLuint compute = compileShader(GL_COMPUTE_SHADER, computeSrc, computePath);
    free(computeSrc);
    GLuint program = glCreateProgram();
    glAttachShader(program, compute);
    glLinkProgram(program);
    GLint success;
    glGetProgramiv(program, GL_LINK_STATUS, &success);
    if (!success) {
        GLchar infoLog[1024];
        glGetProgramInfoLog(program, 1024, NULL, infoLog);
        Console_Printf_Error("SHADER LINK ERROR (COMPUTE):\n%s\n", infoLog);
    }
    glDeleteShader(compute);
    return program;
}

void GLAPIENTRY
GL_MessageCallback(GLenum source,
    GLenum type,
    GLuint id,
    GLenum severity,
    GLsizei length,
    const GLchar* message,
    const void* userParam)
{
    if (id == 131169 || id == 131185 || id == 131218 || id == 131204) return;

    const char* type_str = "Unknown";
    switch (type) {
    case GL_DEBUG_TYPE_ERROR:               type_str = "Error"; break;
    case GL_DEBUG_TYPE_DEPRECATED_BEHAVIOR: type_str = "Deprecated Behavior"; break;
    case GL_DEBUG_TYPE_UNDEFINED_BEHAVIOR:  type_str = "Undefined Behavior"; break;
    case GL_DEBUG_TYPE_PORTABILITY:         type_str = "Portability"; break;
    case GL_DEBUG_TYPE_PERFORMANCE:         type_str = "Performance"; break;
    case GL_DEBUG_TYPE_MARKER:              type_str = "Marker"; break;
    case GL_DEBUG_TYPE_OTHER:               type_str = "Other"; break;
    }

    switch (severity)
    {
    case GL_DEBUG_SEVERITY_HIGH:
        Console_Printf_Error("[GL ERROR] type: %s, message: %s", type_str, message);
        break;
    case GL_DEBUG_SEVERITY_MEDIUM:
        Console_Printf_Error("[GL WARNING] type: %s, message: %s", type_str, message);
        break;
    case GL_DEBUG_SEVERITY_LOW:
        Console_Printf_Warning("[GL INFO] type: %s, message: %s", type_str, message);
        break;
    case GL_DEBUG_SEVERITY_NOTIFICATION:
        Console_Printf("[GL NOTIFICATION] type: %s, message: %s", type_str, message);
        break;
    }
}

void GL_InitDebugOutput(void) {
#ifndef GAME_RELEASE
    GLint flags;
    glGetIntegerv(GL_CONTEXT_FLAGS, &flags);
    if (flags & GL_CONTEXT_FLAG_DEBUG_BIT)
    {
        glEnable(GL_DEBUG_OUTPUT);
        glEnable(GL_DEBUG_OUTPUT_SYNCHRONOUS);
        glDebugMessageCallback(GL_MessageCallback, 0);
        glDebugMessageControl(GL_DONT_CARE, GL_DONT_CARE, GL_DEBUG_SEVERITY_NOTIFICATION, 0, NULL, GL_FALSE);
        Console_Printf("OpenGL Debug Callback Initialized.");
    }
    else
    {
        Console_Printf_Warning("OpenGL Debug Context not available.");
    }
#else
    Console_Printf("OpenGL Debug is disabled on release builds.");
#endif
}