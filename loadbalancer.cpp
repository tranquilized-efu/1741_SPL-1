
#include <sys/socket.h>
#include <sys/mman.h>
#include <sys/wait.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <signal.h>

// Number of backend servers
const int SERVER_COUNT = 3;

// Load balancer port
const int LOAD_BALANCER_PORT = 8000;

// Backend server ports
int backendPorts[SERVER_COUNT] = {9001, 9002, 9003};

// Round Robin pointer
int currentServer = 0;

// Shared active connection counters
int *activeConnections;

// Selected algorithm
int algorithm;

// Convert string to integer
int stringToInt(char text[])
{
    int number = 0;
    int i = 0;

    if (text[0] == '\0')
        return -1;

    while (text[i] != '\0')
    {
        if (text[i] < '0' || text[i] > '9')
            return -1;

        number = number * 10 + (text[i] - '0');
        i++;
    }

    return number;
}

// Print a string
void printText(const char text[])
{
    int i = 0;

    while (text[i] != '\0')
    {
        write(1, &text[i], 1);
        i++;
    }
}

// Print an integer
void printNumber(int number)
{
    char digits[12];
    int i = 0;

    if (number == 0)
    {
        write(1, "0", 1);
        return;
    }

    while (number > 0)
    {
        digits[i] = (number % 10) + '0';
        number /= 10;
        i++;
    }

    while (i > 0)
    {
        i--;
        write(1, &digits[i], 1);
    }
}

// Send all bytes
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

// Remove finished child processes
void reapChildren(int signalNumber)
{
    (void)signalNumber;

    while (waitpid(-1, 0, WNOHANG) > 0)
    {
        // Reap all finished child processes
    }
}

// Select backend using Round Robin
int roundRobin()
{
    int selected = currentServer;

    currentServer++;

    if (currentServer == SERVER_COUNT)
        currentServer = 0;

    return selected;
}

// Select backend using Least Connections
int leastConnections()
{
    int selected = currentServer;
    int minimum = activeConnections[selected];

    // Check all backend servers
    for (int i = 1; i < SERVER_COUNT; i++)
    {
        int index = (currentServer + i) % SERVER_COUNT;

        if (activeConnections[index] < minimum)
        {
            minimum = activeConnections[index];
            selected = index;
        }
    }

    // Rotate the starting point to break ties fairly
    currentServer = (selected + 1) % SERVER_COUNT;

    return selected;
}

// Select backend using IP Hash
int ipHash(unsigned int clientIP)
{
    return clientIP % SERVER_COUNT;
}

// Choose an algorithm
int selectBackend(unsigned int clientIP)
{
    if (algorithm == 1)
        return roundRobin();

    if (algorithm == 2)
        return leastConnections();

    if (algorithm == 3)
        return ipHash(clientIP);

    return -1;
}

// Connect to selected backend
int connectToBackend(int port)
{
    int backendSocket;

    struct sockaddr_in backendAddress = {};

    backendSocket = socket(AF_INET, SOCK_STREAM, 0);

    if (backendSocket < 0)
        return -1;

    backendAddress.sin_family = AF_INET;
    backendAddress.sin_port = htons(port);
    backendAddress.sin_addr.s_addr = htonl(INADDR_LOOPBACK);

    if (connect(
            backendSocket,
            (struct sockaddr *)&backendAddress,
            sizeof(backendAddress)) < 0)
    {
        close(backendSocket);
        return -1;
    }

    return backendSocket;
}

// Handle one client in a child process
void handleClient(int clientSocket, int selected)
{
    char request[4096];
    int totalBytes = 0;

    // Receive request from client
    while (totalBytes < 4095)
    {
        int received = recv(
            clientSocket,
            request + totalBytes,
            4095 - totalBytes,
            0
        );

        if (received < 0)
        {
            close(clientSocket);
            return;
        }

        if (received == 0)
            break;

        totalBytes += received;
    }

    if (totalBytes == 0 || totalBytes == 4095)
    {
        char errorMessage[] = "Invalid or oversized request";
        sendAll(clientSocket, errorMessage,
                sizeof(errorMessage) - 1);
        close(clientSocket);
        return;
    }

    request[totalBytes] = '\0';

    // Connect to selected backend
    int backendSocket = connectToBackend(
        backendPorts[selected]
    );

    if (backendSocket < 0)
    {
        char errorMessage[] =
            "Error: Backend server unavailable";

        sendAll(clientSocket, errorMessage,
                sizeof(errorMessage) - 1);

        close(clientSocket);
        return;
    }

    // Forward client request
    if (sendAll(backendSocket, request, totalBytes) < 0)
    {
        close(backendSocket);
        close(clientSocket);
        return;
    }

    // Tell backend request is complete
    shutdown(backendSocket, SHUT_WR);

    // Receive and forward backend response
    char response[1024];
    int received;

    while ((received = recv(
                backendSocket,
                response,
                sizeof(response),
                0)) > 0)
    {
        if (sendAll(clientSocket, response, received) < 0)
            break;
    }

    close(backendSocket);
    close(clientSocket);
}

int main(int argc, char *argv[])
{
    // Step 1: Read algorithm selection
    if (argc != 2)
    {
        printText("Choose an algorithm:\n");
        printText("1. Round Robin\n");
        printText("2. Least Connections\n");
        printText("3. IP Hash\n");
        printText("Usage: ./loadbalancer <1|2|3>\n");

        return 1;
    }

    algorithm = stringToInt(argv[1]);

    if (algorithm < 1 || algorithm > 3)
    {
        printText("Invalid algorithm choice\n");
        return 1;
    }

    // Step 2: Create shared memory for active connections
    activeConnections = (int *)mmap(
        0,
        sizeof(int) * SERVER_COUNT,
        PROT_READ | PROT_WRITE,
        MAP_SHARED | MAP_ANONYMOUS,
        -1,
        0
    );

    if (activeConnections == MAP_FAILED)
    {
        printText("Shared memory creation failed\n");
        return 1;
    }

    // Initialize connection counters
    for (int i = 0; i < SERVER_COUNT; i++)
        activeConnections[i] = 0;

    // Step 3: Handle finished child processes
    signal(SIGCHLD, reapChildren);

    // Step 4: Create load balancer socket
    int serverSocket = socket(AF_INET, SOCK_STREAM, 0);

    if (serverSocket < 0)
    {
        printText("Socket creation failed\n");
        munmap(activeConnections,
               sizeof(int) * SERVER_COUNT);
        return 1;
    }

    // Allow port reuse
    int option = 1;

    setsockopt(
        serverSocket,
        SOL_SOCKET,
        SO_REUSEADDR,
        &option,
        sizeof(option)
    );

    // Step 5: Configure load balancer address
    struct sockaddr_in serverAddress = {};

    serverAddress.sin_family = AF_INET;
    serverAddress.sin_port = htons(LOAD_BALANCER_PORT);
    serverAddress.sin_addr.s_addr = htonl(INADDR_ANY);

    // Step 6: Bind socket
    if (bind(
            serverSocket,
            (struct sockaddr *)&serverAddress,
            sizeof(serverAddress)) < 0)
    {
        printText("Binding failed\n");
        close(serverSocket);
        munmap(activeConnections,
               sizeof(int) * SERVER_COUNT);
        return 1;
    }

    // Step 7: Start listening
    if (listen(serverSocket, 20) < 0)
    {
        printText("Listen failed\n");
        close(serverSocket);
        munmap(activeConnections,
               sizeof(int) * SERVER_COUNT);
        return 1;
    }

    printText("Load Balancer running on port 8000\n");
    printText("Selected algorithm: ");

    if (algorithm == 1)
        printText("Round Robin\n");
    else if (algorithm == 2)
        printText("Least Connections\n");
    else
        printText("IP Hash\n");

    // Step 8: Accept clients continuously
    while (1)
    {
        struct sockaddr_in clientAddress = {};
        socklen_t clientLength = sizeof(clientAddress);

        int clientSocket = accept(
            serverSocket,
            (struct sockaddr *)&clientAddress,
            &clientLength
        );

        if (clientSocket < 0)
            continue;

        // Convert client IP to host byte order
        unsigned int clientIP =
            ntohl(clientAddress.sin_addr.s_addr);

        // Step 9: Select backend
        int selected = selectBackend(clientIP);

        // Increment active connection count atomically
        __sync_add_and_fetch(
            &activeConnections[selected], 1
        );

        printText("\nSelected Backend Server ");
        printNumber(selected + 1);
        printText(" (Port ");
        printNumber(backendPorts[selected]);
        printText(")\n");

        if (algorithm == 2)
        {
            printText("Active connections: ");

            for (int i = 0; i < SERVER_COUNT; i++)
            {
                printNumber(activeConnections[i]);
                printText(" ");
            }

            printText("\n");
        }

        // Step 10: Create child process
        int processID = fork();

        if (processID < 0)
        {
            printText("Process creation failed\n");

            __sync_sub_and_fetch(
                &activeConnections[selected], 1
            );

            close(clientSocket);
            continue;
        }

        if (processID == 0)
        {
            // Child process
            close(serverSocket);

            handleClient(clientSocket, selected);

            // Decrease active connection count
            __sync_sub_and_fetch(
                &activeConnections[selected], 1
            );

            _exit(0);
        }

        // Parent process
        close(clientSocket);
    }

    return 0;
}
