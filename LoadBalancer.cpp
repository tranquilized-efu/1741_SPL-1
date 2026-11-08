#include <iostream>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <cstring>

using namespace std;

// The Load Balancer Class
struct LoadBalancer {
    int backend_ports[3] = {8081, 8082, 8083};
    
    // State variables for algorithms
    int rr_index = 0;              // For Round Robin
    int server_loads[3] = {0,0,0}; // For Least Connections

    int algorithm_choice = 1;

    // --- ALGORITHM 1: ROUND ROBIN ---
    // Cycles sequentially: 0, 1, 2, 0, 1, 2...
    int get_round_robin() {
        int index = rr_index;
        rr_index = (rr_index + 1) % 3; // Move to next, wrap around at 3
        return index;
    }

    // --- ALGORITHM 2: LEAST CONNECTIONS ---
    // Finds the server that has handled the fewest requests
    int get_least_connections() {
        int min_index = 0;
        for (int i = 1; i < 3; i++) {
            if (server_loads[i] < server_loads[min_index]) {
                min_index = i;
            }
        }
        return min_index;
    }

    // --- ALGORITHM 3: IP HASH ---
    // Converts the client IP address characters to a number, and divides by 3
    int get_ip_hash(const char* client_ip) {
        int hash_value = 0;
        for (int i = 0; client_ip[i] != '\0'; i++) {
            hash_value += client_ip[i];
        }
        return hash_value % 3;
    }

    // Helper to connect to the chosen backend server
    int connect_to_backend(int port) {
        int sock = socket(AF_INET, SOCK_STREAM, 0);
        sockaddr_in addr;
        addr.sin_family = AF_INET;
        addr.sin_port = htons(port);
        inet_pton(AF_INET, "127.0.0.1", &addr.sin_addr);
        
        if (connect(sock, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
            cout << "Failed to connect to backend on port " << port << "\n";
            return -1;
        }
        return sock;
    }

    // Main proxy loop
    void run() {
        int lb_fd = socket(AF_INET, SOCK_STREAM, 0);
        int opt = 1;
        setsockopt(lb_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

        sockaddr_in lb_addr;
        lb_addr.sin_family = AF_INET;
        lb_addr.sin_addr.s_addr = INADDR_ANY;
        lb_addr.sin_port = htons(8080); // Load balancer listens on 8080

        bind(lb_fd, (struct sockaddr*)&lb_addr, sizeof(lb_addr));
        listen(lb_fd, 5);
        cout << "\n[Load Balancer] Listening on port 8080...\n";

        while (true) {
            // 1. Accept client connection and get their IP address
            sockaddr_in client_addr;
            socklen_t client_len = sizeof(client_addr);
            int client_sock = accept(lb_fd, (struct sockaddr*)&client_addr, &client_len);
            
            char client_ip[INET_ADDRSTRLEN];
            inet_ntop(AF_INET, &(client_addr.sin_addr), client_ip, INET_ADDRSTRLEN);

            // 2. Select backend using chosen algorithm
            int chosen_index = 0;
            if (algorithm_choice == 1) {
                chosen_index = get_round_robin();
            } else if (algorithm_choice == 2) {
                chosen_index = get_least_connections();
            } else if (algorithm_choice == 3) {
                chosen_index = get_ip_hash(client_ip);
            }

            int chosen_port = backend_ports[chosen_index];
            server_loads[chosen_index]++; // Update load tracker

            cout << "Client IP " << client_ip << " routed to Port " << chosen_port << "\n";

            // 3. Read message from Client
            char buffer[1024] = {0};
            int bytes_read = read(client_sock, buffer, 1024);

            if (bytes_read > 0) {
                // 4. Connect to Backend and Forward Client's message
                int backend_sock = connect_to_backend(chosen_port);
                if (backend_sock != -1) {
                    send(backend_sock, buffer, bytes_read, 0);

                    // 5. Read response from Backend
                    memset(buffer, 0, 1024); // Clear buffer
                    bytes_read = read(backend_sock, buffer, 1024);

                    // 6. Forward Backend's response to Client
                    send(client_sock, buffer, bytes_read, 0);
                    close(backend_sock);
                }
            }
            close(client_sock);
        }
    }
};

// Main Controller
int main() {
    LoadBalancer lb;

    cout << "=== Load Balancer Main Controller ===\n";
    cout << "Select Routing Algorithm:\n";
    cout << "1. Round Robin\n";
    cout << "2. Least Connections\n";
    cout << "3. IP Hash\n";
    cout << "Choice (1-3): ";
    cin >> lb.algorithm_choice;

    if (lb.algorithm_choice < 1 || lb.algorithm_choice > 3) {
        cout << "Invalid choice. Defaulting to Round Robin.\n";
        lb.algorithm_choice = 1;
    }

    // Start listening and routing
    lb.run();

    return 0;
}