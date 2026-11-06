#include <iostream>
#include <cstring>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <unistd.h>

int main() {
    // 1. Create TCP socket
    int client_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (client_fd < 0) {
        std::cerr << "Socket creation failed\n";
        return 1;
    }

    // 2. Define server address to connect to
    sockaddr_in server_address;
    server_address.sin_family = AF_INET;
    server_address.sin_port = htons(8080);

    // Convert IPv4 address from text to binary form
    if (inet_pton(AF_INET, "127.0.0.1", &server_address.sin_addr) <= 0) {
        std::cerr << "Invalid address or address not supported\n";
        return 1;
    }

    // 3. Connect to the server
    if (connect(client_fd, (struct sockaddr*)&server_address, sizeof(server_address)) < 0) {
        std::cerr << "Connection failed\n";
        return 1;
    }

    // 4. Send message to server
    std::string message = "Hello Backend, this is the Client!";
    send(client_fd, message.c_str(), message.length(), 0);
    std::cout << "Message sent to server.\n";

    // 5. Read response from server
    char buffer[1024] = {0};
    read(client_fd, buffer, 1024);
    std::cout << "Server replied: " << buffer << "\n";

    // 6. Clean up
    close(client_fd);
    return 0;
}
