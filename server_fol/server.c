#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <signal.h>
#include <arpa/inet.h> // Include for inet_addr
#include <dirent.h>
#include <sys/stat.h>
#include <time.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include <fcntl.h>

#define MAX_PATH_LENGTH 4096
#define MAX_DIRECTORIES 100
#define MAX_COMMAND_LEN 1024
#define OUTPUT_BUFF_SIZE 4096

int tarFlag = 0;
int total_connections = 0;
char outputBuff[OUTPUT_BUFF_SIZE];
char command[1024];
struct Directory {
    char name[MAX_PATH_LENGTH];
    time_t birth_time;
};

// Function declarations
void error(const char *msg);
void list_directories(const char *path);
void executeCommand(const char *cmd);
void crequest(int newsockfd, int sockfd_mirror1, int sockfd_mirror2);
int determinServer();
int compare_dir_names(const void *a, const void *b);
void list_directories_by_birth_time(const char *home_dir);
void directory_birth_time(const char *dir_name, struct Directory *dir);
int compare_directories(const void *a, const void *b);

struct FileInfo {
    char name[MAX_PATH_LENGTH];
    char location[MAX_PATH_LENGTH];
    off_t size;
    time_t creation_time;
    mode_t permissions;
};
// Global variable to hold the target filename
char *target_filename;
// Function to execute the stat command and retrieve output
char *file_birth_time(const char *file_path) {
    char stat_cmd[MAX_COMMAND_LEN];
    snprintf(stat_cmd, MAX_COMMAND_LEN, "stat \"%s\"", file_path);

    int pipefd[2];
    if (pipe(pipefd) == -1) {
        perror("pipe");
        exit(EXIT_FAILURE);
    }

    pid_t pid = fork();
    if (pid == -1) {
        perror("fork");
        exit(EXIT_FAILURE);
    }

    if (pid == 0) { // Child process
        close(pipefd[0]); // Close unused read end

        // Redirect stdout to the pipe
        if (dup2(pipefd[1], STDOUT_FILENO) == -1) {
            perror("dup2");
            exit(EXIT_FAILURE);
        }

        // Execute stat command
        system(stat_cmd);
        exit(EXIT_SUCCESS);
    } else { // Parent process
        close(pipefd[1]); // Close unused write end

        // Read from the pipe and return the output
        char *buf = (char *)malloc(MAX_COMMAND_LEN);
        ssize_t num_read = read(pipefd[0], buf, MAX_COMMAND_LEN);
        if (num_read == -1) {
            perror("read");
            exit(EXIT_FAILURE);
        }
        close(pipefd[0]); // Close read end of pipe
        wait(NULL);
        return buf;
    }
}
// Function to parse birth time from stat output
time_t parse_birth_time(const char *stat_output) {
    char *line = strtok((char *)stat_output, "\n");
    while (line != NULL) {
        if (strstr(line, "Birth:") != NULL) {
            line += strlen("Birth: "); // Move to the actual birth time value
            struct tm tm;
            strptime(line, "%Y-%m-%d %H:%M:%S", &tm);
            time_t birth_time = mktime(&tm);
            return birth_time;
        }
        line = strtok(NULL, "\n");
    }
    return -1; // Birth time not found
}


// Function to search for a file in the directory tree rooted at the specified path
// Returns 1 if file is found, 0 otherwise
int search_file(const char *path, const char *filename, struct FileInfo *file_info) {
    // printf("Searching for file: %s\n", filename);

    DIR *dir;
    struct dirent *entry;
    struct stat statbuf;
    char full_path[MAX_PATH_LENGTH];

    if ((dir = opendir(path)) == NULL) {
        return 0; // Failed to open directory
    }

    while ((entry = readdir(dir)) != NULL) {
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) {
            continue;
        }

        snprintf(full_path, MAX_PATH_LENGTH, "%s/%s", path, entry->d_name);

        if (lstat(full_path, &statbuf) == -1) {
            continue; // Failed to get file status
        }

        if (S_ISDIR(statbuf.st_mode)) {
            if (search_file(full_path, filename, file_info)) {
                closedir(dir);
                return 1; // File found in subdirectory
            }
        } else if (S_ISREG(statbuf.st_mode)) {
            if (strcmp(entry->d_name, filename) == 0) {
                // File found
                strncpy(file_info->name, entry->d_name, MAX_PATH_LENGTH);
                strncpy(file_info->location, full_path, MAX_PATH_LENGTH);
                file_info->size = statbuf.st_size;
                //file_info->creation_time = statbuf.st_ctime;
                 // Try to get birth time
                char *stat_output = file_birth_time(full_path);
                if (stat_output != NULL) {
                    file_info->creation_time = parse_birth_time(stat_output);
                //    file_info->creation_time = stat_output;
                        free(stat_output); // Free allocated memory
                } else {
                    printf("Displaying creation time");
                    file_info->creation_time = statbuf.st_ctime; // Fall back to change time
                }
                file_info->permissions = statbuf.st_mode & (S_IRWXU | S_IRWXG | S_IRWXO);
                closedir(dir);
                return 1;
            }
        }
    }

    closedir(dir);
    return 0; // File not found
}

void handle_search_file(const char *filename) {
    struct FileInfo file_info;
    printf("Searching for file in handle_search_file: %s\n", filename);
    const char *home_dir = getenv("HOME");
    int offset = 0; // This will keep track of the current position in outputBuff

    if (home_dir == NULL) {
        offset += snprintf(outputBuff + offset, OUTPUT_BUFF_SIZE - offset, "Failed to get user's home directory\n");
        return;
    }

    // memset(outputBuff, 0, sizeof(outputBuff));
    if (search_file(home_dir, filename, &file_info)) {
        offset += snprintf(outputBuff + offset, OUTPUT_BUFF_SIZE - offset, "File found:\n");
        offset += snprintf(outputBuff + offset, OUTPUT_BUFF_SIZE - offset, "Name: %s\n", file_info.name);
        offset += snprintf(outputBuff + offset, OUTPUT_BUFF_SIZE - offset, "Location: %s\n", file_info.location);
        offset += snprintf(outputBuff + offset, OUTPUT_BUFF_SIZE - offset, "Size: %ld bytes\n", (long)file_info.size);

        // Note: You may need to adjust the time format as per your requirements
        char timeBuf[128];
        strftime(timeBuf, sizeof(timeBuf), "%c", localtime(&file_info.creation_time));
        offset += snprintf(outputBuff + offset, OUTPUT_BUFF_SIZE - offset, "Creation Time: %s\n", timeBuf);

        offset += snprintf(outputBuff + offset, OUTPUT_BUFF_SIZE - offset, "Permissions: ");
        offset += snprintf(outputBuff + offset, OUTPUT_BUFF_SIZE - offset, (file_info.permissions & S_IRUSR) ? "r" : "-");
        offset += snprintf(outputBuff + offset, OUTPUT_BUFF_SIZE - offset, (file_info.permissions & S_IWUSR) ? "w" : "-");
        offset += snprintf(outputBuff + offset, OUTPUT_BUFF_SIZE - offset, (file_info.permissions & S_IXUSR) ? "x" : "-");
        offset += snprintf(outputBuff + offset, OUTPUT_BUFF_SIZE - offset, (file_info.permissions & S_IRGRP) ? "r" : "-");
        offset += snprintf(outputBuff + offset, OUTPUT_BUFF_SIZE - offset, (file_info.permissions & S_IWGRP) ? "w" : "-");
        offset += snprintf(outputBuff + offset, OUTPUT_BUFF_SIZE - offset, (file_info.permissions & S_IXGRP) ? "x" : "-");
        offset += snprintf(outputBuff + offset, OUTPUT_BUFF_SIZE - offset, (file_info.permissions & S_IROTH) ? "r" : "-");
        offset += snprintf(outputBuff + offset, OUTPUT_BUFF_SIZE - offset, (file_info.permissions & S_IWOTH) ? "w" : "-");
        offset += snprintf(outputBuff + offset, OUTPUT_BUFF_SIZE - offset, (file_info.permissions & S_IXOTH) ? "x\n" : "-\n");
    } else {
        offset += snprintf(outputBuff + offset, OUTPUT_BUFF_SIZE - offset, "File not found\n");
    }

    // Ensure null-termination of the outputBuff
    if (offset >= OUTPUT_BUFF_SIZE) {
        outputBuff[OUTPUT_BUFF_SIZE - 1] = '\0';
    } else {
        outputBuff[offset] = '\0';
    }
}


void error(const char *msg) {
    perror(msg);
    exit(1);
}

int compare_directories(const void *a, const void *b) {
    const struct Directory *dir1 = (const struct Directory *)a;
    const struct Directory *dir2 = (const struct Directory *)b;
    return difftime(dir1->birth_time, dir2->birth_time);
}

void directory_birth_time(const char *dir_name, struct Directory *dir) {
    int pipefd[2];
    pid_t pid;

    if (pipe(pipefd) == -1) {
        perror("pipe");
        exit(EXIT_FAILURE);
    }

    pid = fork();
    if (pid == -1) {
        perror("fork");
        exit(EXIT_FAILURE);
    }

    if (pid == 0) {
        dup2(pipefd[1], STDOUT_FILENO);
        close(pipefd[0]);
        close(pipefd[1]);

        execlp("stat", "stat", "-c", "%w", dir_name, NULL);
        perror("execlp");
        exit(EXIT_FAILURE);
    } else {
        int status;
        close(pipefd[1]);
        waitpid(pid, &status, 0);

        char buffer[1024];
        ssize_t bytes_read = read(pipefd[0], buffer, sizeof(buffer) - 1);
        if (bytes_read > 0) {
            buffer[bytes_read] = '\0'; // Null-terminate the string
            struct tm tm_time;
            if (strptime(buffer, "%Y-%m-%d %H:%M:%S", &tm_time) != NULL) {
                dir->birth_time = mktime(&tm_time);
            }
        }
        close(pipefd[0]);
    }
}


void list_directories_by_birth_time(const char *home_dir) {
    DIR *dir;
    struct dirent *ent;
    struct Directory dirs[MAX_DIRECTORIES];
    int num_dirs = 0;

    if ((dir = opendir(home_dir)) != NULL) {
        while ((ent = readdir(dir)) != NULL && num_dirs < MAX_DIRECTORIES) {
            if (ent->d_type == DT_DIR && strcmp(ent->d_name, ".") != 0 && strcmp(ent->d_name, "..") != 0) {
                char dir_path[MAX_PATH_LENGTH];
                snprintf(dir_path, sizeof(dir_path), "%s/%s", home_dir, ent->d_name);
                directory_birth_time(dir_path, &dirs[num_dirs]);
                strcpy(dirs[num_dirs].name, dir_path);
                num_dirs++;
            }
        }
        closedir(dir);
    } else {
        perror("opendir");
        exit(EXIT_FAILURE);
    }

    // Sort and print directories
    qsort(dirs, num_dirs, sizeof(struct Directory), compare_directories);
    memset(outputBuff, 0, sizeof(outputBuff));
    for (int i = 0; i < num_dirs; i++) {
        // Correctly use snprintf to append each directory's name to outputBuff
        snprintf(outputBuff + strlen(outputBuff), sizeof(outputBuff) - strlen(outputBuff), "%s - %s", dirs[i].name, ctime(&dirs[i].birth_time));
    }
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

// Function to compare directory names for sorting
int compare_dir_names(const void *a, const void *b) {
    const char *dir_name_a = *(const char **)a;
    const char *dir_name_b = *(const char **)b;

    // Skip the leading dot (.) if present
    if (dir_name_a[0] == '.') dir_name_a++;
    if (dir_name_b[0] == '.') dir_name_b++;

    return strcasecmp(dir_name_a, dir_name_b); // Use strcasecmp for case-insensitive comparison
}

void list_directories(const char *path) {
    DIR *dir;
    struct dirent *entry;
    char *directories[MAX_DIRECTORIES];
    int num_directories = 0;

    if ((dir = opendir(path)) == NULL) {
        perror("opendir");
        return;
    }

    while ((entry = readdir(dir)) != NULL) {
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) {
            continue;
        }

        struct stat statbuf;
        char full_path[MAX_PATH_LENGTH];
        snprintf(full_path, MAX_PATH_LENGTH, "%s/%s", path, entry->d_name);

        if (stat(full_path, &statbuf) == -1) {
            perror("stat");
            closedir(dir);
            return;
        }

        if (S_ISDIR(statbuf.st_mode)) {
            directories[num_directories] = strdup(entry->d_name);
            if (directories[num_directories] == NULL) {
                fprintf(stderr, "Memory allocation failed\n");
                closedir(dir);
                return;
            }
            num_directories++;
        }
    }

    closedir(dir);
    qsort(directories, num_directories, sizeof(char *), compare_dir_names);
    
    // Ensure the buffer is clean
    memset(outputBuff, 0, sizeof(outputBuff));
    // Write the output of directories to the outputBuff
    for (int i = 0; i < num_directories; i++) {
        snprintf(outputBuff + strlen(outputBuff), sizeof(outputBuff) - strlen(outputBuff), "%s\n", directories[i]);
        free(directories[i]);
    }
}

int receive_file(int sockfd, const char *filename) {
    printf("Filename: %s\n", filename);
    int file_fd, bytes_written;
    ssize_t bytes_read, total_bytes_read = 0;
    off_t file_size;
    char buffer[OUTPUT_BUFF_SIZE];
    
    file_fd = open(filename, O_WRONLY | O_CREAT | O_TRUNC, 0666);
    if (file_fd < 0) {
        perror("Failed to open file for writing");
        return -1;
    }

    read(sockfd, &file_size, sizeof(file_size));
    printf("File size: %ld\n", file_size);

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
    return 0;
}
void remove_file(const char *filename) {
    pid_t pid = fork();

    if (pid == -1) {
        // Fork failed
        perror("fork");
        exit(EXIT_FAILURE);
    } else if (pid == 0) {
        // In the child process
        execlp("rm", "rm", filename, (char *)NULL);
        // If execlp returns, it must have failed
        perror("execlp");
        exit(EXIT_FAILURE);
    } else {
        // In the parent process
        int status;
        waitpid(pid, &status, 0);  // Wait for the child process to finish
        if (WIFEXITED(status)) {
            printf("The file '%s' was removed successfully\n", filename);
        } else {
            printf("Failed to remove the file '%s'\n", filename);
        }
    }
}

int send_file_to_client(char *file_name, int socket_fd) {
    printf("Sending file: %s\n", file_name);
    int file_fd;
    ssize_t bytes_read, bytes_written;
    char buffer[OUTPUT_BUFF_SIZE];
    void *p;
    struct stat file_stat;

    // Open the file
    file_fd = open(file_name, O_RDONLY);
    if (file_fd < 0) {
        perror("Failed to open file");
        return -1;
    }

    fstat(file_fd, &file_stat);
    off_t file_size = file_stat.st_size;
    printf("Check File size: %ld\n", file_size);
    write(socket_fd, &file_size, sizeof(file_size));

    while ((bytes_read = read(file_fd, buffer, OUTPUT_BUFF_SIZE)) > 0) {
        char *p = buffer;
        while (bytes_read > 0) {
            bytes_written = write(socket_fd, p, bytes_read);
            if (bytes_written <= 0) {
                perror("Failed to write to socket");
                close(file_fd);
                return -1;
            }
            bytes_read -= bytes_written;
            p += bytes_written;
        }
    }

    // Close the file after successful transmission
    close(file_fd);
    printf("File sent successfully\n");
    remove_file(file_name);
    return 0;
}

void crequest(int newsockfd, int sockfd_mirror1, int sockfd_mirror2) {
    // memset(outputBuff, 0, sizeof(outputBuff));
    // int n = read(newsockfd, outputBuff, 4096);

    if(determinServer() == 2){
        while(1){
            // char buffer[1024];
            memset(outputBuff, 0, sizeof(outputBuff));
            int n = read(newsockfd, outputBuff, 4096); // Read the command from the client
            if (n <= 0){
                return;
            }

            strcpy(command, outputBuff);
            char *cmd = strtok(command, " ");
            if(strncmp("w24fz",cmd, 5) == 0){
                write(sockfd_mirror1, outputBuff, strlen(outputBuff));
                char *filename = "temp.tar.gz";
                if (receive_file(sockfd_mirror1, filename) == 0) {
                    printf("File received and saved successfully.\n");
                } else {
                    printf("Failed to receive the file.\n");
                }
                printf("check filename: %s\n", filename);
                send_file_to_client(filename, newsockfd);
                // continue;
            } else {
                write(sockfd_mirror1, outputBuff, strlen(outputBuff));
                memset(outputBuff, 0, sizeof(outputBuff));
                n = read(sockfd_mirror1, outputBuff, sizeof(outputBuff)-1);
                write(newsockfd, outputBuff, n);
                if (n < 0) {
                    error("ERROR reading from socket");
                }
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

            strcpy(command, outputBuff);

            char *cmd = strtok(command, " ");
            printf("Command: %s\n", cmd);

            // Check if the command is "dirlist -a"
            if (strncmp("dirlist -a", outputBuff, 10) == 0) {
                // executeCommand("ls");
                const char* HOME_PATH = getenv("HOME");
                list_directories(HOME_PATH);
                write(newsockfd, outputBuff, strlen(outputBuff));
            } else if (strncmp("dirlist -t", outputBuff, 10) == 0) {
                const char* HOME_PATH = getenv("HOME");
                list_directories_by_birth_time(HOME_PATH);
                write(newsockfd, outputBuff, strlen(outputBuff));
            } else if (strncmp("w24fn",cmd, 5) == 0){
                char *fileName = strtok(NULL, " ");
                handle_search_file(fileName);
                printf("Output buffer: %s\n", outputBuff);
                write(newsockfd, outputBuff, strlen(outputBuff));
            } else if (strncmp("w24fz",cmd, 5) == 0){
                int size1 = atoi(strtok(NULL, " "));
                int size2 = atoi(strtok(NULL, " "));
                // printf("Size1: %d, Size2: %d\n", size1, size2);
                char *filename = "temp.tar.gz";
                send_file_to_client(filename, newsockfd);
                // printf("Output buffer: %s\n", outputBuff);
                // write(newsockfd, outputBuff, strlen(outputBuff));
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
        // error("Connection failed to mirror1");
        printf("Connection failed to mirror1\n");

    // Connection to mirror2
    sockfd_mirror2 = socket(AF_INET, SOCK_STREAM, 0);
    if(sockfd_mirror2 < 0) error("ERROR opening socket for mirror2");
    mirror2_addr.sin_family = AF_INET;
    mirror2_addr.sin_addr.s_addr = inet_addr("127.0.0.1");
    mirror2_addr.sin_port = htons(8081);
    if(connect(sockfd_mirror2, (struct sockaddr *) &mirror2_addr, sizeof(mirror2_addr)) < 0)
    //     error("Connection failed to mirror2");
        printf("Connection failed to mirror2\n");


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
