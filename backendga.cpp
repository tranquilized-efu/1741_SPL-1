#include <iostream>
#include <string>
#include <cstring>
#include <unistd.h>
#include <arpa/inet.h>

int main(int argc, char* argv[]) {
    if (argc < 2) {
        std::cerr << "Usage: " << argv[0] << " <port>\n";
        return 1;
    }
    int port = std::stoi(argv[1]);

    int server_fd = socket(AF_INET, SOCK_STREAM, 0);
    int opt = 1;
    setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    sockaddr_in address{};
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons(port);

    bind(server_fd, (struct sockaddr*)&address, sizeof(address));
    listen(server_fd, 10);
    std::cout << "[Backend :" << port << " PID " << getpid() << "] Server online.\n";

    while (true) {
        sockaddr_in client_addr;
        socklen_t addrlen = sizeof(client_addr);
        int client_fd = accept(server_fd, (struct sockaddr*)&client_addr, &addrlen);
        if (client_fd < 0) continue;

        char buffer[1024] = {0};
        int valread = read(client_fd, buffer, 1024);
        if (valread > 0) {
            std::string msg(buffer, valread);
            std::string response = "Response from Backend [Port " + std::to_string(port) + 
                                   " | PID " + std::to_string(getpid()) + "]: Echoing -> " + msg;
            send(client_fd, response.c_str(), response.length(), 0);
        }
        close(client_fd);
    }
    close(server_fd);
    return 0;
}
