
#include <iostream>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <climits>
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include <signal.h>

using namespace std;

// Convert string to integer
int stringToInt(const char text[])
{
    if (text[0] == '\0')
        return -1;

    int number = 0;

    for (int i = 0; text[i] != '\0'; i++)
    {
        if (text[i] < '0' || text[i] > '9')
            return -1;

        int digit = text[i] - '0';

        if (number > (INT_MAX - digit) / 10)
            return -1;

        number = number * 10 + digit;
    }

    return number;
}

// Send all data, even if send() sends only part of it
int sendAll(int socket, const char message[], int length)
{
    int sent = 0;

    while (sent < length)
    {
        int result = send(
            socket,
            message + sent,
            length - sent,
            0
        );

        if (result <= 0)
            return -1;

        sent += result;
    }

    return sent;
}

int main(int argc, char *argv[])
{
    // Check command-line arguments
    if (argc != 4)
    {
        cout << "Usage: ./backend <port> <server_id> <delay>\n";
        return 1;
    }

    int port = stringToInt(argv[1]);
    int serverID = stringToInt(argv[2]);
    int delay = stringToInt(argv[3]);

    if (port < 1 || port > 65535 ||
        serverID < 1 || delay < 0)
    {
        cout << "Invalid arguments\n";
        return 1;
    }

    // Prevent termination if a client disconnects during send()
    signal(SIGPIPE, SIG_IGN);

    int serverSocket;

    sockaddr_in serverAddress = {};

    // STEP 1: Create TCP socket
    serverSocket = socket(AF_INET, SOCK_STREAM, 0);

    if (serverSocket < 0)
    {
        cout << "Socket creation failed\n";
        return 1;
    }

    // STEP 2: Enable address reuse
    int opt = 1;

    setsockopt(
        serverSocket,
        SOL_SOCKET,
        SO_REUSEADDR,
        &opt,
        sizeof(opt)
    );

    // STEP 3: Configure server address
    serverAddress.sin_family = AF_INET;
    serverAddress.sin_port = htons(port);
    serverAddress.sin_addr.s_addr = htonl(INADDR_ANY);

    // STEP 4: Bind socket to port
    if (bind(
            serverSocket,
            (sockaddr *)&serverAddress,
            sizeof(serverAddress)
        ) < 0)
    {
        cout << "Binding failed on port " << port << "\n";
        close(serverSocket);
        return 1;
    }

    // STEP 5: Start listening
    if (listen(serverSocket, 10) < 0)
    {
        cout << "Listen failed\n";
        close(serverSocket);
        return 1;
    }

    cout << "\nBackend Server " << serverID
         << " is running on port " << port << "\n";

    cout << "Waiting for clients...\n";

    // STEP 6: Accept clients continuously
    while (true)
    {
        int clientSocket = accept(
            serverSocket,
            nullptr,
            nullptr
        );

        if (clientSocket < 0)
        {
            cout << "Accept failed\n";
            continue;
        }

        cout << "\nClient connected to Server "
             << serverID << "\n";

        // STEP 7: Receive client request
        char request[1024];
        int totalBytes = 0;

        while (totalBytes < 1023)
        {
            char ch;

            int received = recv(
                clientSocket,
                &ch,
                1,
                0
            );

            if (received <= 0)
                break;

            if (ch == '\n')
                break;

            request[totalBytes] = ch;
            totalBytes++;
        }

        request[totalBytes] = '\0';

        cout << "Received request: " << request << "\n";

        // STEP 8: Simulate processing time
        cout << "Processing request...\n";

        sleep(delay);

        // STEP 9: Prepare response
        char response[1200];

        int responseLength = snprintf(
            response,
            sizeof(response),
            "Response from Backend Server %d\n"
            "Port: %d\n"
            "Your request was: %s\n",
            serverID,
            port,
            request
        );

        // STEP 10: Send response
        if (responseLength < 0 ||
            responseLength >= (int)sizeof(response))
        {
            cout << "Response formatting failed\n";
        }
        else if (sendAll(
                     clientSocket,
                     response,
                     responseLength
                 ) < 0)
        {
            cout << "Sending response failed\n";
        }
        else
        {
            cout << "Response sent successfully\n";
        }

        // STEP 11: Close client connection
        close(clientSocket);

        cout << "Client disconnected\n";
    }

    close(serverSocket);

    return 0;
}
