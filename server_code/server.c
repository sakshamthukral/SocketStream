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
// Function to execute the stat command and retrieve output

time_t target_date;
char temp_dir[MAX_PATH_LENGTH];
//------------------------ w24fda ------------------------
void get_tar_w24fda(const char *date){
    printf("Inside get_tar_w24fds\n");
    struct tm tm = {0};
    strptime(date, "%Y-%m-%d", &tm);
    target_date = mktime(&tm);

    // Create temp directory
    create_temp_dir_w24fd();

    // Search files and copy them to temp directory
    const char *home_dir = getenv("HOME");
    if (home_dir == NULL) {
        fprintf(stderr, "Failed to get user's home directory\n");
        return;
    }

    search_files_w24fda(home_dir);
    // Create temp.tar.gz archive
    const char *temp_folder = "temp";

    create_tar_gz(temp_folder);
     // To remove temp after tar creation
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
            continue; // Failed to get file status
        }

        if (S_ISDIR(statbuf.st_mode)) {
            // Skip the 'temp' directory
            if (strcmp(entry->d_name, temp_folder) == 0 && strcmp(path, getcwd(NULL, 0))==0) {
                continue;
            }
            search_files_w24fda(full_path); // Recursively search subdirectories
        } else if (S_ISREG(statbuf.st_mode)) {
            // Try to get birth time
            char *stat_output = file_birth_time(full_path);
            if (stat_output != NULL) {
                time_t birth_time = parse_birth_time_w24fd(stat_output);
                free(stat_output); // Free allocated memory
                if (birth_time >= target_date) {
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
time_t parse_birth_time_w24fd(const char *stat_output) {
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

// Function to create temp directory in the home directory
void create_temp_dir_w24fd() {
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

// Function to search for files in the directory tree rooted at the specified path
void search_files_w24fdb(const char *path) {
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
                    printf("birtht time: %ld  | target_date: %ld\n", birth_time, target_date);
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

void get_tar_w24fdb(const char *date){
    // Parse target date
    printf("Inside get_tar_w24fdb\n");
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

    search_files_w24fdb(home_dir);

    // Create temp.tar.gz archive
    const char *temp_folder = "temp";
    create_tar_gz(temp_folder);

    // Remove temp directory after tar creation
    if (remove_directory(temp_folder) == -1) {
        fprintf(stderr, "Failed to remove existing temp folder after tar creation\n");
        return;
    }
}
// ----------------- w24ft -----------------

int is_file_of_type_w24ft(const char *filename, const char *extension) {
    printf("Filename: %s\n", filename);
    printf("Extension: %s\n", extension);
    size_t filename_len = strlen(filename);
    size_t extension_len = strlen(extension);
    if (filename_len < extension_len + 1)
        return 0;
    return strcmp(filename + filename_len - extension_len, extension) == 0;
}

void search_directory_w24ft(const char *dir_path, const char *extensions[], int num_extensions, int *found, const char *temp_dir) {
    DIR *dir;
    struct dirent *entry;
    char full_path[MAX_PATH_LENGTH];

    if ((dir = opendir(dir_path)) == NULL) {
        perror("opendir");
        exit(EXIT_FAILURE);
    }
    printf("Extensions: %d\n", num_extensions);
    for (int i = 0; i < num_extensions; i++) {
        printf("Extension: %s\n", extensions[i]);
    }
    while ((entry = readdir(dir)) != NULL) {
        if (entry->d_type == DT_DIR && strcmp(entry->d_name, ".") != 0 && strcmp(entry->d_name, "..") != 0) {
            // Skip the 'temp' directory
            if (strcmp(entry->d_name, temp_dir) == 0 && strcmp(dir_path, getcwd(NULL, 0))==0) {
                continue;
            }

            snprintf(full_path, sizeof(full_path), "%s/%s", dir_path, entry->d_name);
            search_directory_w24ft(full_path, extensions, num_extensions, found, temp_dir);
        } else if (entry->d_type == DT_REG) {
            for (int i = 0; i < num_extensions; ++i) {
                if (is_file_of_type_w24ft(entry->d_name, extensions[i])) {
                    // printf("%s/%s\n", dir_path, entry->d_name);
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

void create_temp_dir() {
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

void get_tar_w24ft(const char *extensions[],int argc)
{ // print extensions
    // printf("inside get taeargc: %d\n", argc);
    // for (int i = 0; i < argc ; ++i) {
    //     printf("Extension: %s\n", extensions[i]);
    //     printf("Length of extension: %ld\n", strlen(extensions[i]));
    // }

    const char *temp_folder = "temp";

    //create_temp_directory(temp_dir_path);
    printf("Before creating temp directory\n");
    create_temp_dir();
    printf("After creating temp directory\n");
    const char *home_dir = getenv("HOME");
    printf("Home directory: %s\n", home_dir);
    if (home_dir == NULL) {
        fprintf(stderr, "Failed to get user's home directory\n");
        return;
    }


    int found = 0;
    printf("-------------------------------------\n");
    printf("Home directory: %s\n", home_dir);
    printf("Extensions: %d\n", argc - 1);
    for (int i = 0; i < argc - 1; ++i) {
        printf("Extension: %s\n", extensions[i]);
        printf("length of extensions: %ld\n", strlen(extensions[i]));
    }
    search_directory_w24ft(home_dir, extensions, argc - 1, &found, temp_folder);

    if (!found) {
        printf("No files found\n");
        return;
    }
//  const char *temp_folder = "temp";
    // Create tar file of the temp folder
    create_tar_gz(temp_folder);
    // To remove temp after tar creation

    if (remove_directory(temp_folder) == -1) {
    fprintf(stderr, "Failed to remove existing temp folder after tar creation\n");
    return;
    }
}
// ----------------- w24fz -----------------

void search_files_w24fz(const char *path, off_t size1, off_t size2, const char *temp_folder) {
    DIR *dir;
    struct dirent *entry;
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

        if (S_ISDIR(statbuf.st_mode)) {
            // Skip the 'temp' directory
                if (strcmp(entry->d_name, temp_folder) == 0 && strcmp(path, getcwd(NULL, 0)) == 0){
                continue;
            }
            search_files_w24fz(full_path, size1, size2, temp_folder);
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
void copy_file_w24fz(const char *src, const char *dest) {
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
void create_tar_gz(const char *folder_path) {
    pid_t pid = fork();
    if (pid == -1) {
        perror("fork");
        return;
    }
    if (pid == 0) {
        // Child process
        execlp("tar", "tar", "-czf", "temp.tar.gz", folder_path, (char *)NULL);
        perror("execlp");
        exit(EXIT_FAILURE);
    } else {
        // Parent process
        int status;
        waitpid(pid, &status, 0);
        if (!WIFEXITED(status) || WEXITSTATUS(status) != 0) {
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
void get_tar_w24fz(off_t size1, off_t size2){
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

void remove_newline(char *str) {
    // printf("Removing newline character\n");
    printf("In here\n");
    size_t len = strlen(str);
    // Check and replace newline at the end of the string
    if (len > 0 && str[len - 1] == '\n') {
        str[len - 1] = '\0';
    }
    // Check and replace newline at the start of the string
    if (str[0] == '\n') {
        memmove(str, str + 1, len); // Move the string one position to the left
        str[len - 1] = '\0'; // Correctly terminate the string
    }
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
            if((strncmp("w24fz",cmd, 5) == 0) || (strncmp("w24ft",cmd, 5) == 0) || (strncmp("w24fdb",cmd, 6) == 0) || (strncmp("w24fda",cmd, 6) == 0)){
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

            strcpy(command, outputBuff);
            char *cmd = strtok(command, " ");
            if((strncmp("w24fz",cmd, 5) == 0) || (strncmp("w24ft",cmd, 5) == 0) || (strncmp("w24fdb",cmd, 6) == 0) || (strncmp("w24fda",cmd, 6) == 0)){
                write(sockfd_mirror2, outputBuff, strlen(outputBuff));
                char *filename = "temp.tar.gz";
                if (receive_file(sockfd_mirror2, filename) == 0) {
                    printf("File received and saved successfully.\n");
                } else {
                    printf("Failed to receive the file.\n");
                }
                printf("check filename: %s\n", filename);
                send_file_to_client(filename, newsockfd);
                // continue;
            } else {
                write(sockfd_mirror2, outputBuff, strlen(outputBuff));
                memset(outputBuff, 0, sizeof(outputBuff));
                n = read(sockfd_mirror2, outputBuff, sizeof(outputBuff)-1);
                write(newsockfd, outputBuff, n);
                if (n < 0) {
                    error("ERROR reading from socket");
                }
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
            if(outputBuff[strlen(outputBuff) - 1] == '\n') {
                printf("Removing trailing newline character\n");
                outputBuff[strlen(outputBuff) - 1] = '\0'; // Remove the trailing newline character
            }
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
                off_t size1 = atoll(strtok(NULL, " "));
                off_t size2 = atoll(strtok(NULL, " "));
                get_tar_w24fz(size1,size2);
                char *filename = "temp.tar.gz";
                send_file_to_client(filename, newsockfd);
            } else if (strncmp("w24ft",cmd, 5) == 0){
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
            } else if (strncmp("w24fdb",cmd, 6) == 0){
                char *date = strtok(NULL, " ");
                get_tar_w24fdb(date);
                char *filename = "temp.tar.gz";
                send_file_to_client(filename, newsockfd);
            }else if (strncmp("w24fda",cmd, 6) == 0){
                printf("In here\n");
                char *date = strtok(NULL, " ");
                get_tar_w24fda(date);
                char *filename = "temp.tar.gz";
                send_file_to_client(filename, newsockfd);
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
