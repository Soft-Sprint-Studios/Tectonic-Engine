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
#include <FL/Fl.H>
#include <FL/Fl_Double_Window.H>
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
typedef int socket_t;
#define INVALID_SOCKET -1
#define SOCKET_ERROR -1
#define closesocket close
#endif

#define TCONSOLE_PORT 28016
#define BUFFER_SIZE 4096

Fl_Double_Window* window = nullptr;
Fl_Text_Display* text_display = nullptr;
Fl_Text_Buffer* text_buffer = nullptr;
Fl_Text_Buffer* style_buffer = nullptr;
Fl_Input* input_field = nullptr;
Fl_Button* send_button = nullptr;
Fl_Menu_Bar* menu_bar = nullptr;
Fl_Box* status_bar = nullptr;

Fl_Text_Display::Style_Table_Entry style_table[] = {
    { FL_WHITE, FL_COURIER, 14 }, // A - Normal
    { FL_YELLOW, FL_COURIER, 14 }, // B - Warning
    { FL_RED, FL_COURIER, 14 }, // C - Error
    { FL_CYAN, FL_COURIER_BOLD, 14}, // D - User Input
};

socket_t server_socket = INVALID_SOCKET;
socket_t client_socket = INVALID_SOCKET;
vector<string> message_queue;
mutex queue_mutex;
thread server_thread;
bool should_exit = false;

void append_message(const string& msg, char style_char = 'A') {
    if (msg.rfind("[ERROR]", 0) == 0) {
        style_char = 'C';
    }
    else if (msg.rfind("[WARNING]", 0) == 0) {
        style_char = 'B';
    }
    else if (msg.rfind("[TConsole]", 0) == 0) {
        style_char = 'D';
    }
    else if (msg.rfind("> ", 0) == 0) {
        style_char = 'D';
    }

    text_buffer->append(msg.c_str());
    text_buffer->append("\n");

    string style_line(msg.length(), style_char);
    style_buffer->append(style_line.c_str());
    style_buffer->append("\n");

    text_display->scroll(text_display->count_lines(0, text_buffer->length(), 1), 0);
}

void idle_callback(void*) {
    vector<string> local_queue;
    {
        lock_guard<mutex> lock(queue_mutex);
        if (!message_queue.empty()) {
            local_queue.swap(message_queue);
        }
    }

    for (const auto& msg : local_queue) {
        append_message(msg);
    }

    static bool last_connected_state = false;
    if (last_connected_state != (client_socket != INVALID_SOCKET)) {
        last_connected_state = (client_socket != INVALID_SOCKET);
        if (last_connected_state) {
            status_bar->label("Engine Connected.");
        }
        else {
            status_bar->label("Waiting for engine connection...");
        }
    }

    Fl::repeat_timeout(0.05, idle_callback);
}

void send_command_callback(Fl_Widget*, void*) {
    const char* command = input_field->value();
    if (strlen(command) > 0 && client_socket != INVALID_SOCKET) {
        string full_command = string(command) + "\n";
        send(client_socket, full_command.c_str(), full_command.length(), 0);
        append_message("> " + string(command), 'D');
        input_field->value("");
    }
    else if (strlen(command) > 0) {
        append_message("[!] Engine not connected. Command not sent: " + string(command), 'C');
    }
    input_field->take_focus();
}

void input_callback(Fl_Widget*, void*) {
    if (Fl::event_key() == FL_Enter || Fl::event_key() == FL_KP_Enter) {
        send_command_callback(nullptr, nullptr);
    }
}

void on_window_close(Fl_Widget*, void*) {
    should_exit = true;
    if (server_socket != INVALID_SOCKET) {
        closesocket(server_socket);
        server_socket = INVALID_SOCKET;
    }
    if (server_thread.joinable()) {
        server_thread.join();
    }
    window->hide();
}

void on_quit_cb(Fl_Widget* w, void* data) {
    on_window_close(w, data);
}

void on_clear_cb(Fl_Widget*, void*) {
    text_buffer->text("");
    style_buffer->text("");
}

void on_about_cb(Fl_Widget*, void*) {
    fl_message_title("About Tectonic Console");
    fl_message("A remote console for the Tectonic Engine.\n\n"
        "Copyright (c) 2025-2026 Soft Sprint Studios");
}

void server_loop() {
#ifdef PLATFORM_WINDOWS
    WSADATA wsaData;
    WSAStartup(MAKEWORD(2, 2), &wsaData);
#endif

    server_socket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (server_socket == INVALID_SOCKET) return;

    int opt = 1;
    setsockopt(server_socket, SOL_SOCKET, SO_REUSEADDR, (char*)&opt, sizeof(opt));

    sockaddr_in server_addr;
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(TCONSOLE_PORT);
    server_addr.sin_addr.s_addr = inet_addr("127.0.0.1");

    if (bind(server_socket, (sockaddr*)&server_addr, sizeof(server_addr)) == SOCKET_ERROR) {
        closesocket(server_socket);
        return;
    }

    listen(server_socket, 1);

    while (!should_exit) {
        sockaddr_in client_addr;
        socklen_t client_len = sizeof(client_addr);
        client_socket = accept(server_socket, (sockaddr*)&client_addr, &client_len);

        if (client_socket == INVALID_SOCKET) {
            if (should_exit) break;
            continue;
        }

        {
            lock_guard<mutex> lock(queue_mutex);
            message_queue.push_back("[TConsole] Engine connected.");
        }
        Fl::awake();

        send(client_socket, "ok", 2, 0);

        char buffer[BUFFER_SIZE];
        int bytes_received;
        string line_buffer;

        while ((bytes_received = recv(client_socket, buffer, BUFFER_SIZE - 1, 0)) > 0) {
            buffer[bytes_received] = '\0';
            line_buffer += buffer;

            size_t pos;
            while ((pos = line_buffer.find('\n')) != string::npos) {
                string line = line_buffer.substr(0, pos);
                if (!line.empty() && line.back() == '\r') {
                    line.pop_back();
                }

                if (!line.empty()) {
                    lock_guard<mutex> lock(queue_mutex);
                    message_queue.push_back(line);
                }
                Fl::awake();
                line_buffer.erase(0, pos + 1);
            }
        }

        {
            lock_guard<mutex> lock(queue_mutex);
            message_queue.push_back("[TConsole] Engine disconnected.");
        }
        Fl::awake();
        closesocket(client_socket);
        client_socket = INVALID_SOCKET;
    }

#ifdef PLATFORM_WINDOWS
    WSACleanup();
#endif
}

int main(int argc, char** argv) {
    window = new Fl_Double_Window(800, 600, "Tectonic Console");
    window->callback(on_window_close);

    menu_bar = new Fl_Menu_Bar(0, 0, 800, 25);
    menu_bar->add("File/Quit", FL_CTRL + 'q', on_quit_cb, window);
    menu_bar->add("Edit/Clear", FL_CTRL + 'l', on_clear_cb, window);
    menu_bar->add("Help/About", 0, on_about_cb, window);

    text_buffer = new Fl_Text_Buffer();
    style_buffer = new Fl_Text_Buffer();

    text_display = new Fl_Text_Display(10, 35, 780, 515);
    text_display->buffer(text_buffer);
    text_display->highlight_data(style_buffer, style_table, sizeof(style_table) / sizeof(style_table[0]), 'A', 0, 0);
    text_display->color(FL_BLACK);
    text_display->textcolor(FL_WHITE);
    text_display->textfont(FL_COURIER);
    text_display->textsize(14);

    input_field = new Fl_Input(10, 560, 700, 30);
    input_field->callback(input_callback);
    input_field->when(FL_WHEN_ENTER_KEY | FL_WHEN_RELEASE);

    send_button = new Fl_Button(720, 560, 70, 30, "Send");
    send_button->callback(send_command_callback);

    status_bar = new Fl_Box(0, 590, 800, 10, "Waiting for engine connection...");
    status_bar->align(FL_ALIGN_LEFT | FL_ALIGN_INSIDE | FL_ALIGN_CLIP);
    status_bar->box(FL_FLAT_BOX);

    window->resizable(text_display);
    window->end();
    window->show(argc, argv);

    Fl::add_timeout(0.05, idle_callback);

    server_thread = thread(server_loop);

    return Fl::run();
}