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
char outputBuff[4096];

void executeCommand(const char *cmd) {
    FILE *fp;
    char command[1050];
    // Ensure the buffer is clean
    memset(outputBuff, 0, sizeof(outputBuff));

    // Prepend "cd ~ && " to run the command in the home directory
    snprintf(command, sizeof(command), "cd ~ && %s", cmd);

    // Open the command for reading
    fp = popen(command, "r");
    if (fp == NULL) {
        printf("Failed to run command\n");
        return; // Exit the function
    }

    // Read the output into outputBuff
    fread(outputBuff, 1, sizeof(outputBuff) - 1, fp);

    // Ensure the buffer is null-terminated
    outputBuff[sizeof(outputBuff) - 1] = '\0';

    // Close the process
    pclose(fp);

    // At this point, outputBuff contains the command's output.
    // If you need to print or process the output, you can do so after calling executeCommand.
}

void crequest(int newsockfd) {
    while(1){
            memset(outputBuff, 0, sizeof(outputBuff));
            int n = read(newsockfd, outputBuff, 4096);
            if (n <= 0){
                return;
            }

            printf("Received command: %s\n", outputBuff);

            // Check if the command is "dirlist -a"
            if (strncmp("dirlist -a", outputBuff, 10) == 0) {
                executeCommand("ls");
                write(newsockfd, outputBuff, strlen(outputBuff));
            } 
            // Check if the command is "quitc" and exit the process
            else if (strncmp("quitc", outputBuff, 5) == 0) {
                printf("Quit command received. Exiting child process.\n");
                exit(0); // Explicitly exit the child process
            } 
            else {
                char* invalidCommand = "Invalid command\n";
                write(newsockfd, invalidCommand, strlen(invalidCommand));
            }

        }
}

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
        printf("Inside mirror1\n");
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