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
#include <FL/Fl.H>
#include <FL/Fl_Window.H>
#include <FL/Fl_Text_Display.H>
#include <FL/Fl_Text_Buffer.H>
#include <FL/Fl_Input.H>
#include <FL/Fl_Button.H>
#include <FL/Fl_Menu_Bar.H>
#include <FL/fl_ask.H>
#include <FL/Fl_Box.H>
#include <string>
#include <vector>
#include <thread>
#include <mutex>
#include <atomic>

#ifdef _WIN32
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
typedef int socket_t;
#define INVALID_SOCKET -1
#define SOCKET_ERROR -1
#define closesocket close
#endif

#define TCONSOLE_PORT 28016
#define TCONSOLE_BUFFER_SIZE 4096

struct AppData {
    Fl_Text_Display* logDisplay;
    Fl_Text_Buffer* logBuffer;
    Fl_Box* statusBar;
    std::vector<std::string> messages;
    std::mutex mtx;
    std::atomic<bool> is_connected{ false };
    std::atomic<bool> should_update_status{ true };
    socket_t client_socket = INVALID_SOCKET;
};

void server_thread_func(AppData* app_data) {
#ifdef _WIN32
    WSADATA wsaData;
    WSAStartup(MAKEWORD(2, 2), &wsaData);
#endif

    socket_t listen_socket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (listen_socket == INVALID_SOCKET) return;

    sockaddr_in server_addr;
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = inet_addr("127.0.0.1");
    server_addr.sin_port = htons(TCONSOLE_PORT);

    int opt = 1;
    setsockopt(listen_socket, SOL_SOCKET, SO_REUSEADDR, (const char*)&opt, sizeof(int));

    if (bind(listen_socket, (sockaddr*)&server_addr, sizeof(server_addr)) == SOCKET_ERROR) {
        closesocket(listen_socket);
        return;
    }

    if (listen(listen_socket, 1) == SOCKET_ERROR) {
        closesocket(listen_socket);
        return;
    }

    while (true) {
        app_data->client_socket = accept(listen_socket, NULL, NULL);
        if (app_data->client_socket != INVALID_SOCKET) {
            app_data->is_connected = true;
            app_data->should_update_status = true;
            Fl::awake();
            send(app_data->client_socket, "ok", 2, 0);

            char buffer[TCONSOLE_BUFFER_SIZE];
            int bytes_received;
            std::string partial_line;

            while ((bytes_received = recv(app_data->client_socket, buffer, sizeof(buffer) - 1, 0)) > 0) {
                buffer[bytes_received] = '\0';
                partial_line += buffer;

                size_t pos = 0;
                std::string token;
                while ((pos = partial_line.find('\n')) != std::string::npos) {
                    token = partial_line.substr(0, pos);
                    {
                        std::lock_guard<std::mutex> lock(app_data->mtx);
                        app_data->messages.push_back(token);
                    }
                    Fl::awake();
                    partial_line.erase(0, pos + 1);
                }
            }

            app_data->is_connected = false;
            app_data->should_update_status = true;
            Fl::awake();
            closesocket(app_data->client_socket);
            app_data->client_socket = INVALID_SOCKET;
        }
    }
    closesocket(listen_socket);
#ifdef _WIN32
    WSACleanup();
#endif
}

class TConsoleWindow : public Fl_Window {
public:
    Fl_Input* commandInput;
    Fl_Button* sendBtn;
    Fl_Menu_Bar* menuBar;
    AppData app_data;

    TConsoleWindow(int W, int H, const char* L = 0) : Fl_Window(W, H, L) {
        begin();
        menuBar = new Fl_Menu_Bar(0, 0, W, 25);
        menuBar->add("File/Quit", FL_CTRL + 'q', on_quit_cb, this);
        menuBar->add("Edit/Clear", FL_CTRL + 'l', on_clear_cb, this);
        menuBar->add("Help/About", 0, on_about_cb, this);

        app_data.logBuffer = new Fl_Text_Buffer();
        app_data.logDisplay = new Fl_Text_Display(0, 25, W, H - 75, "");
        app_data.logDisplay->buffer(app_data.logBuffer);

        commandInput = new Fl_Input(0, H - 50, W - 80, 25, "");
        sendBtn = new Fl_Button(W - 80, H - 50, 80, 25, "Send");

        app_data.statusBar = new Fl_Box(0, H - 25, W, 25, "Waiting for engine connection...");
        app_data.statusBar->align(FL_ALIGN_LEFT | FL_ALIGN_INSIDE | FL_ALIGN_CLIP);
        app_data.statusBar->box(FL_UP_BOX);
        end();
        resizable(this);

        commandInput->callback(on_command_cb, this);
        sendBtn->callback(on_send_btn_cb, this);
        Fl::add_idle(on_idle_cb, this);
    }

private:
    static void on_command_cb(Fl_Widget* w, void* data) {
        TConsoleWindow* win = (TConsoleWindow*)data;
        if (Fl::event_key() == FL_Enter || Fl::event_key() == FL_KP_Enter) {
            on_send_btn_cb(w, data);
        }
    }

    static void on_send_btn_cb(Fl_Widget* w, void* data) {
        TConsoleWindow* win = (TConsoleWindow*)data;
        if (win->app_data.is_connected) {
            const char* command = win->commandInput->value();
            if (command && strlen(command) > 0) {
                std::string cmd_with_newline = std::string(command) + "\n";
                send(win->app_data.client_socket, cmd_with_newline.c_str(), cmd_with_newline.length(), 0);

                std::string log_entry = "> " + std::string(command) + "\n";
                win->app_data.logBuffer->append(log_entry.c_str());
                win->app_data.logDisplay->scroll(win->app_data.logBuffer->length(), 0);

                win->commandInput->value("");
            }
        }
    }

    static void on_quit_cb(Fl_Widget*, void* data) {
        TConsoleWindow* win = (TConsoleWindow*)data;
        win->hide();
    }

    static void on_clear_cb(Fl_Widget*, void* data) {
        TConsoleWindow* win = (TConsoleWindow*)data;
        win->app_data.logBuffer->text("");
    }

    static void on_about_cb(Fl_Widget*, void*) {
        fl_message_title("About Tectonic Console");
        fl_message("A remote console for the Tectonic Engine.\n\n"
            "Copyright (c) 2025 Soft Sprint Studios");
    }

    static void on_idle_cb(void* data) {
        TConsoleWindow* win = (TConsoleWindow*)data;
        win->app_data.mtx.lock();
        if (!win->app_data.messages.empty()) {
            for (const auto& msg : win->app_data.messages) {
                win->app_data.logBuffer->append((msg + "\n").c_str());
            }
            win->app_data.messages.clear();
            win->app_data.logDisplay->scroll(win->app_data.logBuffer->length(), 0);
        }
        win->app_data.mtx.unlock();

        if (win->app_data.should_update_status) {
            if (win->app_data.is_connected) {
                win->app_data.statusBar->label("Engine Connected.");
            }
            else {
                win->app_data.statusBar->label("Waiting for engine connection...");
            }
            win->app_data.should_update_status = false;
        }
    }
};

int main(int argc, char** argv) {
    TConsoleWindow* window = new TConsoleWindow(800, 600, "Tectonic Console");
    window->end();
    window->show(argc, argv);
    std::thread server(server_thread_func, &window->app_data);
    server.detach();
    return Fl::run();
}