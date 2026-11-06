#include <iostream>
#include <cstring>
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>

int main() {
    // 1. Create TCP socket (IPv4, TCP stream)
    int server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (server_fd == -1) {
        std::cerr << "Socket creation failed\n";
        return 1;
    }

    // 2. Define server address and port
    sockaddr_in address;
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY; // Listen on all interfaces
    address.sin_port = htons(8080);       // Port 8080

    // 3. Bind socket to the address
    if (bind(server_fd, (struct sockaddr*)&address, sizeof(address)) < 0) {
        std::cerr << "Bind failed\n";
        return 1;
    }

    // 4. Listen for incoming connections (queue size 3)
    listen(server_fd, 3);
    std::cout << "Server listening on port 8080...\n";

    // 5. Accept an incoming client connection
    int client_socket = accept(server_fd, nullptr, nullptr);
    if (client_socket < 0) {
        std::cerr << "Accept failed\n";
        return 1;
    }

    // 6. Read message from client
    char buffer[1024] = {0};
    read(client_socket, buffer, 1024);
    std::cout << "Received from client: " << buffer << "\n";

    // 7. Send response to client
    std::string response = "Message received loud and clear!";
    send(client_socket, response.c_str(), response.length(), 0);

    // 8. Clean up
    close(client_socket);
    close(server_fd);
    return 0;
}
