
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h> // defines the structure hostent, hostent is used to store information about a host like IP address, host name, etc.

void error(const char *msg)
{
    perror(msg);
    exit(1);
}

int main(int argc, char *argv[]){ // two paragmeters, filename and port number
    if(argc < 3){
        fprintf(stderr, "Usage %s hostname port\n", argv[0]);
        exit(1);
    }

    int sockfd, portno,n;
    struct sockaddr_in serv_addr;
    struct hostent *server; // pointer to hostent structure

    char buffer[255]; // buffer to store message
    
    portno = atoi(argv[2]); // convert port number from string to integer
    sockfd = socket(AF_INET, SOCK_STREAM, 0); // create socket, AF_INET is domain, SOCK_STREAM is type of socket, 0 is protocol
    if(sockfd < 0){
        error("Error opening socket.");
    }

    server = gethostbyname(argv[1]); // get the host name from the command line argument
    if(server == NULL){
        fprintf(stderr, "Error, no such host.");
    }

    bzero((char *) &serv_addr, sizeof(serv_addr)); // clear the server address
    serv_addr.sin_family = AF_INET; // set domain
    bcopy((char *)server->h_addr_list[0], (char *)&serv_addr.sin_addr.s_addr, server->h_length); // copy the server address to the server structure
    serv_addr.sin_port = htons(portno); // set port number of server, htonl and htons are used to convert host byte order to network byte order, htonl is for long and htons is for short
    if(connect(sockfd, (struct sockaddr *) &serv_addr, sizeof(serv_addr)) < 0){ // connect to the server
        error("Connection failed.");
    }
    //--------------------------------------------------------------------------------
    int num1, num2, ans, choice;
    S:bzero(buffer, 255); // clear the buffer
    n = read(sockfd, buffer, 255); // read the message from server
    if(n < 0){
        error("Error reading from socket.");
    }
    printf("Server - %s\n", buffer); // print the message from server
    scanf("%d", &num1); // get the number 1 from user
    write(sockfd, &num1, sizeof(int)); // send the number 1 to server

    bzero(buffer, 255); // clear the buffer
    n = read(sockfd, buffer, 255); // read the message from server
    if(n < 0){
        error("Error reading from socket.");
    }
    printf("Server - %s\n", buffer); // print the message from server
    scanf("%d", &num2); // get the number 2 from user
    write(sockfd, &num2, sizeof(int)); // send the number 2 to server

    bzero(buffer, 255); // clear the buffer
    n = read(sockfd, buffer, 255); // read the message from server
    if(n < 0){
        error("Error reading from socket.");
    }
    printf("Server - %s\n", buffer); // print the message from server
    scanf("%d", &choice); // get the choice from user
    write(sockfd, &choice, sizeof(int)); // send the choice to server

    if(choice == 5){
        goto Q;
    }
    read(sockfd, &ans, sizeof(int)); // read the answer from server
    printf("Server - The answer is: %d\n", ans); // print the answer from server
    if(choice != 5){
        goto S;
    }
    Q:
    printf("You chose to exit\n");
    close(sockfd);
    return 0;
}