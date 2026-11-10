
#include <iostream>
#include <cstring>
#include <cstdlib>
#include <string>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

using namespace std;

// Send all data, even if send() sends only part of it.
bool sendAll(int socketFD, const string& message)
{
    size_t totalSent = 0;

    while (totalSent < message.size())
    {
        ssize_t sent = send(
            socketFD,
            message.c_str() + totalSent,
            message.size() - totalSent,
            0
        );

        if (sent <= 0)
            return false;

        totalSent += sent;
    }

    return true;
}

int main(int argc, char* argv[])
{
    if (argc != 2)
    {
        cout << "Usage: ./backend <port>\n";
        return 1;
    }

    int port = atoi(argv[1]);

    // Step 1: Create TCP socket
    int serverSocket = socket(
        AF_INET, SOCK_STREAM, 0
    );

    if (serverSocket < 0)
    {
        perror("Socket creation failed");
        return 1;
    }

    // Allow quick reuse of the port
    int option = 1;

    setsockopt(
        serverSocket,
        SOL_SOCKET,
        SO_REUSEADDR,
        &option,
        sizeof(option)
    );

    // Step 2: Configure server address
    sockaddr_in serverAddress{};

    serverAddress.sin_family = AF_INET;
    serverAddress.sin_port = htons(port);
    serverAddress.sin_addr.s_addr = INADDR_ANY;

    // Step 3: Bind socket to port
    if (bind(
        serverSocket,
        (sockaddr*)&serverAddress,
        sizeof(serverAddress)
    ) < 0)
    {
        perror("Bind failed");
        close(serverSocket);
        return 1;
    }

    // Step 4: Listen for clients
    if (listen(serverSocket, 10) < 0)
    {
        perror("Listen failed");
        close(serverSocket);
        return 1;
    }

    cout << "Backend server running on port "
         << port << endl;

    // Step 5: Accept incoming connections
    while (true)
    {
        sockaddr_in clientAddress{};
        socklen_t clientLength = sizeof(clientAddress);

        int clientSocket = accept(
            serverSocket,
            (sockaddr*)&clientAddress,
            &clientLength
        );

        if (clientSocket < 0)
        {
            perror("Accept failed");
            continue;
        }

        // Step 6: Receive client request
        char buffer[4096] = {};
        string request;

        while (true)
        {
            ssize_t received = recv(
                clientSocket,
                buffer,
                sizeof(buffer),
                0
            );

            if (received <= 0)
                break;

            request.append(buffer, received);
        }

        // Step 7: Prepare response
        string response =
            "\nResponse from Backend Server\n"
            "Server Port: " + to_string(port) +
            "\nYour Request: " + request + "\n";

        // Step 8: Send response
        sendAll(clientSocket, response);

        cout << "Request processed on port "
             << port << endl;

        close(clientSocket);
    }

    close(serverSocket);

    return 0;
}
