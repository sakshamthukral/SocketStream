#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <fcntl.h>
#include <sys/stat.h>

#define BUFFER_SIZE 4096

void error(const char *msg) {
    perror(msg);
    exit(1);
}

int receive_file(int sockfd, const char *filename) {
    printf("Filename: %s\n", filename);
    int file_fd, bytes_written;
    ssize_t bytes_read, total_bytes_read = 0;
    off_t file_size;
    char buffer[BUFFER_SIZE];
    char message[14];
    int bt_read = read(sockfd, message, 13);
    message[13] = '\0'; // Null-terminate the message
    if (strcmp(message, "No file found") == 0) {
        printf("%s\n",message);
        return 0;
    }else{
        printf("%s\n",message);
        
        read(sockfd, &file_size, sizeof(file_size));
        printf("File size: %ld\n", file_size);

        file_fd = open(filename, O_WRONLY | O_CREAT | O_TRUNC, 0666);
        if (file_fd < 0) {
            perror("Failed to open file for writing");
            return -1;
        }

        while (total_bytes_read < file_size) {
            bytes_read = read(sockfd, buffer, sizeof(buffer));
            if (bytes_read < 0) {
                perror("Failed to read from socket");
                close(file_fd);
                return -1;
            }
            total_bytes_read += bytes_read;
            bytes_written = write(file_fd, buffer, bytes_read);
            if (bytes_written < 0) {
                perror("Failed to write to file");
                close(file_fd);
                return -1;
            }
        }
        close(file_fd);
        printf("File received and saved successfully.\n");
    }
    return 0;
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

    char command[1024];
    char original_command[1024];
    while(1) {
        printf("Enter command: ");
        bzero(command, 1024);
        fgets(command, 1024, stdin);
        command[strcspn(command, "\n")] = '\0'; // Remove newline character

        strcpy(original_command, command);

        // Using strtok to split the command and count arguments
        char *args[10]; // Array to hold command parts; adjust size if expecting more parts
        memset(args, 0, sizeof(args)); // Initialize array to null
        int arg_count = 0;
        char *token = strtok(command, " ");
        while(token != NULL) { // Ensure not to exceed array bounds
            args[arg_count++] = token;
            token = strtok(NULL, " ");
        }

        if (strcmp(args[0], "exit") == 0) {
            printf("Exiting...\n");
            break;
        }
        else if (strcmp(args[0], "dirlist") == 0 && (strcmp(args[1], "-a") == 0 || strcmp(args[1], "-t") == 0) && arg_count == 2) {
            // If valid, proceed with sending it to the server
            write(sockfd, original_command, strlen(original_command)); 
        }
        else if (strcmp(args[0], "w24fn") == 0 && arg_count == 2) {
            // If "w24fn" and there are exactly two arguments, proceed with sending it to the server
            write(sockfd, original_command, strlen(original_command));
        } else if (strcmp(args[0], "w24fz") == 0 && arg_count == 3) {
            write(sockfd, original_command, strlen(original_command));
        }else if (strcmp(args[0], "w24ft") == 0 && arg_count > 1 && arg_count < 5) {
            write(sockfd, original_command, strlen(original_command));
        }else if (strcmp(args[0], "w24fdb") == 0 && arg_count==2) {
            write(sockfd, original_command, strlen(original_command));
        }else if (strcmp(args[0], "w24fda") == 0 && arg_count==2) {
            write(sockfd, original_command, strlen(original_command));
        }
        else if (strcmp(args[0], "quitc") == 0) {
            // If "quitc", send it to the server and then break the loop
            write(sockfd, original_command, strlen(original_command));
            break;
        }
        else {
                printf("Invalid command\nPlease enter 'dirlist -a', 'dirlist -t', 'w24fn <filename>', 'w24fz size1 size2', 'w24ft <extension list>', 'w24fdb date', 'w24fda date' or 'quitc'.\n");
                continue; // Ask for the command again
        }

        //------------------------ Read from server ------------------------
            
        if ((strcmp(args[0], "w24fz") == 0) || (strcmp(args[0], "w24ft") == 0) || (strcmp(args[0], "w24fdb") == 0) || (strcmp(args[0], "w24fda") == 0)){
            // Special handling for file receive mode
            char *filename = "temp.tar.gz";
            receive_file(sockfd, filename);
            continue;
        } else {
            // Normal read process for other commands
            char buffer[4096];
            int n = read(sockfd, buffer, sizeof(buffer)-1);
            if (n < 0) {
                error("ERROR reading from socket");
            }
            buffer[n] = '\0';
            printf("%s\n", buffer);
        }
    }

    close(sockfd);
    return 0;
}