#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <signal.h>

#define SERVER_PORT 8000

void error(const char *msg) {
    perror(msg);
    exit(1);
}

int mirror1Connections = 0;
int mirror2Connections = 0;
int serverw24Connections = 0;
int totalConnections = 0; // Keep track of total connections to distribute them

// Function to determine which server instance should handle the connection
int determineTargetServer() {
    // Update the logic to correctly distribute connections as specified
    if (totalConnections < 3) {
        return 1; // Serverw24
    } else if (totalConnections >= 3 && totalConnections < 6) {
        return 2; // Mirror1
    } else if (totalConnections >= 6 && totalConnections < 9) {
        return 3; // Mirror2
    } else {
        // Alternating between serverw24, mirror1, and mirror2 for subsequent connections
        return (totalConnections % 3) + 1;
    }
}

void executeCommand(int sock, const char *cmd) {
    FILE *fp;
    char path[1035];

    fp = popen(cmd, "r");
    if (fp == NULL) {
        printf("Failed to run command\n");
        write(sock, "Failed to run command\n", 22); // Inform the client of failure
        return; // Exit the function
    }

    // Read and send the output
    while (fgets(path, sizeof(path)-1, fp) != NULL) {
        write(sock, path, strlen(path));
    }

    // Ensure to signal the client that command execution is done
    write(sock, "END\n", 4);

    pclose(fp);
}

void crequest(int newsockfd) {
    char buffer[256];
    bzero(buffer, 256);
    int n = read(newsockfd, buffer, 255);
    if (n <= 0) return;

    buffer[n] = '\0';

    printf("Received command: %s\n", buffer);

    // Determine target server
    int targetServer = determineTargetServer();

    if (targetServer == 2) {
        // For mirror1
        int pid = fork();
        if (pid == 0) { // Child process
            execlp("./client", "./client", "127.0.0.1", "8080", buffer, NULL);
            exit(0);
        }
    } else if (targetServer == 3) {
        // For mirror2
        int pid = fork();
        if (pid == 0) { // Child process
            execlp("./client", "./client", "127.0.0.1", "8081", buffer, NULL);
            exit(0);
        }
    } else {
        printf("Serverw24 connection\n");
        // Serverw24 handles the command directly
        if (strncmp("dirlist -a", buffer, 10) == 0) {
            executeCommand(newsockfd, "ls -l");
        } else if (strncmp("quitc", buffer, 5) == 0) {
            printf("Quit command received. Exiting child process.\n");
            exit(0);
        } else {
            char* invalidCommand = "Invalid command\n";
            write(newsockfd, invalidCommand, strlen(invalidCommand));
        }

        char* endOfMessage = "END\n";
        write(newsockfd, endOfMessage, strlen(endOfMessage));
    }
}

int main() {
    int sockfd, newsockfd;
    socklen_t clilen;
    struct sockaddr_in serv_addr, cli_addr;

    sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if(sockfd < 0) error("Error opening socket.");

    bzero((char *) &serv_addr, sizeof(serv_addr));
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_addr.s_addr = INADDR_ANY;
    serv_addr.sin_port = htons(SERVER_PORT);

    if(bind(sockfd, (struct sockaddr *) &serv_addr, sizeof(serv_addr)) < 0) 
        error("Binding failed.");

    listen(sockfd, 5);
    clilen = sizeof(cli_addr);

    while(1) {
        newsockfd = accept(sockfd, (struct sockaddr *) &cli_addr, &clilen);
        if(newsockfd < 0) error("Error on accept.");

        totalConnections++; // Increment total connections for distribution logic
        int pid = fork();
        if(pid == 0) { // Child process
            close(sockfd);
            crequest(newsockfd);
            exit(0);
        } else {
            close(newsockfd);
            signal(SIGCHLD, SIG_IGN);

            // Update connection counts based on the target server determined after incrementing total connections
            int targetServer = determineTargetServer() - 1; // Adjust for 0 indexing
            if (targetServer == 0) serverw24Connections++;
            else if (targetServer == 1) mirror1Connections++;
            else if (targetServer == 2) mirror2Connections++;
        }
    }

    close(sockfd);
    return 0;
}
