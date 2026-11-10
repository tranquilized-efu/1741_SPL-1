#include <iostream>
#include <string>
#include <cstring>
#include <unistd.h>
#include <arpa/inet.h>

int main(int argc, char* argv[]) {
    std::string message = "Hello Load Balancer!";
    if (argc > 1) message = argv[1];

    int sock = socket(AF_INET, SOCK_STREAM, 0);
    sockaddr_in serv_addr{};
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(8080);
    inet_pton(AF_INET, "127.0.0.1", &serv_addr.sin_addr);

    if (connect(sock, (struct sockaddr*)&serv_addr, sizeof(serv_addr)) < 0) {
        std::cerr << "Connection failed.\n";
        return 1;
    }

    send(sock, message.c_str(), message.length(), 0);
    char buffer[1024] = {0};
    read(sock, buffer, 1024);
    std::cout << "[Client Received] " << buffer << "\n";

    close(sock);
    return 0;
}
