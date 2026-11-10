
#include <iostream>
#include <string>
#include <cstring>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

using namespace std;

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

int main()
{
    // Step 1: Create socket
    int clientSocket = socket(
        AF_INET, SOCK_STREAM, 0
    );

    if (clientSocket < 0)
    {
        perror("Socket creation failed");
        return 1;
    }

    // Step 2: Configure load balancer address
    sockaddr_in serverAddress{};

    serverAddress.sin_family = AF_INET;
    serverAddress.sin_port = htons(8080);

    inet_pton(
        AF_INET,
        "127.0.0.1",
        &serverAddress.sin_addr
    );

    // Step 3: Connect to load balancer
    if (connect(
        clientSocket,
        (sockaddr*)&serverAddress,
        sizeof(serverAddress)
    ) < 0)
    {
        perror("Connection failed");
        close(clientSocket);
        return 1;
    }

    cout << "Connected to Load Balancer!\n";

    // Step 4: Enter request
    string request;

    cout << "Enter your request: ";
    getline(cin, request);

    // Step 5: Send request
    if (!sendAll(clientSocket, request))
    {
        cout << "Request sending failed\n";
        close(clientSocket);
        return 1;
    }

    // Tell the receiver that no more request data is coming.
    shutdown(clientSocket, SHUT_WR);

    // Step 6: Receive response
    char buffer[4096];

    cout << "\nLoad Balancer Response:\n";

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

        cout.write(buffer, received);
    }

    cout << endl;

    close(clientSocket);

    return 0;
}
