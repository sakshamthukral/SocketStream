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
#include <fcntl.h>
#include <errno.h>

#define MAX_PATH_LENGTH 4096
#define MAX_DIRECTORIES 100
#define OUTPUT_BUFF_SIZE 4096
#define MAX_COMMAND_LEN 1024
#define BUFFER_SIZE 4096
char outputBuff[OUTPUT_BUFF_SIZE];
char command[1024];
int flagForFDB = 0;
int flagForFDA = 0;

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
void error(const char *msg);
void list_directories(const char *path);
void executeCommand(const char *cmd);
void crequest(int newsockfd);
int determinServer();
int compare_dir_names(const void *a, const void *b);
void list_directories_by_birth_time(const char *home_dir);
void get_birth_time(const char *dir_name, struct Directory *dir);
int compare_directories(const void *a, const void *b);
void handle_search_file(const char *filename);
void remove_file(const char *filename);
int send_file_to_client(const char *file_name, int socket_fd);
void get_tar_w24fz(off_t size1, off_t size2);
void search_files_w24fz(const char *path, off_t size1, off_t size2, const char *temp_folder);
void copy_file_w24fz(const char *src, const char *dest);
void create_tar_gz(const char *folder_path);
int remove_directory(const char *dir_path);
int is_file_of_type_w24ft(const char *filename, const char *extension);
void search_directory_w24ft(const char *dir_path, const char *extensions[], int num_extensions, int *found, const char *temp_dir);
void create_temp_dir();
void get_tar_w24ft(const char *extensions[],int argc);
void get_tar_w24fdb(const char *date);
void search_files_w24fdb(const char *path);
void create_temp_dir_w24fd();
void search_files_w24fda(const char *path);
void get_tar_w24fda(const char *date);
time_t parse_birth_time_w24fd(const char *stat_output);
char *file_birth_time(const char *file_path);  // Function prototype


time_t target_date;
char temp_dir[MAX_PATH_LENGTH];
//------------------------ w24fda ------------------------
void get_tar_w24fda(const char *date){ // Function to get tar file of files created after a specific date
    struct tm tm = {0};
    strptime(date, "%Y-%m-%d", &tm);
    target_date = mktime(&tm);
    
    create_temp_dir_w24fd();// Create temp directory

    const char *home_dir = getenv("HOME"); // Searching files and copy them to temp directory
    if (home_dir == NULL) {
        fprintf(stderr, "Failed to get user's home directory\n");
        return;
    }

    search_files_w24fda(home_dir); // Search files and copy them to temp directory

    const char *temp_folder = "temp";
    if(flagForFDA == 1){
        create_tar_gz(temp_folder); // Create temp.tar.gz archive
    }
    if (remove_directory(temp_folder) == -1) {
    fprintf(stderr, "Failed to remove existing temp folder after tar creation\n");
    return;
    }
}
void search_files_w24fda(const char *path) {
    DIR *dir;
    struct dirent *entry;
    struct stat statbuf;
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
            continue; 
        }

        if (S_ISDIR(statbuf.st_mode)) {
            // Skip the 'temp' directory
            if (strcmp(entry->d_name, temp_folder) == 0 && strcmp(path, getcwd(NULL, 0))==0) {
                continue;
            }
            search_files_w24fda(full_path); // Recursively searching subdirectories
        } else if (S_ISREG(statbuf.st_mode)) {
            // Try to get birth time
            char *stat_output = file_birth_time(full_path); // Get birth time
            if (stat_output != NULL) {
                time_t birth_time = parse_birth_time_w24fd(stat_output); // Parse birth time
                free(stat_output); // Free allocated memory
                if (birth_time >= target_date) {
                    flagForFDA = 1;
                    // Copying file to temp directory
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
time_t parse_birth_time_w24fd(const char *stat_output) { // Function to parse birth time from stat output
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

void create_temp_dir_w24fd() { // Function to create temp directory in the home directory
    char *cwd = getcwd(NULL, 0);
    if (cwd == NULL) {
        perror("getcwd");
        return;
    }
    snprintf(temp_dir, sizeof(temp_dir), "%s/temp", cwd);
    // Folder already exists, remove and recreate
    if (remove_directory(temp_dir) == -1) {
    fprintf(stderr, "Failed to remove existing temp folder\n");
    return;
    }
    // Create temp2 directory
    if (mkdir(temp_dir, S_IRWXU | S_IRWXG | S_IRWXO) == -1) {
        perror("mkdir");
        exit(EXIT_FAILURE);
    }
}

void search_files_w24fdb(const char *path) { // Function to search files and extract files with birth time before a specific date
    DIR *dir;
    struct dirent *entry;
    struct stat statbuf;
    char full_path[MAX_PATH_LENGTH];

    if ((dir = opendir(path)) == NULL) {
        perror("opendir");
        return;
    }
     const char *temp_folder = "temp";
    while ((entry = readdir(dir)) != NULL) {
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) {
            continue; // Skip current directory, parent directory, and temp directory
        }

        int required_length = snprintf(NULL, 0, "%s/%s", path, entry->d_name);
        if (required_length >= MAX_PATH_LENGTH) {
            fprintf(stderr, "Path length exceeds buffer size\n");
            continue; // Skip this file or handle error
        }

        snprintf(full_path, MAX_PATH_LENGTH, "%s/%s", path, entry->d_name);

        if (lstat(full_path, &statbuf) == -1) {
            perror("lstat");
            continue; 
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

void get_tar_w24fdb(const char *date){ // Function to get tar file of files created before a specific date
    // Parse target date from input
    struct tm tm = {0}; // Initialize to all zeros
    strptime(date, "%Y-%m-%d", &tm); // Parse date string
    tm.tm_hour = 23; // Set time to 23:59:59
    tm.tm_min = 59; // Set time to 23:59:59
    tm.tm_sec = 59; // Set time to 23:59:59
    target_date = mktime(&tm); // Convert to time_t

    create_temp_dir_w24fd(); // Create temp directory

    // Search files and copy them to temp directory
    const char *home_dir = getenv("HOME");
    if (home_dir == NULL) {
        fprintf(stderr, "Failed to get user's home directory\n");
        return;
    }

    search_files_w24fdb(home_dir);

    // Create temp.tar.gz archive
    const char *temp_folder = "temp";
    if(flagForFDB == 1){
        create_tar_gz(temp_folder);
    }

    if (remove_directory(temp_folder) == -1) { // Remove temp directory after tar creation
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

void search_directory_w24ft(const char *dir_path, const char *extensions[], int num_extensions, int *found, const char *temp_dir) { // Function to search files with specific extensions
    DIR *dir; // Directory stream
    struct dirent *entry; // Directory entry
    char full_path[MAX_PATH_LENGTH];

    if ((dir = opendir(dir_path)) == NULL) {
        perror("opendir");
        exit(EXIT_FAILURE);
    }
    
    while ((entry = readdir(dir)) != NULL) { // Read directory entries
        if (entry->d_type == DT_DIR && strcmp(entry->d_name, ".") != 0 && strcmp(entry->d_name, "..") != 0) {
            // Skip the 'temp' directory
            if (strcmp(entry->d_name, temp_dir) == 0 && strcmp(dir_path, getcwd(NULL, 0))==0) {
                continue;
            }

            snprintf(full_path, sizeof(full_path), "%s/%s", dir_path, entry->d_name);
            search_directory_w24ft(full_path, extensions, num_extensions, found, temp_dir); // Recursively search subdirectories
        } else if (entry->d_type == DT_REG) {
            for (int i = 0; i < num_extensions; ++i) {
                if (is_file_of_type_w24ft(entry->d_name, extensions[i])) { // Check if file has a specific extension
                    printf("%s/%s\n", dir_path, entry->d_name);
                    char source_file[MAX_PATH_LENGTH];
                    char dest_file[MAX_PATH_LENGTH];
                    snprintf(source_file, sizeof(source_file), "%s/%s", dir_path, entry->d_name);
                    snprintf(dest_file, sizeof(dest_file), "%s/%s", temp_dir, entry->d_name);
                    int source_fd, dest_fd;
                    ssize_t bytes_read, bytes_written;
                    char buffer[BUFFER_SIZE];

                    // Open source file for reading
                    if ((source_fd = open(source_file, O_RDONLY)) == -1) {
                        perror("open");
                        exit(EXIT_FAILURE);
                    }

                    // Create or open destination file for writing
                    if ((dest_fd = open(dest_file, O_CREAT | O_WRONLY | O_TRUNC, 0644)) == -1) {
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

void create_temp_dir() { // Function to create temp directory in the home directory
    const char *temp_folder = "temp";
    // Folder already exists, remove and recreate
    if (remove_directory(temp_folder) == -1) {
    fprintf(stderr, "Failed to remove existing temp folder\n");
    return;
    }
    // Create temp directory
    if (mkdir(temp_folder, S_IRWXU | S_IRWXG | S_IRWXO) == -1) {
        perror("mkdir");
        exit(EXIT_FAILURE);
    }
}

void get_tar_w24ft(const char *extensions[],int argc) // Function to get tar file of files with specific extensions
{ 
    const char *temp_folder = "temp";
    create_temp_dir();
    const char *home_dir = getenv("HOME");
    if (home_dir == NULL) {
        fprintf(stderr, "Failed to get user's home directory\n");
        return;
    }

    int found = 0;
    search_directory_w24ft(home_dir, extensions, argc - 1, &found, temp_folder);

    if (!found) {
        printf("No files found\n");
        return;
    }

    create_tar_gz(temp_folder);

    if (remove_directory(temp_folder) == -1) {
    fprintf(stderr, "Failed to remove existing temp folder after tar creation\n");
    return;
    }
}
// ----------------- w24fz -----------------

void search_files_w24fz(const char *path, off_t size1, off_t size2, const char *temp_folder) { // Function to search files with specific sizes
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

        if (S_ISDIR(statbuf.st_mode)) { // If directory
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
                copy_file_w24fz(full_path, temp_file_path);
            }
        }
    }

    closedir(dir);
}
void copy_file_w24fz(const char *src, const char *dest) { // Function to copy file from source to destination
    int src_fd, dest_fd;
    ssize_t bytes_read;
    char buffer[BUFFER_SIZE];

    src_fd = open(src, O_RDONLY);
    if (src_fd == -1) {
        perror("open");
        return;
    }

    dest_fd = open(dest, O_WRONLY | O_CREAT | O_TRUNC, S_IRUSR | S_IWUSR);
    if (dest_fd == -1) {
        perror("open");
        close(src_fd);
        return;
    }

    while ((bytes_read = read(src_fd, buffer, BUFFER_SIZE)) > 0) {
        if (write(dest_fd, buffer, bytes_read) != bytes_read) {
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
void create_tar_gz(const char *folder_path) { // Function to create tar file of a folder
    pid_t pid = fork();
    if (pid == -1) {
        perror("fork");
        return;
    }
    if (pid == 0) {// Child process
        execlp("tar", "tar", "-czf", "temp.tar.gz", folder_path, (char *)NULL); // Create tar file
        perror("execlp");
        exit(EXIT_FAILURE);
    } else {// Parent process
        int status;
        waitpid(pid, &status, 0);
        if (!WIFEXITED(status) || WEXITSTATUS(status) != 0) { // Check if child process exited normally
            fprintf(stderr, "Failed to create tar file\n");
        } else {
            printf("Tar file created successfully: temp.tar.gz\n");
        }
    }
}
int remove_directory(const char *dir_path) { // Function to remove a directory
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
        execvp(args[0], (char *const *)args); // Execute rm -rf command
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
void get_tar_w24fz(off_t size1, off_t size2){ // Function to get tar file of files with specific sizes
   const char *home_dir = getenv("HOME");
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

    search_files_w24fz(home_dir, size1, size2, temp_folder); // Search files and copy them to temp folder
    
    create_tar_gz(temp_folder); // Create tar file of the temp folder
    // To remove temp after tar creation
    printf("Attempting to remove temp folder from: %s\n", getcwd(NULL, 0));
if (remove_directory(temp_folder) == -1) {
    fprintf(stderr, "Failed to remove existing temp folder after tar creation\n");
    return;
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
// Function to execute the stat command and retrieve output
char *file_birth_time(const char *file_path) {

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

        execlp("stat", "stat", file_path, NULL); // Execute stat command
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
int search_file(const char *path, const char *filename, struct FileInfo *file_info) {
    DIR *dir; // Directory stream
    struct dirent *entry; // Directory entry
    struct stat statbuf; // File status
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
        
                char *stat_output = file_birth_time(full_path); // Get birth time
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
void handle_search_file(const char *filename) { // Function to search for a file
    struct FileInfo file_info;
    printf("Searching for file in handle_search_file: %s\n", filename);
    const char *home_dir = getenv("HOME");
    int offset = 0; // This will keep track of the current position in outputBuff

    if (home_dir == NULL) {
        offset += snprintf(outputBuff + offset, OUTPUT_BUFF_SIZE - offset, "Failed to get user's home directory\n");
        return;
    }

    // memset(outputBuff, 0, sizeof(outputBuff));
    if (search_file(home_dir, filename, &file_info)) { // Search for the file
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

int compare_directories(const void *a, const void *b) { // Function to compare directories by birth time
    const struct Directory *dir1 = (const struct Directory *)a;
    const struct Directory *dir2 = (const struct Directory *)b;
    return difftime(dir1->birth_time, dir2->birth_time);
}

void get_birth_time(const char *dir_name, struct Directory *dir) { // Function to get birth time of a directory
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

        execlp("stat", "stat", "-c", "%w", dir_name, NULL); // Execute stat command
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


void list_directories_by_birth_time(const char *home_dir) { // Function to list directories by birth time
    DIR *dir;
    struct dirent *ent;
    struct Directory dirs[MAX_DIRECTORIES];
    int num_dirs = 0;

    if ((dir = opendir(home_dir)) != NULL) {
        while ((ent = readdir(dir)) != NULL && num_dirs < MAX_DIRECTORIES) {
            if (ent->d_type == DT_DIR && strcmp(ent->d_name, ".") != 0 && strcmp(ent->d_name, "..") != 0) {
                char dir_path[MAX_PATH_LENGTH];
                snprintf(dir_path, sizeof(dir_path), "%s/%s", home_dir, ent->d_name);
                get_birth_time(dir_path, &dirs[num_dirs]);
                strcpy(dirs[num_dirs].name, dir_path);
                num_dirs++;
            }
        }
        closedir(dir);
    } else {
        perror("opendir");
        exit(EXIT_FAILURE);
    }
    qsort(dirs, num_dirs, sizeof(struct Directory), compare_directories); // Sort directories by birth time

    memset(outputBuff, 0, sizeof(outputBuff)); // Clear the output buffer
    for (int i = 0; i < num_dirs; i++) {
        snprintf(outputBuff + strlen(outputBuff), sizeof(outputBuff) - strlen(outputBuff), "%s - %s", dirs[i].name, ctime(&dirs[i].birth_time));
    }
}

// Function to compare directory names for sorting
int compare_dir_names(const void *a, const void *b) { // Function to compare directory names for sorting
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

int send_file_to_client(const char *file_name, int socket_fd) { // Function to send a file to the client
    printf("Sending file: %s\n", file_name);
    int file_fd;
    ssize_t bytes_read, bytes_written;
    char buffer[OUTPUT_BUFF_SIZE];
    void *p;
    struct stat file_stat;

    // Open the file
    file_fd = open(file_name, O_RDONLY);
    if (file_fd < 0) {
        char *msg = "No file found";
        printf("No file found\n");
        // printf("File size:- %ld\n", strlen(msg));
        int bt_written = write(socket_fd, msg, strlen(msg));
        // printf("Bytes written: %d\n", bt_written);
        remove_directory("temp");
        return 0;
    } else{
    
    char *msg = "File is found";
    printf("File is found\n");
    printf("File size:- %d\n", strlen(msg));
    int bt_written = write(socket_fd, msg, strlen(msg));
    
    printf("Bytes written: %d\n", bt_written);

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
    }
    return 0;
}

void crequest(int newsockfd) { // Function to handle client requests
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
            if (strncmp("dirlist -a", outputBuff, 10) == 0) { // Handle dirlist -a command
                // executeCommand("ls");
                const char* HOME_PATH = getenv("HOME");
                list_directories(HOME_PATH);
                write(newsockfd, outputBuff, strlen(outputBuff));
            } else if (strncmp("dirlist -t", outputBuff, 10) == 0) { // Handle dirlist -t command
                const char* HOME_PATH = getenv("HOME");
                list_directories_by_birth_time(HOME_PATH);
                write(newsockfd, outputBuff, strlen(outputBuff));
            } 
            else if (strncmp("w24fn",cmd, 5) == 0){ // Handle w24fn command
                char *fileName = strtok(NULL, " ");
                handle_search_file(fileName);
                printf("Output buffer: %s\n", outputBuff);
                write(newsockfd, outputBuff, strlen(outputBuff));
            }
            else if (strncmp("w24fz",cmd, 5) == 0){ // Handle w24fz command
                off_t size1 = atoll(strtok(NULL, " "));
                off_t size2 = atoll(strtok(NULL, " "));
                get_tar_w24fz(size1,size2);
                char *filename = "temp.tar.gz";
                send_file_to_client(filename, newsockfd);
            } else if (strncmp("w24ft",cmd, 5) == 0){ // Handle w24ft command
                char *ext[3]; // Adjust size as needed
                int arg_count = 0;
                char *token;
                while ((token = strtok(NULL, " ")) != NULL) {
                    ext[arg_count++] = strdup(token); // Copy the argument to the array
                }
                const char *extensions[4];
                // const char *extensions[arg_count+1]; // Adjust size as needed
                for (int i = 0; i < arg_count; i++) {
                    extensions[i] = ext[i]; // Initialize extensions array with values from ext
                }
                for (int i = 0; i < arg_count; i++) {
                    printf("Extension: %s\n", extensions[i]);
                }
                get_tar_w24ft(extensions,arg_count+1);
                printf("Sending file to client\n");
                char *filename = "temp.tar.gz";
                send_file_to_client(filename, newsockfd);
                for (int i = 0; i < arg_count; i++) {
                    free(ext[i]);
                }
            }else if (strncmp("w24fdb",cmd, 6) == 0){ // Handle w24fdb command
                char *date = strtok(NULL, " ");
                get_tar_w24fdb(date);
                char *filename = "temp.tar.gz";
                send_file_to_client(filename, newsockfd);
            }else if (strncmp("w24fda",cmd, 6) == 0){ // Handle w24fda command
                char *date = strtok(NULL, " ");
                get_tar_w24fda(date);
                char *filename = "temp.tar.gz";
                send_file_to_client(filename, newsockfd);
            }
            // Check if the command is "quitc" and exit the process
            else if (strncmp("quitc", outputBuff, 5) == 0) { // Handle quitc command
                printf("Quit command received. Exiting child process.\n");
                exit(0); // Explicitly exit the child process
            } 
            else { // Handle invalid command
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
    struct sockaddr_in serv_addr, cli_addr; // Server and client address structures

    sockfd = socket(AF_INET, SOCK_STREAM, 0); // Create a new socket
    if(sockfd < 0) error("Error opening socket.");

    bzero((char *) &serv_addr, sizeof(serv_addr)); // Clear the server address structure
    portno = atoi(argv[1]); //  Convert the port number from string to integer

    serv_addr.sin_family = AF_INET; // Set the address family
    serv_addr.sin_addr.s_addr = INADDR_ANY; //  Set the address to accept any incoming messages
    serv_addr.sin_port = htons(portno); //  Convert the port number to network byte order

    if(bind(sockfd, (struct sockaddr *) &serv_addr, sizeof(serv_addr)) < 0) // Bind the socket to the address and port number
        error("Binding failed.");

    listen(sockfd, 5); // Listen for incoming connections
    clilen = sizeof(cli_addr); // Get the size of the client address

    while(1) {
        newsockfd = accept(sockfd, (struct sockaddr *) &cli_addr, &clilen); // Accept a new connection
        if(newsockfd < 0) error("Error on accept.");
        printf("Inside mirror-2\n");
        int pid = fork();
        if(pid < 0) {
            error("Error on fork.");
        }

        if(pid == 0) { // Child process
            close(sockfd);
            crequest(newsockfd); // Handle client requests
            exit(0);
        }
        else {
            close(newsockfd);
            signal(SIGCHLD, SIG_IGN); // Prevent zombie processes
        }
    }

    close(sockfd);
    return 0;
}