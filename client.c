#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>

void error(const char *msg) {
    perror(msg);
    exit(1);
}

int main(int argc, char *argv[]) {
    if(argc < 3) {
        fprintf(stderr, "Usage %s hostname port\n", argv[0]);
        exit(1);
    }

    int sockfd, portno;
    struct sockaddr_in serv_addr;
    struct hostent *server;

    portno = atoi(argv[2]);
    sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if(sockfd < 0) error("Error opening socket.");

    server = gethostbyname(argv[1]);
    if(server == NULL) {
        fprintf(stderr, "Error, no such host\n");
        exit(0);
    }

    bzero((char *) &serv_addr, sizeof(serv_addr));
    serv_addr.sin_family = AF_INET;
    bcopy((char *)server->h_addr_list[0], (char *)&serv_addr.sin_addr.s_addr, server->h_length);
    serv_addr.sin_port = htons(portno);

    if(connect(sockfd, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0) 
        error("Connection Failed.");

    char command[256];
    while(1) {
        printf("Enter command: ");
        bzero(command, 256);
        fgets(command, 255, stdin);
        command[strcspn(command, "\n")] = 0; // Remove newline character

        // Exit command
        if (strcmp(command, "exit") == 0) {
            printf("Exiting...\n");
            break;
        }
        
        // Valid command: "dirlist -a"
        else if (strcmp(command, "dirlist -a") == 0) {
            // If valid, proceed with sending it to the server
            write(sockfd, command, strlen(command));
        }
        
        // Valid command: "quitc"
        else if (strcmp(command, "quitc") == 0) {
            // If "quitc", send it to the server and then break the loop
            write(sockfd, command, strlen(command));
            break;
        }

        // Any other command is invalid
        else {
            printf("Invalid command. Please enter 'dirlist -a', 'quitc', or 'exit'.\n");
            continue; // Ask for the command again
        }

        // Read server's response (only for "dirlist -a")
        if (strcmp(command, "dirlist -a") == 0) {
            char buffer[1024];
            int endReceived = 0;
            while(!endReceived) {
                bzero(buffer, 1024);
                int n = read(sockfd, buffer, 1023);
                if(n <= 0) {
                    // If read error or nothing more to read, break out of the loop
                    break;
                }
                buffer[n] = '\0'; // Ensure null-termination

                // Check for end of message
                if(strstr(buffer, "END") != NULL) {
                    endReceived = 1;
                    *strstr(buffer, "END") = '\0'; // Trim "END" from output
                }
                printf("%s", buffer);
            }
        }
    }

    close(sockfd);
    return 0;
}
