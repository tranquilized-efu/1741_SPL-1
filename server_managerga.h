#ifndef SERVER_MANAGER_H
#define SERVER_MANAGER_H

#include <iostream>
#include <vector>
#include <mutex>
#include <atomic>
#include <unistd.h>
#include <sys/wait.h>
#include <signal.h>

enum class ServerStatus { INACTIVE, STARTING, ACTIVE, STOPPING };

struct Server {
    int id;
    std::string ip;
    int port;
    pid_t pid;
    ServerStatus status;
    std::atomic<int> active_connections{0};

    Server(int id, std::string ip, int port, pid_t pid)
        : id(id), ip(std::move(ip)), port(port), pid(pid), status(ServerStatus::ACTIVE) {}
};

class ServerManager {
private:
    std::vector<std::shared_ptr<Server>> servers;
    std::mutex mtx;
    int next_port = 8081;
    int next_id = 1;

public:
    ServerManager() = default;

    ~ServerManager() {
        std::lock_guard<std::mutex> lock(mtx);
        for (auto& s : servers) {
            if (s->pid > 0) {
                kill(s->pid, SIGTERM);
                waitpid(s->pid, nullptr, 0);
            }
        }
    }

    std::shared_ptr<Server> addServer(const std::string& ip = "127.0.0.1") {
        std::lock_guard<std::mutex> lock(mtx);
        int port = next_port++;
        pid_t pid = fork();

        if (pid == 0) { // Child Process
            execl("./backend", "backend", std::to_string(port).c_str(), nullptr);
            exit(1);
        } else if (pid > 0) { // Parent Process
            auto s = std::make_shared<Server>(next_id++, ip, port, pid);
            servers.push_back(s);
            std::cout << "[ServerManager] Spawned dynamic backend on port " << port << " (PID " << pid << ")\n";
            return s;
        }
        return nullptr;
    }

    bool removeServer(int port) {
        std::lock_guard<std::mutex> lock(mtx);
        for (auto it = servers.begin(); it != servers.end(); ++it) {
            if ((*it)->port == port) {
                kill((*it)->pid, SIGTERM);
                waitpid((*it)->pid, nullptr, 0);
                std::cout << "[ServerManager] Terminated backend on port " << port << "\n";
                servers.erase(it);
                return true;
            }
        }
        return false;
    }

    std::vector<std::shared_ptr<Server>> getActiveServers() {
        std::lock_guard<std::mutex> lock(mtx);
        std::vector<std::shared_ptr<Server>> active;
        for (auto& s : servers) {
            if (s->status == ServerStatus::ACTIVE) active.push_back(s);
        }
        return active;
    }

    size_t getServerCount() {
        std::lock_guard<std::mutex> lock(mtx);
        return servers.size();
    }
};

#endif
