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
#include "ipc_system.h"
#include <stdio.h>
#include <stdbool.h>
#include <string.h>
#include <stdlib.h>

#ifdef PLATFORM_WINDOWS
#include <winsock2.h>
#include <ws2tcpip.h>
#pragma comment(lib, "ws2_32.lib")
typedef SOCKET socket_t;
#else
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
typedef int socket_t;
#define INVALID_SOCKET -1
#define SOCKET_ERROR -1
#endif

#define TCONSOLE_PORT 28016
#define TCONSOLE_BUFFER_SIZE 4096

static socket_t g_tconsole_socket = INVALID_SOCKET;
static bool g_tconsole_connected = false;
static char g_receive_buffer[TCONSOLE_BUFFER_SIZE];
static int g_receive_buffer_len = 0;

void IPC_Init(void) {
#ifdef PLATFORM_WINDOWS
    WSADATA wsaData;
    if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
        return;
    }
#endif

    g_tconsole_socket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (g_tconsole_socket == INVALID_SOCKET) return;

    struct sockaddr_in server_addr;
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(TCONSOLE_PORT);
    server_addr.sin_addr.s_addr = inet_addr("127.0.0.1");

    if (connect(g_tconsole_socket, (struct sockaddr*)&server_addr, sizeof(server_addr)) == SOCKET_ERROR) {
#ifdef PLATFORM_WINDOWS
        closesocket(g_tconsole_socket);
#else
        close(g_tconsole_socket);
#endif
        g_tconsole_socket = INVALID_SOCKET;
        return;
    }

    char response[8];
    int bytes_received = recv(g_tconsole_socket, response, sizeof(response) - 1, 0);
    if (bytes_received > 0) {
        response[bytes_received] = '\0';
        if (strcmp(response, "ok") == 0) {
            g_tconsole_connected = true;
#ifdef PLATFORM_WINDOWS
            u_long mode = 1;
            ioctlsocket(g_tconsole_socket, FIONBIO, &mode);
#else
            fcntl(g_tconsole_socket, F_SETFL, O_NONBLOCK);
#endif
        }
    }
    
    if (!g_tconsole_connected) {
        IPC_Shutdown();
    }
}

void IPC_Shutdown(void) {
    if (g_tconsole_socket != INVALID_SOCKET) {
#ifdef PLATFORM_WINDOWS
        closesocket(g_tconsole_socket);
        WSACleanup();
#else
        close(g_tconsole_socket);
#endif
        g_tconsole_socket = INVALID_SOCKET;
    }
    g_tconsole_connected = false;
}

void IPC_SendMessage(const char* message) {
    if (!g_tconsole_connected) return;

    char buffer[TCONSOLE_BUFFER_SIZE];
    snprintf(buffer, sizeof(buffer), "%s\n", message);
    if (send(g_tconsole_socket, buffer, strlen(buffer), 0) == SOCKET_ERROR) {
        g_tconsole_connected = false;
    }
}

void IPC_ReceiveCommands(command_func_t command_handler) {
    if (!g_tconsole_connected || !command_handler) return;

    int bytes_received = recv(g_tconsole_socket, g_receive_buffer + g_receive_buffer_len, TCONSOLE_BUFFER_SIZE - g_receive_buffer_len - 1, 0);

    if (bytes_received > 0) {
        g_receive_buffer_len += bytes_received;
        g_receive_buffer[g_receive_buffer_len] = '\0';

        char* line_start = g_receive_buffer;
        char* newline;
        while ((newline = strchr(line_start, '\n')) != NULL) {
            *newline = '\0';
            
            char* cmd_copy = _strdup(line_start);
#define MAX_ARGS 16
            int argc = 0;
            char* argv[MAX_ARGS];
            
            char* p = strtok(cmd_copy, " ");
            while(p != NULL && argc < MAX_ARGS) {
                argv[argc++] = p;
                p = strtok(NULL, " ");
            }
            if (argc > 0) {
                command_handler(argc, argv);
            }
            free(cmd_copy);

            line_start = newline + 1;
        }

        if (line_start < g_receive_buffer + g_receive_buffer_len) {
            memmove(g_receive_buffer, line_start, strlen(line_start) + 1);
            g_receive_buffer_len = strlen(line_start);
        } else {
            g_receive_buffer_len = 0;
        }
    } else if (bytes_received == 0 || (bytes_received == SOCKET_ERROR
#ifdef PLATFORM_WINDOWS
        && WSAGetLastError() != WSAEWOULDBLOCK
#else
        && errno != EWOULDBLOCK && errno != EAGAIN
#endif
    )) {
        g_tconsole_connected = false;
    }
}