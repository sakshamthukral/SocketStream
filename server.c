#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <signal.h>
#include <arpa/inet.h> // Include for inet_addr

int total_connections = 0;
char outputBuff[4096];

void error(const char *msg) {
    perror(msg);
    exit(1);
}

int determinServer(){
    // if (total_connections < 3){
    //     return 1;
    // } else if (total_connections >= 3 && total_connections < 6){
    //     return 2;
    // } else if (total_connections >= 6 && total_connections < 9){
    //     return 3;
    // } else {
    //     return (total_connections % 3) + 1;
    
    // }
    return (total_connections % 3) + 1;
}

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

void crequest(int newsockfd, int sockfd_mirror1, int sockfd_mirror2) {
    if(determinServer() == 2){
        while(1){
            // char buffer[1024];
            memset(outputBuff, 0, sizeof(outputBuff));
            int n = read(newsockfd, outputBuff, 4096); // Read the command from the client
            if (n <= 0){
                return;
            }
            // printf("Received command in server: %s\n", outputBuff);
            write(sockfd_mirror1, outputBuff, strlen(outputBuff));
            memset(outputBuff, 0, sizeof(outputBuff));
            n = read(sockfd_mirror1, outputBuff, sizeof(outputBuff)-1);
            write(newsockfd, outputBuff, n);
            if (n < 0) {
                error("ERROR reading from socket");
            }
        }
    } else if(determinServer() == 3){
        while(1){
            // char buffer[1024];
            memset(outputBuff, 0, sizeof(outputBuff));
            int n = read(newsockfd, outputBuff, 4096); // Read the command from the client
            if (n <= 0){
                return;
            }
            // printf("Received command in server: %s\n", outputBuff);
            write(sockfd_mirror2, outputBuff, strlen(outputBuff));
            memset(outputBuff, 0, sizeof(outputBuff));
            n = read(sockfd_mirror2, outputBuff, sizeof(outputBuff)-1);
            write(newsockfd, outputBuff, n);
            if (n < 0) {
                error("ERROR reading from socket");
            }
        }
    } else {
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
}

int main(int argc, char *argv[]) {
    if(argc < 2) {
        fprintf(stderr, "Port number not provided. Program terminated\n");
        exit(1);
    }

    socklen_t clilen;
    int sockfd, newsockfd, portno;
    int sockfd_mirror1, sockfd_mirror2;
    struct sockaddr_in serv_addr, cli_addr, mirror1_addr, mirror2_addr;

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
    
    // Connection to mirror1
    sockfd_mirror1 = socket(AF_INET, SOCK_STREAM, 0);
    if(sockfd_mirror1 < 0) error("ERROR opening socket for mirror1");
    mirror1_addr.sin_family = AF_INET;
    mirror1_addr.sin_addr.s_addr = inet_addr("127.0.0.1");
    mirror1_addr.sin_port = htons(8080);
    if(connect(sockfd_mirror1, (struct sockaddr *) &mirror1_addr, sizeof(mirror1_addr)) < 0)
        error("Connection failed to mirror1");

    // Connection to mirror2
    sockfd_mirror2 = socket(AF_INET, SOCK_STREAM, 0);
    if(sockfd_mirror2 < 0) error("ERROR opening socket for mirror2");
    mirror2_addr.sin_family = AF_INET;
    mirror2_addr.sin_addr.s_addr = inet_addr("127.0.0.1");
    mirror2_addr.sin_port = htons(8081);
    if(connect(sockfd_mirror2, (struct sockaddr *) &mirror2_addr, sizeof(mirror2_addr)) < 0)
        error("Connection failed to mirror2");

    while(1) {
        newsockfd = accept(sockfd, (struct sockaddr *) &cli_addr, &clilen);
        if(newsockfd < 0) error("Error on accept.");

        int pid = fork();
        if(pid < 0) {
            error("Error on fork.");
        }

        if(pid == 0) { // Child process
            close(sockfd);
            crequest(newsockfd, sockfd_mirror1, sockfd_mirror2);
            exit(0);
        }
        else {
            total_connections++;
            close(newsockfd);
            signal(SIGCHLD, SIG_IGN);
        }
    }

    close(sockfd);
    return 0;
}
