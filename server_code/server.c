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
#include <errno.h>

#define MAX_PATH_LENGTH 4096
#define MAX_DIRECTORIES 100
#define MAX_COMMAND_LEN 1024
#define OUTPUT_BUFF_SIZE 4096
#define BUFFER_SIZE 4096

int tarFlag = 0;
int flagForFDB = 0;
int flagForFDA = 0;
int total_connections = 0;
char outputBuff[OUTPUT_BUFF_SIZE];
char command[1024];
struct Directory {
    char name[MAX_PATH_LENGTH];
    time_t birth_time;
};

struct FileInfo {
    char name[MAX_PATH_LENGTH];
    char location[MAX_PATH_LENGTH];
    off_t size;
    time_t creation_time;
    mode_t permissions;
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
void search_files_w24fz(const char *path, off_t size1, off_t size2, const char *temp_folder);
void copy_file_w24fz(const char *src, const char *dest);
void create_tar_gz(const char *folder_path);
int remove_directory(const char *dir_path);
void get_tar_w24fz(off_t size1, off_t size2);
void handle_search_file(const char *filename);
char *file_birth_time(const char *file_path);
time_t parse_birth_time(const char *stat_output);
int search_file(const char *path, const char *filename, struct FileInfo *file_info);
void remove_file(const char *filename);
int send_file_to_client(char *file_name, int socket_fd);
int receive_file(int sockfd, const char *filename);
void get_tar_w24ft(const char *extensions[],int argc);
void search_directory_w24ft(const char *dir_path, const char *extensions[], int num_extensions, int *found, const char *temp_dir);
void create_temp_dir();
void remove_newline(char *str);
time_t parse_birth_time_w24fd(const char *stat_output);
void create_temp_dir_w24fd();
void search_files_w24fdb(const char *path);
void get_tar_w24fdb(const char *date);
void search_files_w24fda(const char *path);
void get_tar_w24fda(const char *date);


// Global variable to hold the target filename
char *target_filename;

time_t target_date;
char temp_dir[MAX_PATH_LENGTH];
//------------------------ w24fda ------------------------
void get_tar_w24fda(const char *date){ // Function to get tar file for files created after a specific date
    struct tm tm = {0};
    strptime(date, "%Y-%m-%d", &tm);
    target_date = mktime(&tm); // Convert struct tm to time_t

    create_temp_dir_w24fd(); // Create temp directory

    // Search files and copy them to temp directory
    const char *home_dir = getenv("HOME"); 
    if (home_dir == NULL) {
        fprintf(stderr, "Failed to get user's home directory\n");
        return;
    }

    search_files_w24fda(home_dir); // Search files in the home directory
    
    const char *temp_folder = "temp";
    if(flagForFDA == 1){ // If files are found then only create tar file
        create_tar_gz(temp_folder);
    }
     // To remove temp after tar creation
    if (remove_directory(temp_folder) == -1) {
    fprintf(stderr, "Failed to remove existing temp folder after tar creation\n");
    return;
    }
}
void search_files_w24fda(const char *path) { // Function to search for files created after a specific date
    DIR *dir; // Directory stream
    struct dirent *entry; // Directory entry
    struct stat statbuf; // File status
    char full_path[MAX_PATH_LENGTH];

    if ((dir = opendir(path)) == NULL) {
        perror("opendir");
        return;
    }
     const char *temp_folder = "temp";
    while ((entry = readdir(dir)) != NULL) {
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) { // Skip current directory, parent directory, and temp directory
            continue; 
        }

        int required_length = snprintf(NULL, 0, "%s/%s", path, entry->d_name);
        if (required_length >= MAX_PATH_LENGTH) {
            fprintf(stderr, "Path length exceeds buffer size\n");
            continue; // Skip this file 
        }

        snprintf(full_path, MAX_PATH_LENGTH, "%s/%s", path, entry->d_name);

        if (lstat(full_path, &statbuf) == -1) { // Get file status
            perror("lstat");
            continue; // Failed to get file status
        }

        if (S_ISDIR(statbuf.st_mode)) { // Check if the entry is a directory
            // Skip the 'temp' directory
            if (strcmp(entry->d_name, temp_folder) == 0 && strcmp(path, getcwd(NULL, 0))==0) { // Skip the 'temp' directory
                continue;
            }
            search_files_w24fda(full_path); // Recursively search subdirectories
        } else if (S_ISREG(statbuf.st_mode)) { // Check if the entry is a regular file
            // Try to get birth time
            char *stat_output = file_birth_time(full_path);
            if (stat_output != NULL) {
                time_t birth_time = parse_birth_time_w24fd(stat_output);
                free(stat_output); // Free allocated memory
                if (birth_time >= target_date) { // Check if the file was created after the target date
                    flagForFDA = 1;
                    // Copy file to temp directory
                    char dest_path[MAX_PATH_LENGTH];
                    snprintf(dest_path, MAX_PATH_LENGTH, "%s/%s", temp_dir, entry->d_name);
                    int src_fd = open(full_path, O_RDONLY);
                    int dest_fd = open(dest_path, O_WRONLY | O_CREAT | O_TRUNC, statbuf.st_mode);
                    if (src_fd == -1 || dest_fd == -1) {
                        perror("open");
                        continue;
                    }
                    char buf[BUFFER_SIZE];
                    ssize_t num_read;
                    while ((num_read = read(src_fd, buf, BUFFER_SIZE)) > 0) {
                        if (write(dest_fd, buf, num_read) != num_read) {
                            perror("write");
                            close(src_fd);
                            close(dest_fd);
                            continue;
                        }
                    }
                    close(src_fd);
                    close(dest_fd);
                    printf("File copied:\n");
                    printf("Location: %s\n", full_path);
                    printf("Birth Time: %s", ctime(&birth_time));
                }
            }
        }
    }

    closedir(dir);
}
// ----------------- w24fdb -----------------
// Function to parse birth time from stat output
time_t parse_birth_time_w24fd(const char *stat_output) { // Function to parse birth time from stat output
    char *line = strtok((char *)stat_output, "\n");
    while (line != NULL) {
        if (strstr(line, "Birth:") != NULL) {
            line += strlen("Birth: "); // Move to the actual birth time value
            struct tm tm;
            strptime(line, "%Y-%m-%d %H:%M:%S", &tm); // Parse the birth time
            time_t birth_time = mktime(&tm); // Convert struct tm to time_t
            return birth_time;
        }
        line = strtok(NULL, "\n");
    }
    return -1; // Birth time not found
}

void create_temp_dir_w24fd() { // Function to create temp directory in the home directory
    char *cwd = getcwd(NULL, 0);
    if (cwd == NULL) {
        perror("getcwd");
        return;
    }
    snprintf(temp_dir, sizeof(temp_dir), "%s/temp", cwd);
    // Folder already exists, remove and recreate
    if (remove_directory(temp_dir) == -1) { // Remove existing temp directory
    fprintf(stderr, "Failed to remove existing temp folder\n");
    return;
    }
    // Create temp2 directory
    if (mkdir(temp_dir, S_IRWXU | S_IRWXG | S_IRWXO) == -1) {
        perror("mkdir");
        exit(EXIT_FAILURE);
    }
}

void search_files_w24fdb(const char *path) { // Function to search for files created before a specific date
    DIR *dir; // Directory stream
    struct dirent *entry; // Directory entry
    struct stat statbuf; // File status
    char full_path[MAX_PATH_LENGTH];

    if ((dir = opendir(path)) == NULL) {
        perror("opendir");
        return;
    }
     const char *temp_folder = "temp";
    while ((entry = readdir(dir)) != NULL) {
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) { // Skip current directory, parent directory, and temp directory
            continue; 
        }

        int required_length = snprintf(NULL, 0, "%s/%s", path, entry->d_name);
        if (required_length >= MAX_PATH_LENGTH) {
            fprintf(stderr, "Path length exceeds buffer size\n");
            continue; // Skip this file or handle error
        }

        snprintf(full_path, MAX_PATH_LENGTH, "%s/%s", path, entry->d_name);

        if (lstat(full_path, &statbuf) == -1) { // Get file status
            perror("lstat");
            continue; // Failed to get file status
        }

        if (S_ISDIR(statbuf.st_mode)) {
            // Skip the 'temp' directory
            if (strcmp(entry->d_name, temp_folder) == 0 && strcmp(path, getcwd(NULL, 0))==0) {
                continue;
            }
            search_files_w24fdb(full_path); // Recursively search subdirectories
        } else if (S_ISREG(statbuf.st_mode)) {
            // Try to get birth time
            char *stat_output = file_birth_time(full_path);
            if (stat_output != NULL) {
                time_t birth_time = parse_birth_time_w24fd(stat_output);
                free(stat_output); // Free allocated memory
                if (birth_time <= target_date) {
                    flagForFDB = 1;
                    // Copy file to temp directory
                    char dest_path[MAX_PATH_LENGTH];
                    snprintf(dest_path, MAX_PATH_LENGTH, "%s/%s", temp_dir, entry->d_name);
                    int src_fd = open(full_path, O_RDONLY);
                    int dest_fd = open(dest_path, O_WRONLY | O_CREAT | O_TRUNC, statbuf.st_mode);
                    if (src_fd == -1 || dest_fd == -1) {
                        perror("open");
                        continue;
                    }
                    char buf[BUFFER_SIZE];
                    ssize_t num_read;
                    while ((num_read = read(src_fd, buf, BUFFER_SIZE)) > 0) {
                        if (write(dest_fd, buf, num_read) != num_read) {
                            perror("write");
                            close(src_fd);
                            close(dest_fd);
                            continue;
                        }
                    }
                    close(src_fd);
                    close(dest_fd);
                    printf("File copied:\n");
                    printf("Location: %s\n", full_path);
                    printf("Birth Time: %s", ctime(&birth_time));
                }
            }
        }
    }

    closedir(dir);
}

void get_tar_w24fdb(const char *date){ // Function to get tar file for files created before a specific date
    // Parsing target date
    struct tm tm = {0};
    strptime(date, "%Y-%m-%d", &tm);
    tm.tm_hour = 23;
    tm.tm_min = 59;
    tm.tm_sec = 59;
    target_date = mktime(&tm);

    // Create temp directory
    create_temp_dir_w24fd();

    // Search files and copy them to temp directory
    const char *home_dir = getenv("HOME");
    if (home_dir == NULL) {
        fprintf(stderr, "Failed to get user's home directory\n");
        return;
    }

    search_files_w24fdb(home_dir); // Search files in the home directory

    const char *temp_folder = "temp";
    if(flagForFDB == 1){ // If files are found then only create tar file
        create_tar_gz(temp_folder);
    }

    // Removing temp directory after tar creation
    if (remove_directory(temp_folder) == -1) {
        fprintf(stderr, "Failed to remove existing temp folder after tar creation\n");
        return;
    }
}
// ----------------- w24ft -----------------

int is_file_of_type_w24ft(const char *filename, const char *extension) { // Function to check if a file has a specific extension
    size_t filename_len = strlen(filename);
    size_t extension_len = strlen(extension);
    if (filename_len < extension_len + 1)
        return 0;
    return strcmp(filename + filename_len - extension_len, extension) == 0;
}

void search_directory_w24ft(const char *dir_path, const char *extensions[], int num_extensions, int *found, const char *temp_dir) { // Function to search for files with specified extensions in a directory
    DIR *dir; // Directory stream
    struct dirent *entry; // Directory entry
    char full_path[MAX_PATH_LENGTH];

    if ((dir = opendir(dir_path)) == NULL) { // Open directory
        perror("opendir");
        exit(EXIT_FAILURE);
    }
    while ((entry = readdir(dir)) != NULL) { // Read directory entries
        if (entry->d_type == DT_DIR && strcmp(entry->d_name, ".") != 0 && strcmp(entry->d_name, "..") != 0) { // Check if the entry is a directory
            // Skip the 'temp' directory
            if (strcmp(entry->d_name, temp_dir) == 0 && strcmp(dir_path, getcwd(NULL, 0))==0) {
                continue;
            }

            snprintf(full_path, sizeof(full_path), "%s/%s", dir_path, entry->d_name);
            search_directory_w24ft(full_path, extensions, num_extensions, found, temp_dir);
        } else if (entry->d_type == DT_REG) { // Check if the entry is a regular file
            for (int i = 0; i < num_extensions; ++i) {
                if (is_file_of_type_w24ft(entry->d_name, extensions[i])) { // Check if the file has the specified extension
                    printf("%s/%s\n", dir_path, entry->d_name);
                    char source_file[MAX_PATH_LENGTH];
                    char dest_file[MAX_PATH_LENGTH];
                    snprintf(source_file, sizeof(source_file), "%s/%s", dir_path, entry->d_name);
                    snprintf(dest_file, sizeof(dest_file), "%s/%s", temp_dir, entry->d_name);
                    int source_fd, dest_fd;
                    ssize_t bytes_read, bytes_written;
                    char buffer[BUFFER_SIZE];

                    if ((source_fd = open(source_file, O_RDONLY)) == -1) { // Open source file for reading
                        perror("open");
                        exit(EXIT_FAILURE);
                    }

                    // Create or open destination file for writing
                    if ((dest_fd = open(dest_file, O_CREAT | O_WRONLY | O_TRUNC, 0644)) == -1) { // Open destination file for writing
                        perror("open");
                        exit(EXIT_FAILURE);
                    }

                    // Copy data from source file to destination file
                    while ((bytes_read = read(source_fd, buffer, BUFFER_SIZE)) > 0) {
                        bytes_written = write(dest_fd, buffer, bytes_read);
                        if (bytes_written != bytes_read) {
                            perror("write");
                            exit(EXIT_FAILURE);
                        }
                    }

                    if (bytes_read == -1) {
                        perror("read");
                        exit(EXIT_FAILURE);
                    }

                    // Close file descriptors
                    if (close(source_fd) == -1 || close(dest_fd) == -1) {
                        perror("close");
                        exit(EXIT_FAILURE);
                    }

                    *found = 1;
                    break;
                }
            }
        }
    }

    closedir(dir);
}

void create_temp_dir() { // Function to create temp directory
    const char *temp_folder = "temp";
    if (remove_directory(temp_folder) == -1) { // Remove existing temp directory
    fprintf(stderr, "Failed to remove existing temp folder\n");
    return;
    }
    if (mkdir(temp_folder, S_IRWXU | S_IRWXG | S_IRWXO) == -1) { // Create temp directory
        perror("mkdir");
        exit(EXIT_FAILURE);
    }
}

void get_tar_w24ft(const char *extensions[],int argc) // Function to get tar file for files with specified extensions
{ 
    const char *temp_folder = "temp";
    create_temp_dir(); // Create temp directory
    const char *home_dir = getenv("HOME"); // Get the user's home directory

    if (home_dir == NULL) {
        fprintf(stderr, "Failed to get user's home directory\n");
        return;
    }


    int found = 0;
    search_directory_w24ft(home_dir, extensions, argc - 1, &found, temp_folder); // Search for files with specified extensions

    if (!found) { // No files found
        printf("No files found\n");
        return;
    }

    create_tar_gz(temp_folder); // Create tar file of the temp directory

    if (remove_directory(temp_folder) == -1) {
    fprintf(stderr, "Failed to remove existing temp folder after tar creation\n");
    return;
    }
}
// ----------------- w24fz -----------------

void search_files_w24fz(const char *path, off_t size1, off_t size2, const char *temp_folder) { // Function to search for files within a specific size range
    DIR *dir; // Directory stream
    struct dirent *entry; // Directory entry
    struct stat statbuf;
    char full_path[MAX_PATH_LENGTH];

    if ((dir = opendir(path)) == NULL) {
        perror("opendir");
        return;
    }

    while ((entry = readdir(dir)) != NULL) {
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) {
            continue;
        }

        snprintf(full_path, MAX_PATH_LENGTH, "%s/%s", path, entry->d_name);
        if (lstat(full_path, &statbuf) == -1) {
            perror("lstat");
            continue;
        }

        if (S_ISDIR(statbuf.st_mode)) { // Check if the entry is a directory
            // Skip the 'temp' directory
                if (strcmp(entry->d_name, temp_folder) == 0 && strcmp(path, getcwd(NULL, 0)) == 0){
                continue;
            }
            search_files_w24fz(full_path, size1, size2, temp_folder); // Recursively search subdirectories
        } else if (S_ISREG(statbuf.st_mode)) {
            off_t file_size = statbuf.st_size;
            if (file_size >= size1 && file_size <= size2) {
                printf("%s\n", full_path);
                // Copy the file to the temp folder
                char temp_file_path[MAX_PATH_LENGTH];
                snprintf(temp_file_path, MAX_PATH_LENGTH, "%s/%s", temp_folder, entry->d_name);
                copy_file_w24fz(full_path, temp_file_path); // Copy the file to the temp folder
            }
        }
    }

    closedir(dir);
}
void copy_file_w24fz(const char *src, const char *dest) { // Function to copy a file from source to destination
    int src_fd, dest_fd;
    ssize_t bytes_read;
    char buffer[BUFFER_SIZE];

    src_fd = open(src, O_RDONLY);
    if (src_fd == -1) {
        perror("open");
        return;
    }

    dest_fd = open(dest, O_WRONLY | O_CREAT | O_TRUNC, S_IRUSR | S_IWUSR); // Create file with read-write permissions for user
    if (dest_fd == -1) {
        perror("open");
        close(src_fd);
        return;
    }

    while ((bytes_read = read(src_fd, buffer, BUFFER_SIZE)) > 0) { // Read from source file and write to destination file
        if (write(dest_fd, buffer, bytes_read) != bytes_read) { // Write may not write all bytes at once
            perror("write");
            close(src_fd);
            close(dest_fd);
            return;
        }
    }

    if (bytes_read == -1) {
        perror("read");
    }

    close(src_fd);
    close(dest_fd);
}
void create_tar_gz(const char *folder_path) { // Function to create a tar.gz archive of the specified folder
    pid_t pid = fork();
    if (pid == -1) {
        perror("fork");
        return;
    }
    if (pid == 0) {
        execlp("tar", "tar", "-czf", "temp.tar.gz", folder_path, (char *)NULL); // Execute tar command
        perror("execlp");
        exit(EXIT_FAILURE);
    } else {// Parent process
        int status;
        waitpid(pid, &status, 0); // Wait for the child process to finish
        if (!WIFEXITED(status) || WEXITSTATUS(status) != 0) { // Check if child process exited normally
            fprintf(stderr, "Failed to create tar file\n");
        } else {
            printf("Tar file created successfully: temp.tar.gz\n");
        }
    }
}
int remove_directory(const char *dir_path) {
    pid_t pid;
    int status;

    pid = fork();

    if (pid < 0) {
        // Fork failed
        perror("Fork failed");
        return -1;
    } else if (pid == 0) {
        // Child process
        //char *programname = "rm";
        //char *args[] = {"rm", "-rf", dir_path, NULL};
        //execvp(programname, args);
        const char *args[] = {"rm", "-rf", dir_path, NULL};
        execvp(args[0], (char *const *)args);
        // If execvp returns, it must have failed
        perror("Execvp failed");
        exit(EXIT_FAILURE);
    } else {
        // Parent process
        // Wait for the child process to finish
        waitpid(pid, &status, 0);

        // Check if child process exited normally
        if (WIFEXITED(status)) {
            int exit_status = WEXITSTATUS(status);
            if (exit_status == 0) {
                printf("Directory %s removed successfully.\n", dir_path);
                return 0;
            } else {
                fprintf(stderr, "Failed to remove directory %s. Exit status: %d\n", dir_path, exit_status);
                return -1;
            }
        } else {
            fprintf(stderr, "Child process did not exit normally.\n");
            return -1;
        }
    }
}
void get_tar_w24fz(off_t size1, off_t size2){ // Function to get tar file for files within a specific size range
   const char *home_dir = getenv("HOME"); // Get the user's home directory
    if (home_dir == NULL) {
        fprintf(stderr, "Failed to get user's home directory\n");
        return;
    }

    printf("Files in the directory tree rooted at %s with sizes between %lld and %lld bytes:\n", home_dir, (long long)size1, (long long)size2);

    // Create or recreate the temp folder
    const char *temp_folder = "temp";
    if (mkdir(temp_folder, 0777) == -1) {
        if (errno != EEXIST) {
            perror("mkdir");
            return;
        } else {
            // Folder already exists, remove and recreate
            if (remove_directory(temp_folder) == -1) {
                fprintf(stderr, "Failed to remove existing temp folder\n");
                return;
            }
            if (mkdir(temp_folder, 0777) == -1) {
                perror("mkdir");
                return;
            }
        }
    }

    search_files_w24fz(home_dir, size1, size2, temp_folder);
    // Create tar file of the temp folder
    create_tar_gz(temp_folder);
    // To remove temp after tar creation
    printf("Attempting to remove temp folder from: %s\n", getcwd(NULL, 0));
if (remove_directory(temp_folder) == -1) {
    fprintf(stderr, "Failed to remove existing temp folder after tar creation\n");
    return;
}
}

char *file_birth_time(const char *file_path) {
    // char stat_cmd[MAX_COMMAND_LEN];
    // snprintf(stat_cmd, MAX_COMMAND_LEN, "stat \"%s\"", file_path);

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
        // system(stat_cmd);
        execlp("stat", "stat", file_path, NULL);
        perror("execlp");
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

void handle_search_file(const char *filename) { // Function to search for a file in the user's home directory and display its information
    struct FileInfo file_info; // Structure to hold file information
    const char *home_dir = getenv("HOME"); // Get the user's home directory
    int offset = 0; // This will keep track of the current position in outputBuff

    if (home_dir == NULL) {
        offset += snprintf(outputBuff + offset, OUTPUT_BUFF_SIZE - offset, "Failed to get user's home directory\n");
        return;
    }

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
    if (offset >= OUTPUT_BUFF_SIZE) { // Check if the buffer was large enough
        outputBuff[OUTPUT_BUFF_SIZE - 1] = '\0'; // Null-terminate the string
    } else {
        outputBuff[offset] = '\0'; // Null-terminate the string
    }
}


void error(const char *msg) {
    perror(msg);
    exit(1);
}

int compare_directories(const void *a, const void *b) { // Function to compare directories by birth time
    const struct Directory *dir1 = (const struct Directory *)a; // Cast the pointers to Directory structure
    const struct Directory *dir2 = (const struct Directory *)b; 
    return difftime(dir1->birth_time, dir2->birth_time); // Compare the birth times of the directories
}

void directory_birth_time(const char *dir_name, struct Directory *dir) { // Function to get the birth time of a directory
    int pipefd[2]; // File descriptors for the pipe
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

        execlp("stat", "stat", "-c", "%w", dir_name, NULL); // Execute the stat command
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
            if (strptime(buffer, "%Y-%m-%d %H:%M:%S", &tm_time) != NULL) { // Parse the birth time
                dir->birth_time = mktime(&tm_time); // Convert the time to time_t
            }
        }
        close(pipefd[0]);
    }
}


void list_directories_by_birth_time(const char *home_dir) { // To list directories in the specified path by birth time
    DIR *dir;
    struct dirent *ent;
    struct Directory dirs[MAX_DIRECTORIES];
    int num_dirs = 0;

    if ((dir = opendir(home_dir)) != NULL) { // Open the directory
        while ((ent = readdir(dir)) != NULL && num_dirs < MAX_DIRECTORIES) { // Read each directory entry
            if (ent->d_type == DT_DIR && strcmp(ent->d_name, ".") != 0 && strcmp(ent->d_name, "..") != 0) { // Check if the entry is a directory
                char dir_path[MAX_PATH_LENGTH];
                snprintf(dir_path, sizeof(dir_path), "%s/%s", home_dir, ent->d_name);
                directory_birth_time(dir_path, &dirs[num_dirs]); // Get the birth time of the directory
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
    qsort(dirs, num_dirs, sizeof(struct Directory), compare_directories); // Sort the directories
    memset(outputBuff, 0, sizeof(outputBuff));
    for (int i = 0; i < num_dirs; i++) {
        snprintf(outputBuff + strlen(outputBuff), sizeof(outputBuff) - strlen(outputBuff), "%s - %s", dirs[i].name, ctime(&dirs[i].birth_time));
    }
}

int determinServer(){
    if (total_connections < 3){
        return 1;
    } else if (total_connections >= 3 && total_connections < 6){
        return 2;
    } else if (total_connections >= 6 && total_connections < 9){
        return 3;
    } else {
        return (total_connections % 3) + 1;
    
    }
    // return (total_connections % 3) + 1;
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

void list_directories(const char *path) { // Function to list directories in the specified path in sorted order
    DIR *dir; // Pointer to a directory stream
    struct dirent *entry; // Pointer to a directory entry
    char *directories[MAX_DIRECTORIES]; // Array of directory names
    int num_directories = 0;

    if ((dir = opendir(path)) == NULL) { // Open the directory
        perror("opendir");
        return;
    }

    while ((entry = readdir(dir)) != NULL) { // Read each directory entry
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) { // Skip the current and parent directories
            continue;
        }

        struct stat statbuf; // Buffer for file status
        char full_path[MAX_PATH_LENGTH]; // Buffer for full path of the directory
        snprintf(full_path, MAX_PATH_LENGTH, "%s/%s", path, entry->d_name);

        if (stat(full_path, &statbuf) == -1) { // Get the file status
            perror("stat");
            closedir(dir);
            return;
        }

        if (S_ISDIR(statbuf.st_mode)) { // Check if the entry is a directory
            directories[num_directories] = strdup(entry->d_name); // Copy the directory name
            if (directories[num_directories] == NULL) { 
                fprintf(stderr, "Memory allocation failed\n");
                closedir(dir);
                return;
            }
            num_directories++; // Increment the number of directories
        }
    }

    closedir(dir);
    qsort(directories, num_directories, sizeof(char *), compare_dir_names); // Sort the directories
    
    memset(outputBuff, 0, sizeof(outputBuff)); // Clear the output buffer
    for (int i = 0; i < num_directories; i++) { // Append each directory name to the output buffer
        snprintf(outputBuff + strlen(outputBuff), sizeof(outputBuff) - strlen(outputBuff), "%s\n", directories[i]);
        free(directories[i]);
    }
}

void remove_file(const char *filename) { // Function to remove a file
    pid_t pid = fork();

    if (pid == -1) {
        perror("fork");
        exit(EXIT_FAILURE);
    } else if (pid == 0) { // In the child process
        execlp("rm", "rm", filename, (char *)NULL);
        perror("execlp"); // This will be printed if execlp fails
        exit(EXIT_FAILURE);
    } else { // In the parent process
        int status;
        waitpid(pid, &status, 0);  // Wait for the child process to finish
        if (WIFEXITED(status)) { // Check if the child process exited normally
            printf("The file '%s' was removed successfully\n", filename);
        } else {
            printf("Failed to remove the file '%s'\n", filename);
        }
    }
}
int receive_file(int sockfd, const char *filename) { // Function to receive a file from the client
    // printf("Filename: %s\n", filename);
    int file_fd, bytes_written;
    ssize_t bytes_read, total_bytes_read = 0;
    off_t file_size;
    char buffer[BUFFER_SIZE];
    char message[14];
    int bt_read = read(sockfd, message, 13);
    message[13] = '\0'; // Null-terminate the message
    // printf("Bytes read: %d\n", bt_read);
    if (strcmp(message, "No file found") == 0) {
        printf("%s\n",message);
        return 0;
    }else{
        printf("%s\n",message);
        
        read(sockfd, &file_size, sizeof(file_size));
        printf("File size: %ld\n", file_size);

        file_fd = open(filename, O_WRONLY | O_CREAT | O_TRUNC, 0666); // Open the file for writing
        if (file_fd < 0) {
            perror("Failed to open file for writing");
            return -1;
        }

        while (total_bytes_read < file_size) { // Loop until the entire file is read
            bytes_read = read(sockfd, buffer, sizeof(buffer)); // Read from the socket
            if (bytes_read < 0) { // Check for read errors
                perror("Failed to read from socket");
                close(file_fd);
                return -1;
            }
            total_bytes_read += bytes_read; // Update the total bytes read
            bytes_written = write(file_fd, buffer, bytes_read); // Write to the file
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

int send_file_to_client(char *file_name, int socket_fd) { // Function to send a file to the client
    printf("Sending file: %s\n", file_name);
    int file_fd;
    ssize_t bytes_read, bytes_written;
    char buffer[OUTPUT_BUFF_SIZE];
    void *p;
    struct stat file_stat;

    file_fd = open(file_name, O_RDONLY); // Open the file for reading only
    if (file_fd < 0) {
        
        char *msg = "No file found";
        printf("No file found\n");
        int bt_written = write(socket_fd, msg, strlen(msg));
        remove_directory("temp");
        return 0;
    } else{
    
    char *msg = "File is found";
    printf("File is found\n");
    // printf("File size:- %d\n", strlen(msg));
    int bt_written = write(socket_fd, msg, strlen(msg));
    
    // printf("Bytes written: %d\n", bt_written);

    fstat(file_fd, &file_stat);
    off_t file_size = file_stat.st_size;
    printf("Check File size: %ld\n", file_size);
    write(socket_fd, &file_size, sizeof(file_size));


    while ((bytes_read = read(file_fd, buffer, OUTPUT_BUFF_SIZE)) > 0) { // Read from the file
        char *p = buffer;
        while (bytes_read > 0) {
            bytes_written = write(socket_fd, p, bytes_read); // Write to the socket
            if (bytes_written <= 0) { // Check for write errors
                perror("Failed to write to socket");
                close(file_fd);
                return -1;
            }
            bytes_read -= bytes_written; // Update the bytes read
            p += bytes_written; // Move the buffer pointer
        }
    }

    // Close the file after successful transmission
    close(file_fd);
    printf("File sent successfully\n");
    remove_file(file_name);
    }
    return 0;
}

void remove_newline(char *str) { // Function to remove newline characters from a string
    printf("In here\n");
    size_t len = strlen(str);
    if (len > 0 && str[len - 1] == '\n') { // Check and replace newline at the end of the string
        str[len - 1] = '\0';
    }

    if (str[0] == '\n') { // Check and replace newline at the start of the string
        memmove(str, str + 1, len); // Move the string one position to the left
        str[len - 1] = '\0'; // Correctly terminate the string
    }
}
void crequest(int newsockfd, int sockfd_mirror1, int sockfd_mirror2) { // Function to handle client requests

    if(determinServer() == 2){ // Will forward the request to mirror1
        while(1){
            memset(outputBuff, 0, sizeof(outputBuff));
            int n = read(newsockfd, outputBuff, 4096); // Read the command from the client
            if (n <= 0){
                return;
            }

            strcpy(command, outputBuff);
            char *cmd = strtok(command, " ");
            if((strncmp("w24fz",cmd, 5) == 0) || (strncmp("w24ft",cmd, 5) == 0) || (strncmp("w24fdb",cmd, 6) == 0) || (strncmp("w24fda",cmd, 6) == 0)){ // Handling redirection for the commands w24fz, w24ft, w24fdb, w24fda which require temp.tar.gz file to be sent to the client
                write(sockfd_mirror1, outputBuff, strlen(outputBuff)); // Write the command to the mirror1 server
                char *filename = "temp.tar.gz";
                receive_file(sockfd_mirror1, filename); // Receive the file from the mirror1 server
                send_file_to_client(filename, newsockfd); // Send the file to the client
            } else {
                write(sockfd_mirror1, outputBuff, strlen(outputBuff)); // Write the command to the mirror1 server
                memset(outputBuff, 0, sizeof(outputBuff));
                n = read(sockfd_mirror1, outputBuff, sizeof(outputBuff)-1); // Read the output from the mirror1 server
                write(newsockfd, outputBuff, n); // Write the output to the client
                if (n < 0) {
                    error("ERROR reading from socket");
                }
            }
        }
    } else if(determinServer() == 3){ // Will forward the request to mirror2
        while(1){
            memset(outputBuff, 0, sizeof(outputBuff)); // Clear the output buffer
            int n = read(newsockfd, outputBuff, 4096); // Read the command from the client
            if (n <= 0){
                return;
            }

            strcpy(command, outputBuff); // Copy the command to the command buffer
            char *cmd = strtok(command, " "); // Getting the command name
            if((strncmp("w24fz",cmd, 5) == 0) || (strncmp("w24ft",cmd, 5) == 0) || (strncmp("w24fdb",cmd, 6) == 0) || (strncmp("w24fda",cmd, 6) == 0)){ // Handling redirection for the commands w24fz, w24ft, w24fdb, w24fda which require temp.tar.gz file to be sent to the client
                write(sockfd_mirror2, outputBuff, strlen(outputBuff)); // Write the command to the mirror2 server
                char *filename = "temp.tar.gz";
                receive_file(sockfd_mirror2, filename); // Receive the file from the mirror2 server
                send_file_to_client(filename, newsockfd); // Send the file to the client
            } else {
                write(sockfd_mirror2, outputBuff, strlen(outputBuff)); // Write the command to the mirror2 server
                memset(outputBuff, 0, sizeof(outputBuff)); // Clear the output buffer
                n = read(sockfd_mirror2, outputBuff, sizeof(outputBuff)-1); // Read the output from the mirror2 server
                write(newsockfd, outputBuff, n); // Write the output to the client
                if (n < 0) {
                    error("ERROR reading from socket");
                }
            }
        }
    } else {
        while(1){
            memset(outputBuff, 0, sizeof(outputBuff)); // Clear the output buffer
            int n = read(newsockfd, outputBuff, 4096); // Read the command from the client
            if (n <= 0){
                return;
            }

            printf("Received command: %s\n", outputBuff);
            if(outputBuff[strlen(outputBuff) - 1] == '\n') {
                printf("Removing trailing newline character\n");
                outputBuff[strlen(outputBuff) - 1] = '\0'; // Remove the trailing newline character
            }
            strcpy(command, outputBuff);
            char *cmd = strtok(command, " ");
            printf("Command: %s\n", cmd);

            if (strncmp("dirlist -a", outputBuff, 10) == 0) { // Handling request for dirlist -a
                const char* HOME_PATH = getenv("HOME");
                list_directories(HOME_PATH);
                write(newsockfd, outputBuff, strlen(outputBuff));
            } else if (strncmp("dirlist -t", outputBuff, 10) == 0) { // Handling request for dirlist -t
                const char* HOME_PATH = getenv("HOME");
                list_directories_by_birth_time(HOME_PATH);
                write(newsockfd, outputBuff, strlen(outputBuff));
            } else if (strncmp("w24fn",cmd, 5) == 0){ // Handling request for w24fn
                char *fileName = strtok(NULL, " ");
                handle_search_file(fileName);
                printf("Output buffer: %s\n", outputBuff);
                write(newsockfd, outputBuff, strlen(outputBuff));
            } else if (strncmp("w24fz",cmd, 5) == 0){ // Handling request for w24fz
                off_t size1 = atoll(strtok(NULL, " "));
                off_t size2 = atoll(strtok(NULL, " "));
                get_tar_w24fz(size1,size2);
                char *filename = "temp.tar.gz";
                send_file_to_client(filename, newsockfd);
            } else if (strncmp("w24ft",cmd, 5) == 0){ // Handling request for w24ft
                char *ext[3]; 
                int arg_count = 0;
                char *token;
                while ((token = strtok(NULL, " ")) != NULL) {
                    ext[arg_count++] = strdup(token); // Copy the argument to the ext array
                }
                const char *extensions[4];
                for (int i = 0; i < arg_count; i++) {
                    extensions[i] = ext[i]; // Initialize extensions array with values from ext
                }
                for (int i = 0; i < arg_count; i++) {
                    printf("Extension: %s\n", extensions[i]);
                }
                get_tar_w24ft(extensions,arg_count+1); // Get the tar file for the specified extensions
                printf("Sending file to client\n");
                char *filename = "temp.tar.gz";
                send_file_to_client(filename, newsockfd); // Send the file to the client
                for (int i = 0; i < arg_count; i++) {
                    free(ext[i]);
                }
            } else if (strncmp("w24fdb",cmd, 6) == 0){ // Handlig request for w24fdb
                char *date = strtok(NULL, " ");
                get_tar_w24fdb(date);
                char *filename = "temp.tar.gz";
                send_file_to_client(filename, newsockfd);
            }else if (strncmp("w24fda",cmd, 6) == 0){ // Handlig request for w24fda
                char *date = strtok(NULL, " ");
                get_tar_w24fda(date);
                char *filename = "temp.tar.gz";
                send_file_to_client(filename, newsockfd);
            }
            // Check if the command is "quitc" and exit the process
            else if (strncmp("quitc", outputBuff, 5) == 0) {
                printf("Quit command received. Exiting child process: %d.\n", total_connections);
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
    struct sockaddr_in serv_addr, cli_addr, mirror1_addr, mirror2_addr; // Server and client address structures

    sockfd = socket(AF_INET, SOCK_STREAM, 0); // AF_INET: Address domain of the socket, SOCK_STREAM: Type of socket, 0: Default protocol (TCP)
    if(sockfd < 0) error("Error opening socket.");

    bzero((char *) &serv_addr, sizeof(serv_addr)); // Initialize serv_addr to zeros
    portno = atoi(argv[1]);

    serv_addr.sin_family = AF_INET; // AF_INET: Address domain of the socket
    serv_addr.sin_addr.s_addr = INADDR_ANY; // INADDR_ANY: Accept connections to all IP addresses of the machine
    serv_addr.sin_port = htons(portno); // htons: Convert port number to network byte order

    if(bind(sockfd, (struct sockaddr *) &serv_addr, sizeof(serv_addr)) < 0) 
        error("Binding failed.");

    listen(sockfd, 5);
    clilen = sizeof(cli_addr);
    
    // Connection to mirror1
    sockfd_mirror1 = socket(AF_INET, SOCK_STREAM, 0); // Create a new socket for mirror1
    if(sockfd_mirror1 < 0) error("ERROR opening socket for mirror1");
    mirror1_addr.sin_family = AF_INET; // Address family
    mirror1_addr.sin_addr.s_addr = inet_addr("127.0.0.1"); // IP address of mirror1
    mirror1_addr.sin_port = htons(8080); // Port number for mirror1
    if(connect(sockfd_mirror1, (struct sockaddr *) &mirror1_addr, sizeof(mirror1_addr)) < 0)
        // error("Connection failed to mirror1");
        printf("Connection failed to mirror1\n");

    // Connection to mirror2
    sockfd_mirror2 = socket(AF_INET, SOCK_STREAM, 0); // Create a new socket for mirror2
    if(sockfd_mirror2 < 0) error("ERROR opening socket for mirror2");
    mirror2_addr.sin_family = AF_INET; // Address family
    mirror2_addr.sin_addr.s_addr = inet_addr("127.0.0.1"); // IP address of mirror2
    mirror2_addr.sin_port = htons(8081); // Port number for mirror2
    if(connect(sockfd_mirror2, (struct sockaddr *) &mirror2_addr, sizeof(mirror2_addr)) < 0)
    //     error("Connection failed to mirror2");
        printf("Connection failed to mirror2\n");


    while(1) {
        newsockfd = accept(sockfd, (struct sockaddr *) &cli_addr, &clilen); // Accept a new connection
        if(newsockfd < 0) error("Error on accept.");

        int pid = fork(); // Fork a new process for each connection
        if(pid < 0) {
            error("Error on fork.");
        }

        if(pid == 0) { // Child process
            close(sockfd);
            crequest(newsockfd, sockfd_mirror1, sockfd_mirror2); // Handle the client request
            exit(0);
        }
        else {
            total_connections++; // Increment the total number of connections
            close(newsockfd); // Close the new socket in the parent process
            signal(SIGCHLD, SIG_IGN); // Ignore the signal sent by the child process
        }
    }

    close(sockfd);
    return 0;
}
