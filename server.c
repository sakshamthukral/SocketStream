#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <signal.h>

void error(const char *msg)
{
    perror(msg);
    exit(1);
}

void crequest(int newsockfd) {
    int n;
    int num1, num2, ans, choice;
    
    do {
        n = write(newsockfd, "Enter number 1: ", strlen("Enter number 1: ")); // Ask the client to enter number 1
        if(n < 0) error("Error writing to socket.");
        read(newsockfd, &num1, sizeof(int)); // read the number 1 from client
        printf("Client - Number 1 is: %d\n", num1);

        n = write(newsockfd, "Enter number 2: ", strlen("Enter number 2: "));
        if(n < 0) error("Error writing to socket.");
        read(newsockfd, &num2, sizeof(int));
        printf("Client - Number 2 is: %d\n", num2);

        n = write(newsockfd, "Enter your choice: \n1. Addition\n2. Subtraction\n3. Multiplication\n4. Division\n5. Exit\n", strlen("Enter your choice: \n1. Addition\n2. Subtraction\n3. Multiplication\n4. Division\n5. Exit\n"));
        if(n < 0) error("Error writing to socket.");
        read(newsockfd, &choice, sizeof(int));
        printf("Client - Choice is: %d\n", choice);

        switch(choice) {
            case 1:
                ans = num1 + num2;
                break;
            case 2:
                ans = num1 - num2;
                break;
            case 3:
                ans = num1 * num2;
                break;
            case 4:
                ans = num1 / num2;
                break;
            case 5:
                break;
        }

        write(newsockfd, &ans, sizeof(int)); // send answer to client
    } while(choice != 5);

    close(newsockfd); // Close client socket
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

        int pid = fork();
        if(pid < 0) {
            error("Error on fork.");
        }

        if(pid == 0) { // This is the child process
            close(sockfd); // Close the original socket in the child
            crequest(newsockfd);
            exit(0);
        }
        else {
            close(newsockfd); // Close the new socket in the parent
            signal(SIGCHLD, SIG_IGN); // Ignore the child signal
        }
    }

    close(sockfd); // Close socket
    return 0;
}
