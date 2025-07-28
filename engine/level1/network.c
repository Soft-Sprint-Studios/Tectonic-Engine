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
#include "network.h"
#include "gl_console.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <SDL_thread.h>

#ifdef PLATFORM_LINUX
#include <sys/socket.h>
#include <netdb.h>
#include <unistd.h>
#include <sys/stat.h>
#include <time.h>
#endif

typedef struct {
    char* url;
    char* filepath;
} DownloadArgs;

typedef struct {
    char* hostname;
} PingArgs;

static void parse_url(const char* url, char* host, size_t host_len, char* path, size_t path_len) {
    const char* scheme_end = strstr(url, "://");
    if (scheme_end) {
        host = strncpy(host, scheme_end + 3, host_len - 1);
        host[host_len - 1] = '\0';
    }
    else {
        host = strncpy(host, url, host_len - 1);
        host[host_len - 1] = '\0';
    }

    char* path_start = strchr(host, '/');
    if (path_start) {
        strncpy(path, path_start, path_len - 1);
        path[path_len - 1] = '\0';
        *path_start = '\0';
    }
    else {
        strncpy(path, "/", path_len);
    }
}

#ifdef PLATFORM_WINDOWS
static int download_thread_func_win32(void* data) {
    DownloadArgs* args = (DownloadArgs*)data;
    SOCKET sock = INVALID_SOCKET;
    struct addrinfo* result = NULL, * ptr = NULL, hints;
    char host[256], path[2048];

    parse_url(args->url, host, sizeof(host), path, sizeof(path));

    ZeroMemory(&hints, sizeof(hints));
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_protocol = IPPROTO_TCP;

    if (getaddrinfo(host, "80", &hints, &result) != 0) {
        Console_Printf_Error("[Network] ERROR: getaddrinfo failed for %s", host);
        goto cleanup;
    }

    for (ptr = result; ptr != NULL; ptr = ptr->ai_next) {
        sock = socket(ptr->ai_family, ptr->ai_socktype, ptr->ai_protocol);
        if (sock == INVALID_SOCKET) continue;
        if (connect(sock, ptr->ai_addr, (int)ptr->ai_addrlen) == SOCKET_ERROR) {
            closesocket(sock);
            sock = INVALID_SOCKET;
            continue;
        }
        break;
    }
    freeaddrinfo(result);

    if (sock == INVALID_SOCKET) {
        Console_Printf_Error("[Network] ERROR: Unable to connect to server %s", host);
        goto cleanup;
    }

    char request[2048];
    sprintf(request, "GET %s HTTP/1.1\r\nHost: %s\r\nConnection: close\r\n\r\n", path, host);

    if (send(sock, request, (int)strlen(request), 0) == SOCKET_ERROR) {
        Console_Printf_Error("[Network] ERROR: send failed.");
        goto cleanup;
    }

    FILE* fp = fopen(args->filepath, "wb");
    if (!fp) {
        Console_Printf_Error("[Network] ERROR: Failed to open file for writing: %s", args->filepath);
        goto cleanup;
    }

    char buffer[4096];
    int bytes_received;
    char* body_start = NULL;

    while ((bytes_received = recv(sock, buffer, sizeof(buffer), 0)) > 0) {
        if (!body_start) {
            body_start = strstr(buffer, "\r\n\r\n");
            if (body_start) {
                body_start += 4;
                fwrite(body_start, 1, bytes_received - (body_start - buffer), fp);
            }
        }
        else {
            fwrite(buffer, 1, bytes_received, fp);
        }
    }
    fclose(fp);
    Console_Printf("[Network] Download finished: %s -> %s", args->url, args->filepath);

cleanup:
    if (sock != INVALID_SOCKET) closesocket(sock);
    free(args->url);
    free(args->filepath);
    free(args);
    return 0;
}

static int ping_thread_func_win32(void* data) {
    PingArgs* args = (PingArgs*)data;
    SOCKET sock = INVALID_SOCKET;
    struct addrinfo* result = NULL, hints;

    ZeroMemory(&hints, sizeof(hints));
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_protocol = IPPROTO_TCP;

    if (getaddrinfo(args->hostname, "80", &hints, &result) != 0) {
        Console_Printf_Error("[Network] Ping failed for %s: Cannot resolve host", args->hostname);
        goto cleanup;
    }

    sock = socket(result->ai_family, result->ai_socktype, result->ai_protocol);
    if (sock == INVALID_SOCKET) {
        Console_Printf_Error("[Network] Ping failed for %s: Cannot create socket", args->hostname);
        freeaddrinfo(result);
        goto cleanup;
    }

    LARGE_INTEGER frequency, start, end;
    QueryPerformanceFrequency(&frequency);
    QueryPerformanceCounter(&start);

    if (connect(sock, result->ai_addr, (int)result->ai_addrlen) == SOCKET_ERROR) {
        Console_Printf_Error("[Network] Ping failed for %s: Connection timed out or refused", args->hostname);
    }
    else {
        QueryPerformanceCounter(&end);
        double time_ms = (double)(end.QuadPart - start.QuadPart) * 1000.0 / frequency.QuadPart;
        Console_Printf_Error("[Network] Ping reply from %s: time=%.0f ms", args->hostname, time_ms);
    }

    freeaddrinfo(result);

cleanup:
    if (sock != INVALID_SOCKET) closesocket(sock);
    free(args->hostname);
    free(args);
    return 0;
}
#else
static int download_thread_func_posix(void* data) {
    DownloadArgs* args = (DownloadArgs*)data;
    int sock = -1;
    struct addrinfo* result = NULL, * ptr = NULL, hints;
    char host[256], path[2048];

    parse_url(args->url, host, sizeof(host), path, sizeof(path));

    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;

    if (getaddrinfo(host, "80", &hints, &result) != 0) {
        Console_Printf_Error("[Network] ERROR: getaddrinfo failed for %s", host);
        goto cleanup;
    }

    for (ptr = result; ptr != NULL; ptr = ptr->ai_next) {
        sock = socket(ptr->ai_family, ptr->ai_socktype, ptr->ai_protocol);
        if (sock == -1) continue;
        if (connect(sock, ptr->ai_addr, ptr->ai_addrlen) == -1) {
            close(sock);
            sock = -1;
            continue;
        }
        break;
    }
    freeaddrinfo(result);

    if (sock == -1) {
        Console_Printf_Error("[Network] ERROR: Unable to connect to server %s", host);
        goto cleanup;
    }

    char request[2048];
    sprintf(request, "GET %s HTTP/1.1\r\nHost: %s\r\nConnection: close\r\n\r\n", path, host);

    if (send(sock, request, strlen(request), 0) < 0) {
        Console_Printf_Error("[Network] ERROR: send failed.");
        goto cleanup;
    }

    FILE* fp = fopen(args->filepath, "wb");
    if (!fp) {
        Console_Printf_Error("[Network] ERROR: Failed to open file for writing: %s", args->filepath);
        goto cleanup;
    }

    char buffer[4096];
    int bytes_received;
    char* body_start = NULL;

    while ((bytes_received = recv(sock, buffer, sizeof(buffer), 0)) > 0) {
        if (!body_start) {
            body_start = strstr(buffer, "\r\n\r\n");
            if (body_start) {
                body_start += 4;
                fwrite(body_start, 1, bytes_received - (body_start - buffer), fp);
            }
        }
        else {
            fwrite(buffer, 1, bytes_received, fp);
        }
    }
    fclose(fp);
    Console_Printf("[Network] Download finished: %s -> %s", args->url, args->filepath);

cleanup:
    if (sock != -1) close(sock);
    free(args->url);
    free(args->filepath);
    free(args);
    return 0;
}

static int ping_thread_func_posix(void* data) {
    PingArgs* args = (PingArgs*)data;
    int sock = -1;
    struct addrinfo* result = NULL, hints;

    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;

    if (getaddrinfo(args->hostname, "80", &hints, &result) != 0) {
        Console_Printf_Error("[Network] Ping failed for %s: Cannot resolve host", args->hostname);
        goto cleanup;
    }

    sock = socket(result->ai_family, result->ai_socktype, result->ai_protocol);
    if (sock < 0) {
        Console_Printf_Error("[Network] Ping failed for %s: Cannot create socket", args->hostname);
        freeaddrinfo(result);
        goto cleanup;
    }

    struct timespec start, end;
    clock_gettime(CLOCK_MONOTONIC, &start);

    if (connect(sock, result->ai_addr, result->ai_addrlen) == -1) {
        Console_Printf_Error("[Network] Ping failed for %s: Connection timed out or refused", args->hostname);
    }
    else {
        clock_gettime(CLOCK_MONOTONIC, &end);
        double time_ms = (end.tv_sec - start.tv_sec) * 1000.0 + (end.tv_nsec - start.tv_nsec) / 1000000.0;
        Console_Printf_Error("[Network] Ping reply from %s: time=%.0f ms", args->hostname, time_ms);
    }

    freeaddrinfo(result);

cleanup:
    if (sock != -1) close(sock);
    free(args->hostname);
    free(args);
    return 0;
}
#endif

void Network_Init(void) {
#ifdef PLATFORM_WINDOWS
    WSADATA wsaData;
    if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
        fprintf(stderr, "FATAL: WSAStartup failed.\n");
        exit(EXIT_FAILURE);
    }
#endif
    Console_Printf("Network System Initialized.\n");
}

void Network_Shutdown(void) {
#ifdef PLATFORM_WINDOWS
    WSACleanup();
#endif
    Console_Printf("Network System Shutdown.\n");
}

bool Network_DownloadFile(const char* url, const char* output_filepath) {
    DownloadArgs* args = (DownloadArgs*)malloc(sizeof(DownloadArgs));
    if (!args) return false;

    args->url = _strdup(url);
    args->filepath = _strdup(output_filepath);

    if (!args->url || !args->filepath) {
        free(args->url);
        free(args->filepath);
        free(args);
        return false;
    }

#ifdef PLATFORM_WINDOWS
    SDL_Thread* thread = SDL_CreateThread(download_thread_func_win32, "DownloadThread", (void*)args);
#else
    SDL_Thread* thread = SDL_CreateThread(download_thread_func_posix, "DownloadThread", (void*)args);
#endif

    if (!thread) {
        Console_Printf_Error("[Network] ERROR: Could not create download thread.");
        free(args->url);
        free(args->filepath);
        free(args);
        return false;
    }
    SDL_DetachThread(thread);
    return true;
}

bool Network_Ping(const char* hostname) {
    PingArgs* args = (PingArgs*)malloc(sizeof(PingArgs));
    if (!args) return false;

    args->hostname = _strdup(hostname);
    if (!args->hostname) {
        free(args);
        return false;
    }

#ifdef PLATFORM_WINDOWS
    SDL_Thread* thread = SDL_CreateThread(ping_thread_func_win32, "PingThread", (void*)args);
#else
    SDL_Thread* thread = SDL_CreateThread(ping_thread_func_posix, "PingThread", (void*)args);
#endif

    if (!thread) {
        Console_Printf_Error("[Network] ERROR: Could not create ping thread.");
        free(args->hostname);
        free(args);
        return false;
    }
    SDL_DetachThread(thread);
    return true;
}