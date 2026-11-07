
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>

int stringToInt(char text[])
{
    int number = 0;
    int i = 0;

    while (text[i] != '\0')
    {
        if (text[i] < '0' || text[i] > '9')
            return -1;

        number = number * 10 + (text[i] - '0');
        i++;
    }

    return number;
}

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
        number = number / 10;
        i++;
    }

    while (i > 0)
    {
        i--;
        write(1, &digits[i], 1);
    }
}

int sendAll(int socket, char message[], int length)
{
    int sent = 0;

    while (sent < length)
    {
        int result = send(socket, message + sent,
                          length - sent, 0);

        if (result <= 0)
            return -1;

        sent += result;
    }

    return sent;
}

void copyText(char destination[], int &position, const char source[])
{
    int i = 0;

    while (source[i] != '\0')
    {
        destination[position] = source[i];
        position++;
        i++;
    }
}

int main(int argc, char *argv[])
{
    // Check command-line arguments
    if (argc != 4)
    {
        write(1, "Usage: ./backend <port> <server_id> <delay>\n", 44);
        return 1;
    }

    int port = stringToInt(argv[1]);
    int serverID = stringToInt(argv[2]);
    int delay = stringToInt(argv[3]);

    if (port < 1 || port > 65535 ||
        serverID < 1 || delay < 0)
    {
        write(1, "Invalid arguments\n", 19);
        return 1;
    }

    int serverSocket;
    int clientSocket;

    struct sockaddr_in serverAddress = {};

    // Step 1: Create TCP socket
    serverSocket = socket(AF_INET, SOCK_STREAM, 0);

    if (serverSocket < 0)
    {
        write(1, "Socket creation failed\n", 23);
        return 1;
    }

    // Step 2: Configure server address
    serverAddress.sin_family = AF_INET;
    serverAddress.sin_port = htons(port);
    serverAddress.sin_addr.s_addr = htonl(INADDR_ANY);

    // Step 3: Bind socket to port
    if (bind(serverSocket,
             (struct sockaddr *)&serverAddress,
             sizeof(serverAddress)) < 0)
    {
        write(1, "Binding failed. Port may be in use.\n", 36);
        close(serverSocket);
        return 1;
    }

    // Step 4: Start listening
    if (listen(serverSocket, 10) < 0)
    {
        write(1, "Listen failed\n", 14);
        close(serverSocket);
        return 1;
    }

    write(1, "\nBackend Server ", 16);
    printNumber(serverID);
    write(1, " is running on port ", 20);
    printNumber(port);
    write(1, "\nWaiting for clients...\n", 24);

    // Step 5: Accept clients continuously
    while (1)
    {
        clientSocket = accept(serverSocket, 0, 0);

        if (clientSocket < 0)
        {
            write(1, "Accept failed\n", 14);
            continue;
        }

        write(1, "\nClient connected to Server ", 28);
        printNumber(serverID);
        write(1, "\n", 1);

        // Step 6: Receive client request
        char request[1024];
        int totalBytes = 0;

        while (totalBytes < 1023)
        {
            int received = recv(
                clientSocket,
                request + totalBytes,
                1023 - totalBytes,
                0
            );

            if (received <= 0)
                break;

            totalBytes += received;
        }

        request[totalBytes] = '\0';

        write(1, "Received request: ", 19);
        write(1, request, totalBytes);
        write(1, "\n", 1);

        // Step 7: Simulate processing time
        write(1, "Processing request...\n", 22);
        sleep(delay);

        // Step 8: Prepare response
        char response[1200];
        int position = 0;

        copyText(response, position, "Response from Backend Server ");

        // Convert server ID into characters
        char idText[12];
        int idLength = 0;
        int temp = serverID;

        do
        {
            idText[idLength] = (temp % 10) + '0';
            temp = temp / 10;
            idLength++;
        }
        while (temp > 0);

        while (idLength > 0)
        {
            idLength--;
            response[position] = idText[idLength];
            position++;
        }

        copyText(response, position, "\nYour request was: ");

        int i = 0;

        while (request[i] != '\0' && position < 1190)
        {
            response[position] = request[i];
            position++;
            i++;
        }

        response[position] = '\0';

        // Step 9: Send response to client
        if (sendAll(clientSocket, response, position) < 0)
        {
            write(1, "Sending response failed\n", 24);
        }
        else
        {
            write(1, "Response sent successfully\n", 27);
        }

        // Step 10: Close this client connection
        close(clientSocket);

        write(1, "Client disconnected\n", 21);
    }

    close(serverSocket);

    return 0;
}
