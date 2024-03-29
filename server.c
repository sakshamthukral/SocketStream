#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <signal.h>

void error(const char *msg) {
    perror(msg);
    exit(1);
}

void executeCommand(int sock, const char *cmd) {
    FILE *fp;
    char path[1035];

    // Ensure the command is safe and will terminate. Adjust as needed.
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

    // Check if the command is "dirlist -a"
    if (strncmp("dirlist -a", buffer, 10) == 0) {
        executeCommand(newsockfd, "ls -l");
    } 
    // Check if the command is "quitc" and exit the process
    else if (strncmp("quitc", buffer, 5) == 0) {
        printf("Quit command received. Exiting child process.\n");
        exit(0); // Explicitly exit the child process
    } 
    else {
        char* invalidCommand = "Invalid command\n";
        write(newsockfd, invalidCommand, strlen(invalidCommand));
    }

    // Send end of message in all cases, except for quitc since it exits the process
    char* endOfMessage = "END\n";
    write(newsockfd, endOfMessage, strlen(endOfMessage));
}

// void crequest(int newsockfd) {
//     char buffer[256];
//     bzero(buffer, 256);
//     int n = read(newsockfd, buffer, 255); // Read command from client
//     if (n <= 0) return;

//     buffer[n] = '\0'; // Ensure null-termination

//     printf("Received command: %s\n", buffer);

//     // Execute the command directly without validation
//     executeCommand(newsockfd, "ls -l");

//     char* endOfMessage = "END\n";
//     write(newsockfd, endOfMessage, strlen(endOfMessage));
// }


int main(int argc, char *argv[]) {
    if(argc < 2) {
        fprintf(stderr, "Port number not provided. Program terminated\n");
        exit(1);
    }

    int sockfd, newsockfd, portno;
    socklen_t clilen;
    struct sockaddr_in serv_addr, cli_addr;

    sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if(sockfd < 0) error("Error opening socket.");

    bzero((char *) &serv_addr, sizeof(serv_addr));
    portno = atoi(argv[1]);

    serv_addr.sin_family = AF_INET;
    serv_addr.sin_addr.s_addr = INADDR_ANY;
    serv_addr.sin_port = htons(portno);

    if(bind(sockfd, (struct sockaddr *) &serv_addr, sizeof(serv_addr)) < 0) 
        error("Binding failed.");

    listen(sockfd, 5);
    clilen = sizeof(cli_addr);

    while(1) {
        newsockfd = accept(sockfd, (struct sockaddr *) &cli_addr, &clilen);
        if(newsockfd < 0) error("Error on accept.");

        int pid = fork();
        if(pid < 0) {
            error("Error on fork.");
        }

        if(pid == 0) { // Child process
            close(sockfd);
            crequest(newsockfd);
            exit(0);
        }
        else {
            close(newsockfd);
            signal(SIGCHLD, SIG_IGN);
        }
    }

    close(sockfd);
    return 0;
}
