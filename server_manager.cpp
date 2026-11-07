#include <iostream>
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include <sys/wait.h>

// 1. The Server Structure
struct BackendServer {
    int port;

    void run() {
        int server_fd = socket(AF_INET, SOCK_STREAM, 0);
        if (server_fd == -1) {
            std::cout << "Socket creation failed for port " << port << "\n";
            return;
        }

        // Prevent "Address already in use" errors during quick restarts
        int opt = 1;
        setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

        sockaddr_in address;
        address.sin_family = AF_INET;
        address.sin_addr.s_addr = INADDR_ANY;
        address.sin_port = htons(port);

        if (bind(server_fd, (struct sockaddr*)&address, sizeof(address)) < 0) {
            std::cout << "Bind failed on port " << port << "\n";
            return;
        }

        listen(server_fd, 3);
        std::cout << "[Server] Listening on port " << port << "...\n";

        // Keep this specific process alive to handle sequential requests
        while (true) {
            int client_socket = accept(server_fd, nullptr, nullptr);
            if (client_socket < 0) continue;

            char buffer[1024] = {0};
            read(client_socket, buffer, 1024);
            std::cout << "[Port " << port << "] Received: " << buffer << "\n";

            // Format a response manually using standard C-style arrays
            char response[256];
            int len = snprintf(response, sizeof(response), "Hello from backend server on port %d!", port);
            send(client_socket, response, len, 0);

            close(client_socket);
        }
        close(server_fd);
    }
};

// 2. The Server Manager
struct ServerManager {
    int ports[3];

    // Constructor to assign ports
    ServerManager(int p1, int p2, int p3) {
        ports[0] = p1;
        ports[1] = p2;
        ports[2] = p3;
    }

    void start_all() {
        std::cout << "Starting 3 backend servers using OS processes...\n";
        
        for (int i = 0; i < 3; i++) {
            pid_t pid = fork(); // Clone the process here
            
            if (pid == 0) {
                // We are inside the CHILD process
                BackendServer server;
                server.port = ports[i];
                server.run();
                exit(0); // If run() finishes, kill this child process
            } 
            else if (pid < 0) {
                std::cout << "Fork failed!\n";
            }
            // The PARENT process ignores the 'if' block and continues the loop
        }
    }

    void wait_all() {
        // The parent process waits here so the program doesn't exit,
        // catching any child processes if they happen to close.
        for (int i = 0; i < 3; i++) {
            wait(nullptr);
        }
    }
};

int main() {
    // Instantiate the manager with our 3 ports
    ServerManager manager(8081, 8082, 8083);
    
    // Fork the processes and start listening
    manager.start_all();
    
    // Prevent the main program from closing
    manager.wait_all();

    return 0;
}