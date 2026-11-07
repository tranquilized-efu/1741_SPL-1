
#include <iostream>
#include <cstdio>
#include <cstdlib>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>

using namespace std;

// Information about each backend server
struct BackendConfig
{
    int port;
    int serverID;
    int delay;
};

// Server Manager
class ServerManager
{
private:

    BackendConfig servers[3];
    pid_t processIDs[3];

    // Launch one backend server
    void startServer(int index)
    {
        pid_t pid = fork();

        if (pid < 0)
        {
            cout << "Fork failed for Server "
                 << servers[index].serverID << "\n";
            return;
        }

        if (pid == 0)
        {
            // CHILD PROCESS

            char portText[12];
            char idText[12];
            char delayText[12];

            snprintf(portText, sizeof(portText),
                     "%d", servers[index].port);

            snprintf(idText, sizeof(idText),
                     "%d", servers[index].serverID);

            snprintf(delayText, sizeof(delayText),
                     "%d", servers[index].delay);

            cout << "Starting Backend Server "
                 << servers[index].serverID << "\n";

            // Replace child process with backend executable
            execl(
                "./backend",
                "./backend",
                portText,
                idText,
                delayText,
                (char *)nullptr
            );

            // This executes only if execl() fails
            perror("Backend execution failed");

            _exit(1);
        }
        else
        {
            // PARENT PROCESS

            processIDs[index] = pid;

            cout << "Backend Server "
                 << servers[index].serverID
                 << " started with PID "
                 << pid << "\n";
        }
    }

public:

    // Constructor
    ServerManager()
    {
        servers[0] = {8081, 1, 2};
        servers[1] = {8082, 2, 3};
        servers[2] = {8083, 3, 1};

        for (int i = 0; i < 3; i++)
        {
            processIDs[i] = -1;
        }
    }

    // Start all backend servers
    void startAll()
    {
        cout << "\nStarting backend servers...\n";

        for (int i = 0; i < 3; i++)
        {
            startServer(i);
        }
    }

    // Monitor backend processes
    void monitorServers()
    {
        cout << "\nServer Manager is monitoring backends...\n";

        while (true)
        {
            int status;

            // Wait for any child process to terminate
            pid_t finishedPID = waitpid(-1, &status, 0);

            if (finishedPID < 0)
            {
                perror("waitpid failed");
                break;
            }

            // Find which backend terminated
            for (int i = 0; i < 3; i++)
            {
                if (processIDs[i] == finishedPID)
                {
                    cout << "\nBackend Server "
                         << servers[i].serverID
                         << " stopped unexpectedly.\n";

                    if (WIFEXITED(status))
                    {
                        cout << "Exit code: "
                             << WEXITSTATUS(status) << "\n";
                    }
                    else if (WIFSIGNALED(status))
                    {
                        cout << "Terminated by signal: "
                             << WTERMSIG(status) << "\n";
                    }

                    // Restart the failed backend
                    cout << "Restarting Backend Server "
                         << servers[i].serverID << "...\n";

                    startServer(i);

                    break;
                }
            }
        }
    }
};

int main()
{
    ServerManager manager;

    // Start three backend processes
    manager.startAll();

    // Monitor their execution
    manager.monitorServers();

    return 0;
}
