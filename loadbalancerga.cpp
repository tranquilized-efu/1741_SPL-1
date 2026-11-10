#include <iostream>
#include <thread>
#include <vector>
#include <string>
#include <functional>
#include <cstring>
#include <unistd.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include "server_manager.h"

enum class Algorithm { ROUND_ROBIN, IP_HASH, LEAST_CONNECTIONS };

class LoadBalancer {
private:
    int lb_port;
    Algorithm algo;
    ServerManager manager;
    std::atomic<size_t> rr_index{0};
    int server_fd{-1};

    size_t hashIP(const std::string& ip) {
        std::hash<std::string> hasher;
        return hasher(ip);
    }

    std::shared_ptr<Server> selectServer(const std::string& client_ip) {
        auto active = manager.getActiveServers();
        if (active.empty()) return nullptr;

        if (algo == Algorithm::ROUND_ROBIN) {
            size_t idx = rr_index.fetch_add(1) % active.size();
            return active[idx];
        } else if (algo == Algorithm::IP_HASH) {
            size_t idx = hashIP(client_ip) % active.size();
            return active[idx];
        } else if (algo == Algorithm::LEAST_CONNECTIONS) {
            auto least = active[0];
            for (auto& s : active) {
                if (s->active_connections.load() < least->active_connections.load()) {
                    least = s;
                }
            }
            return least;
        }
        return active[0];
    }

    void handleClient(int client_fd, std::string client_ip) {
        auto target_server = selectServer(client_ip);
        if (!target_server) {
            std::string err = "HTTP/1.1 503 Service Unavailable\r\n\r\nNo backend available.";
            send(client_fd, err.c_str(), err.length(), 0);
            close(client_fd);
            return;
        }

        target_server->active_connections++;

        // Open socket to chosen backend
        int backend_fd = socket(AF_INET, SOCK_STREAM, 0);
        sockaddr_in b_addr{};
        b_addr.sin_family = AF_INET;
        b_addr.sin_port = htons(target_server->port);
        inet_pton(AF_INET, target_server->ip.c_str(), &b_addr.sin_addr);

        if (connect(backend_fd, (struct sockaddr*)&b_addr, sizeof(b_addr)) == 0) {
            char buffer[2048] = {0};
            int bytes_read = read(client_fd, buffer, sizeof(buffer));
            if (bytes_read > 0) {
                send(backend_fd, buffer, bytes_read, 0);
                int b_read = read(backend_fd, buffer, sizeof(buffer));
                if (b_read > 0) {
                    send(client_fd, buffer, b_read, 0);
                }
            }
        }
        close(backend_fd);
        close(client_fd);

        target_server->active_connections--;
    }

public:
    LoadBalancer(int port, Algorithm a) : lb_port(port), algo(a) {}

    void startCLI() {
        std::string cmd;
        while (true) {
            std::cout << "\n[CLI] Commands: 'add', 'remove <port>', 'list', 'exit'\n> ";
            std::cin >> cmd;
            if (cmd == "add") {
                manager.addServer();
            } else if (cmd == "remove") {
                int p;
                std::cin >> p;
                manager.removeServer(p);
            } else if (cmd == "list") {
                auto active = manager.getActiveServers();
                std::cout << "\n--- Active Servers (" << active.size() << ") ---\n";
                for (auto& s : active) {
                    std::cout << "Port: " << s->port << " | PID: " << s->pid 
                              << " | Active Conns: " << s->active_connections.load() << "\n";
                }
            } else if (cmd == "exit") {
                exit(0);
            }
        }
    }

    void run() {
        // Initial setup: Boot 3 static backends per Phase 2
        for (int i = 0; i < 3; ++i) manager.addServer();
        // Brief pause for backend initialization
        usleep(100000);

        server_fd = socket(AF_INET, SOCK_STREAM, 0);
        int opt = 1;
        setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

        sockaddr_in address{};
        address.sin_family = AF_INET;
        address.sin_addr.s_addr = INADDR_ANY;
        address.sin_port = htons(lb_port);

        bind(server_fd, (struct sockaddr*)&address, sizeof(address));
        listen(server_fd, 100);

        std::cout << "[LoadBalancer] Listening on port " << lb_port << "...\n";

        std::thread(&LoadBalancer::startCLI, this).detach();

        while (true) {
            sockaddr_in client_addr;
            socklen_t addrlen = sizeof(client_addr);
            int client_fd = accept(server_fd, (struct sockaddr*)&client_addr, &addrlen);
            if (client_fd < 0) continue;

            char client_ip[INET_ADDRSTRLEN];
            inet_ntop(AF_INET, &client_addr.sin_addr, client_ip, INET_ADDRSTRLEN);

            std::thread(&LoadBalancer::handleClient, this, client_fd, std::string(client_ip)).detach();
        }
    }
};

int main(int argc, char* argv[]) {
    Algorithm algo = Algorithm::ROUND_ROBIN;
    if (argc > 1) {
        std::string a = argv[1];
        if (a == "iphash") algo = Algorithm::IP_HASH;
        else if (a == "leastconnections") algo = Algorithm::LEAST_CONNECTIONS;
    }

    LoadBalancer lb(8080, algo);
    lb.run();
    return 0;
}
