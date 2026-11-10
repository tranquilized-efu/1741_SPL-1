#include <iostream>
#include <string>
#include <vector>
#include <mutex>
#include <thread>
#include <sstream>
#include <cstdlib>
#include <cstring>
#include <algorithm>
#include <cerrno>
#include <functional>

#include <unistd.h>
#include <signal.h>
#include <sys/socket.h>
#include <sys/wait.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/select.h>
#include <fcntl.h>

using namespace std;
// 1. SERVER STRUCTURE
struct Server
{
    int port;
    pid_t processID;
    int activeConnections;
    bool active;
};
// 2. LOAD BALANCING ALGORITHMS
enum Algorithm
{
    ROUND_ROBIN,
    IP_HASH,
    LEAST_CONNECTIONS
};

// Custom IP hash function
unsigned long hashIP(const string& ip)
{
    unsigned long hash = 5381;

    for (char c : ip)
    {
        hash = ((hash << 5) + hash) + c;
    }

    return hash;
}
// 3. SERVER MANAGER
class ServerManager
{
private:

    vector<Server> servers;

    mutex serverMutex;

    int nextPort = 8081;

    int roundRobinIndex = 0;

public:
    // ADD SERVER
    bool addServer()
    {
        int port;

        {
            lock_guard<mutex> lock(serverMutex);
            port = nextPort++;
        }

        string portText = to_string(port);

        // Create a new backend process
        pid_t pid = fork();

        if (pid < 0)
        {
            perror("Fork failed");
            return false;
        }

        if (pid == 0)
        {
            // Child process executes backend program
            execl("./backend", "./backend", portText.c_str(), (char*)NULL);
            perror("Backend execution failed");
            _exit(127);
        }

        // Wait until the backend is ready
        bool ready = false;

        for (int i = 0; i < 30; i++)
        {
            int testSocket = socket(
                AF_INET, SOCK_STREAM, 0
            );

            if (testSocket < 0)
                break;

            sockaddr_in address{};

            address.sin_family = AF_INET;
            address.sin_port = htons(port);

            inet_pton(AF_INET, "127.0.0.1", &address.sin_addr);

            if (connect(testSocket, (sockaddr*)&address, sizeof(address)) == 0)
            {
                ready = true;
                close(testSocket);
                break;
            }

            close(testSocket);
            usleep(100000);
        }

        if (!ready)
        {
            cout << "Backend failed to start on port " << port << endl;

            kill(pid, SIGTERM);
            waitpid(pid, NULL, 0);

            return false;
        }

        // Register the server
        {
            lock_guard<mutex> lock(serverMutex);

            servers.push_back({port, pid, 0, true});
        }

        cout << "Server added successfully!" << " Port: " << port << " PID: " << pid << endl;

        return true;
    }

    // REMOVE SERVER

    bool removeServer(int port)
    {
        pid_t pid = -1;

        {
            lock_guard<mutex> lock(serverMutex);

            for (auto it = servers.begin();
                 it != servers.end(); ++it)
            {
                if (it->port == port && it->active)
                {
                    // Do not kill a server with
                    // requests currently in progress.
                    if (it->activeConnections > 0)
                    {
                        cout << "Server has active " << "connections. Try again later.\n";

                        return false;
                    }

                    pid = it->processID;

                    servers.erase(it);

                    break;
                }
            }
        }

        if (pid == -1)
        {
            cout << "Server not found!\n";
            return false;
        }

        kill(pid, SIGTERM);

        waitpid(pid, NULL, 0);

        cout << "Server removed from port " << port << endl;

        return true;
    }

    // GET ACTIVE SERVER COUNT

    int getServerCount()
    {
        lock_guard<mutex> lock(serverMutex);

        return servers.size();
    }

    // DISPLAY SERVER INFORMATION

    void displayServers()
    {
        lock_guard<mutex> lock(serverMutex);

        cout << "\n========== SERVER STATUS ==========\n";

        cout << "PORT\tPID\tCONNECTIONS\tSTATUS\n";

        for (const auto& server : servers)
        {
            cout << server.port << "\t" << server.processID << "\t" << server.activeConnections << "\t\t" << (server.active ? "ACTIVE" : "INACTIVE") << endl;
        }

        cout << "Total servers: " << servers.size() << endl;

        cout << "===================================\n";
    }

    // SELECT SERVER USING AN ALGORITHM

    bool selectServer(
        Algorithm algorithm,
        const string& clientIP,
        Server& selected
    )
    {
        lock_guard<mutex> lock(serverMutex);

        vector<int> available;

        for (int i = 0; i < (int)servers.size(); i++)
        {
            if (servers[i].active)    available.push_back(i);
        }

        if (available.empty()) return false;

        int selectedIndex = available[0];

        // ROUND ROBIN
        if (algorithm == ROUND_ROBIN)
        {
            int position =
                roundRobinIndex % available.size();

            selectedIndex = available[position];

            roundRobinIndex++;
        }

        // IP HASH
        else if (algorithm == IP_HASH)
        {
            unsigned long hash = hashIP(clientIP);

            int position = hash % available.size();

            selectedIndex = available[position];
        }

        // LEAST CONNECTIONS
        else if (algorithm == LEAST_CONNECTIONS)
        {
            for (int index : available)
            {
                if (servers[index].activeConnections <
                    servers[selectedIndex].activeConnections)
                {
                    selectedIndex = index;
                }
            }
        }

        // Increase active connection count
        servers[selectedIndex].activeConnections++;

        selected = servers[selectedIndex];

        return true;
    }

    // DECREASE ACTIVE CONNECTION COUNT

    void releaseConnection(pid_t pid)
    {
        lock_guard<mutex> lock(serverMutex);

        for (auto& server : servers)
        {
            if (server.processID == pid)
            {
                if (server.activeConnections > 0)
                    server.activeConnections--;

                return;
            }
        }
    }

    // STOP ALL BACKEND SERVERS

    void stopAll()
    {
        vector<pid_t> processes;

        {
            lock_guard<mutex> lock(serverMutex);

            for (const auto& server : servers)
            {
                processes.push_back(server.processID);
            }

            servers.clear();
        }

        for (pid_t pid : processes)
        {
            kill(pid, SIGTERM);
            waitpid(pid, NULL, 0);
        }

        cout << "All backend servers stopped.\n";
    }
};
// 4. SEND ALL DATA
bool sendAll(int socketFD, const string& message)
{
    size_t totalSent = 0;

    while (totalSent < message.size())
    {
        ssize_t sent = send(socketFD, message.c_str() + totalSent, message.size() - totalSent, 0);

        if (sent <= 0) return false;

        totalSent += sent;
    }

    return true;
}
// 5. CONNECT TO BACKEND
int connectBackend(int port)
{
    int backendSocket = socket(AF_INET, SOCK_STREAM, 0);

    if (backendSocket < 0) return -1;

    // Prevent this descriptor from leaking
    // into dynamically created backend processes.
    fcntl(backendSocket, F_SETFD, FD_CLOEXEC);

    sockaddr_in address{};

    address.sin_family = AF_INET;
    address.sin_port = htons(port);

    inet_pton(AF_INET, "127.0.0.1", &address.sin_addr);

    if (connect(backendSocket, (sockaddr*)&address, sizeof(address)) < 0){
        close(backendSocket);
        return -1;
    }

    return backendSocket;
}

// 6. HANDLE CLIENT REQUEST

void handleClient(
    int clientSocket,
    string clientIP,
    ServerManager& manager,
    Algorithm algorithm
)
{
    char buffer[4096];

    string request;

    // Receive complete client request
    while (true)
    {
        ssize_t received = recv( clientSocket, buffer, sizeof(buffer), 0);

        if (received <= 0)
            break;

        request.append(buffer, received);
    }

    if (request.empty())
    {
        close(clientSocket);
        return;
    }

    // Select backend server
    Server selected;

    if (!manager.selectServer(algorithm, clientIP, selected))
    {
        sendAll(clientSocket, "ERROR: No active backend servers.\n");

        close(clientSocket);
        return;
    }

    cout << "\nClient: " << clientIP << " -> Backend: " << selected.port << endl;

    // Connect to selected backend
    int backendSocket = connectBackend(selected.port);

    if (backendSocket < 0)
    {
        sendAll( clientSocket, "ERROR: Backend connection failed.\n");

        manager.releaseConnection(selected.processID);

        close(clientSocket);
        return;
    }

    // Forward request to backend
    bool success = sendAll(backendSocket, request);

    // Tell backend that request transmission is complete
    shutdown(backendSocket, SHUT_WR);

    // Receive backend response
    string response;

    while (true)
    {
        ssize_t received = recv(backendSocket, buffer, sizeof(buffer), 0);

        if (received <= 0)
            break;

        response.append(buffer, received);
    }

    // Forward response to original client
    if (success && !response.empty())
    {
        sendAll(clientSocket, response);
    }
    else
    {
        sendAll(clientSocket, "ERROR: Backend did not respond correctly.\n");
    }

    close(backendSocket);
    close(clientSocket);

    // Release active connection
    manager.releaseConnection(selected.processID);
}
// 7. MAIN LOAD BALANCER
int main(int argc, char* argv[])
{
    if (argc != 2)
    {
        cout << "Usage:\n";
        cout << "./loadbalancer roundrobin\n";
        cout << "./loadbalancer iphash\n";
        cout << "./loadbalancer leastconnections\n";

        return 1;
    }

    // Ignore broken pipe signals
    signal(SIGPIPE, SIG_IGN);

    // Select algorithm
    string choice = argv[1];

    Algorithm algorithm;

    if (choice == "roundrobin")
        algorithm = ROUND_ROBIN;

    else if (choice == "iphash")
        algorithm = IP_HASH;

    else if (choice == "leastconnections")
        algorithm = LEAST_CONNECTIONS;

    else
    {
        cout << "Invalid algorithm!\n";
        return 1;
    }

    cout << "Selected Algorithm: "
         << choice << endl;

    // Create Server Manager
    ServerManager manager;

    // Start three initial backend servers
    cout << "\nStarting backend servers...\n";

    for (int i = 0; i < 3; i++)
    {
        if (!manager.addServer())
        {
            manager.stopAll();
            return 1;
        }
    }

    // Create load balancer socket
    int serverSocket = socket(
        AF_INET, SOCK_STREAM, 0
    );

    if (serverSocket < 0)
    {
        perror("Socket creation failed");
        manager.stopAll();
        return 1;
    }

    fcntl(serverSocket, F_SETFD, FD_CLOEXEC);

    int option = 1;

    setsockopt(serverSocket, SOL_SOCKET, SO_REUSEADDR, &option, sizeof(option));

    sockaddr_in address{};

    address.sin_family = AF_INET;
    address.sin_port = htons(8080);
    address.sin_addr.s_addr = INADDR_ANY;

    // Bind load balancer to port 8080
    if (bind(serverSocket, (sockaddr*)&address, sizeof(address)) < 0)
    {
        perror("Load balancer bind failed");

        close(serverSocket);
        manager.stopAll();

        return 1;
    }

    // Listen for clients
    if (listen(serverSocket, 20) < 0)
    {
        perror("Listen failed");

        close(serverSocket);
        manager.stopAll();

        return 1;
    }

    cout << "\nLoad Balancer running on port 8080\n";

    cout << "\nADMIN COMMANDS:\n";
    cout << "add              - Add backend server\n";
    cout << "remove <port>    - Remove backend server\n";
    cout << "list             - Display servers\n";
    cout << "quit             - Stop load balancer\n";

    // Store client threads
    vector<thread> clientThreads;

    bool running = true;

    while (running)
    {
        fd_set readfds;

        FD_ZERO(&readfds);

        FD_SET(serverSocket, &readfds);
        FD_SET(STDIN_FILENO, &readfds);

        int maxFD = max(serverSocket, STDIN_FILENO);

        // Wait for client connections or admin commands
        int activity = select(maxFD + 1, &readfds, NULL, NULL, NULL
        );

        if (activity < 0)
        {
            if (errno == EINTR)
                continue;

            perror("Select failed");
            break;
        }

        // Process administrator commands
        if (FD_ISSET(STDIN_FILENO, &readfds))
        {
            string command;

            if (!getline(cin, command))
            {
                running = false;
                continue;
            }

            if (command == "add")
            {
                manager.addServer();
            }

            else if (command == "list")
            {
                manager.displayServers();
            }

            else if (command == "quit")
            {
                running = false;
            }

            else if (command.rfind("remove ", 0) == 0)
            {
                int port = atoi(
                    command.substr(7).c_str()
                );

                manager.removeServer(port);
            }

            else
            {
                cout << "Unknown command!\n";
            }
        }

        // Accept client connections
        if (running && FD_ISSET(serverSocket, &readfds))
        {
            sockaddr_in clientAddress{};

            socklen_t clientLength =
                sizeof(clientAddress);

            int clientSocket = accept(serverSocket, (sockaddr*)&clientAddress, &clientLength);

            if (clientSocket >= 0)
            {
                fcntl(clientSocket, F_SETFD, FD_CLOEXEC);

                char ip[INET_ADDRSTRLEN] = {};

                inet_ntop(AF_INET, &clientAddress.sin_addr, ip, sizeof(ip));

                // Handle every client in a separate thread
                clientThreads.emplace_back(handleClient, clientSocket, string(ip), ref(manager), algorithm);
            }
        }
    }

    cout << "\nShutting down load balancer...\n";

    close(serverSocket);

    // Wait for all client handlers to finish
    for (auto& t : clientThreads)
    {
        if (t.joinable())
            t.join();
    }

    // Terminate backend processes
    manager.stopAll();

    cout << "Load Balancer stopped.\n";

    return 0;
}
