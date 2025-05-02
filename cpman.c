#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <unistd.h>
#include <dirent.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <limits.h>
#include <time.h>
#include <errno.h>
#include <fcntl.h>
#include <libgen.h>
#include "cpman.h"

char COMPOSE_CMD[256] = {0};
char DOCKER_CMD[256] = {0};
char **compose_files = NULL;
int compose_file_count = 0;
char *exclude_pattern = NULL;
int verbose_mode = 0;
int timeout_seconds = 60;
int max_depth = 2;

int main(int argc, char *argv[]) {
    signal(SIGINT, signal_handler);

    int mode = 0;
    char *path = NULL;
    char *exclude = NULL;

    if (!parse_args(argc, argv, &mode, &path, &exclude)) {
        return 1;
    }

    exclude_pattern = exclude;

    if (argc > 1 && strcmp(argv[1], "--help") == 0) {
        print_help();
        return 0;
    }

    if (path && chdir(path) != 0) {
        perror("Failed to change directory");
        return 1;
    }

    check_command();
    find_compose_files();

    if (compose_file_count == 0) {
        printf(YELLOW "No compose files found. Did you specify the correct path? Try adjusting the depth with -d option.\n" NC);
        return 1;
    }

    main_menu(mode);

    free_compose_files();

    return 0;
}

void main_menu(int mode) {
    if (mode == 1) {
        pause_all_compose();
        return;
    } else if (mode == 2) {
        start_all_compose();
        return;
    } else if (mode == 3) {
        update_compose_files();
        return;
    }

    printf(YELLOW "Please select an option:\n" NC);
    printf(GREEN "1) Stop all compose services\n" NC);
    printf(GREEN "2) Start all compose services\n" NC);
    printf(GREEN "3) Update all compose services (default)\n" NC);

    char choice[10];
    fgets(choice, sizeof(choice), stdin);

    if (choice[0] == '1') {
        pause_all_compose();
    } else if (choice[0] == '2') {
        start_all_compose();
    } else {
        update_compose_files();
    }
}

int execute_command_with_timeout(const char *command, char *output, size_t output_size, const char *work_dir) {
    if (verbose_mode) {
        if (work_dir) {
            printf(CYAN "Executing in %s: %s\n" NC, work_dir, command);
        } else {
            printf(CYAN "Executing: %s\n" NC, command);
        }
    }

    char original_dir[PATH_MAX];
    if (work_dir) {
        if (!getcwd(original_dir, sizeof(original_dir))) {
            perror("Failed to get current directory");
            return -1;
        }

        if (chdir(work_dir) != 0) {
            perror("Failed to change to working directory");
            return -1;
        }
    }

    char temp_file[] = "/tmp/cpman_output_XXXXXX";
    int fd = mkstemp(temp_file);
    if (fd == -1) {
        perror("Failed to create temporary file");
        if (work_dir) chdir(original_dir);
        return -1;
    }

    close(fd);

    char redirect_cmd[2048];
    snprintf(redirect_cmd, sizeof(redirect_cmd), "%s > %s 2>&1", command, temp_file);

    pid_t pid = fork();
    if (pid == -1) {
        perror("Failed to fork");
        unlink(temp_file);
        if (work_dir) chdir(original_dir);
        return -1;
    }

    if (pid == 0) {
        execl("/bin/sh", "sh", "-c", redirect_cmd, NULL);
        exit(1);
    }

    time_t start_time = time(NULL);
    int status;
    pid_t result;
    int timed_out = 0;

    while (1) {
        result = waitpid(pid, &status, WNOHANG);
        if (result == pid) {
            break;
        } else if (result == -1) {
            perror("waitpid failed");
            kill(pid, SIGKILL);
            unlink(temp_file);
            if (work_dir) chdir(original_dir);
            return -1;
        }

        if (time(NULL) - start_time > timeout_seconds) {
            printf(RED "\nCommand timed out after %d seconds. Show output? [y/N]: " NC, timeout_seconds);
            fflush(stdout);

            char response[10];
            fgets(response, sizeof(response), stdin);
            if (response[0] == 'y' || response[0] == 'Y') {
                FILE *fp = fopen(temp_file, "r");
                if (fp) {
                    char buffer[256];
                    printf(YELLOW "\n--- Command Output ---\n" NC);
                    while (fgets(buffer, sizeof(buffer), fp) != NULL) {
                        printf("%s", buffer);
                    }
                    printf(YELLOW "\n--- End Output ---\n" NC);
                    fclose(fp);
                }
            }

            printf(YELLOW "Terminate the process? [Y/n]: " NC);
            fgets(response, sizeof(response), stdin);
            if (response[0] != 'n' && response[0] != 'N') {
                kill(pid, SIGTERM);
                sleep(1);
                kill(pid, SIGKILL);
                timed_out = 1;
                break;
            } else {
                printf(YELLOW "Continuing to wait for the process to complete...\n" NC);
                start_time = time(NULL);
            }
        }

        usleep(100000);
    }

    FILE *fp = fopen(temp_file, "r");
    if (fp) {
        if (verbose_mode) {
            char buffer[256];
            printf(YELLOW "\n--- Command Output ---\n" NC);
            while (fgets(buffer, sizeof(buffer), fp) != NULL) {
                printf("%s", buffer);
            }
            printf(YELLOW "\n--- End Output ---\n" NC);

            rewind(fp);
        }

        if (output && output_size > 0) {
            size_t bytes_read = fread(output, 1, output_size - 1, fp);
            output[bytes_read] = '\0';
        }

        fclose(fp);
    } else if (output && output_size > 0) {
        output[0] = '\0';
    }

    unlink(temp_file);

    if (work_dir) {
        if (chdir(original_dir) != 0) {
            perror("Failed to return to original directory");
        }
    }

    if (timed_out) {
        return -2;
    }

    int exit_code = WIFEXITED(status) ? WEXITSTATUS(status) : -1;
    if (verbose_mode) {
        printf(CYAN "Command exited with code: %d\n" NC, exit_code);
    }

    return exit_code;
}

char *get_image_id(const char *file) {
    char *file_copy = strdup(file);
    if (!file_copy) {
        perror("Failed to allocate memory");
        return NULL;
    }

    char *dir = dirname(strdup(file_copy));
    char *base_filename = basename(file_copy);

    char command[1024];
    char original_dir[PATH_MAX];

    if (!getcwd(original_dir, sizeof(original_dir))) {
        perror("Failed to get current directory");
        free(file_copy);
        free(dir);
        return NULL;
    }

    if (chdir(dir) != 0) {
        perror("Failed to change to compose file directory");
        free(file_copy);
        free(dir);
        return NULL;
    }

    snprintf(command, sizeof(command), "%s -f \"%s\" config | grep 'image:' | awk '{print $2}'",
             COMPOSE_CMD, base_filename);

    FILE *fp = popen(command, "r");
    if (!fp) {
        chdir(original_dir);
        free(file_copy);
        free(dir);
        return NULL;
    }

    char images[4096] = {0};
    char line[256];
    while (fgets(line, sizeof(line), fp) != NULL) {
        strcat(images, line);
    }
    pclose(fp);

    if (chdir(original_dir) != 0) {
        perror("Failed to return to original directory");
    }

    free(file_copy);
    free(dir);

    char digests[8192] = {0};

    char *image = strtok(images, "\n");
    while (image != NULL) {
        snprintf(command, sizeof(command), "%s image inspect --format='{{index .RepoDigests 0}}' \"%s\" 2>/dev/null",
                DOCKER_CMD, image);

        fp = popen(command, "r");
        if (!fp) {
            fprintf(stderr, "Failed to run command: %s\n", command);
            return NULL;
        }

        char digest[512] = {0};
        if (fgets(digest, sizeof(digest), fp) == NULL) {
            strcpy(digest, image);
        }

        strtok(digest, "\n");

        strcat(digests, digest);
        strcat(digests, "\n");

        pclose(fp);

        image = strtok(NULL, "\n");
    }

    char sorted_digests[8192];
    snprintf(command, sizeof(command), "echo \"%s\" | sort", digests);
    fp = popen(command, "r");
    if (!fp) return NULL;

    sorted_digests[0] = '\0';
    while (fgets(line, sizeof(line), fp) != NULL) {
        strcat(sorted_digests, line);
    }
    pclose(fp);

    char md5_command[16384];
    snprintf(md5_command, sizeof(md5_command), "echo \"%s\" | md5sum", sorted_digests);
    fp = popen(md5_command, "r");
    if (!fp) return NULL;

    static char md5sum[33];
    if (fgets(line, sizeof(line), fp) != NULL) {
        sscanf(line, "%32s", md5sum);
    }
    pclose(fp);

    return md5sum;
}

void update_compose_files() {
    char before_pull[33];
    char after_pull[33];
    char output_buffer[4096];

    for (int i = 0; i < compose_file_count; i++) {
        const char *compose_file = compose_files[i];
        printf(CYAN "Updating %s...\n" NC, compose_file);

        char *file_copy = strdup(compose_file);
        if (!file_copy) {
            perror("Failed to allocate memory");
            continue;
        }
        char *compose_dir = dirname(file_copy);

        char *image_id = get_image_id(compose_file);
        if (image_id) {
            strncpy(before_pull, image_id, 32);
            before_pull[32] = '\0';
        } else {
            printf(RED "Failed to get image digest before pull\n" NC);
            free(file_copy);
            continue;
        }

        char pull_command[1024];
        snprintf(pull_command, sizeof(pull_command), "%s -f \"%s\" pull", COMPOSE_CMD, compose_file);

        printf(YELLOW "Pulling images (timeout: %d seconds)...\n" NC, timeout_seconds);
        int result = execute_command_with_timeout(pull_command, output_buffer, sizeof(output_buffer), compose_dir);

        if (result == -2) {
            printf(RED "Pull command timed out.\n" NC);
            free(file_copy);
            continue;
        } else if (result != 0) {
            printf(RED "Pull command failed with exit code %d.\n" NC, result);
            free(file_copy);
            continue;
        }

        image_id = get_image_id(compose_file);
        if (image_id) {
            strncpy(after_pull, image_id, 32);
            after_pull[32] = '\0';
        } else {
            printf(RED "Failed to get image digest after pull\n" NC);
            free(file_copy);
            continue;
        }

        if (strcmp(before_pull, after_pull) != 0) {
            printf(GREEN "New images pulled, restarting service...\n" NC);

            char down_command[1024];
            snprintf(down_command, sizeof(down_command), "%s -f \"%s\" down", COMPOSE_CMD, compose_file);

            result = execute_command_with_timeout(down_command, output_buffer, sizeof(output_buffer), compose_dir);
            if (result != 0 && result != -2) {
                printf(RED "Down command failed with exit code %d.\n" NC, result);
                free(file_copy);
                continue;
            }

            char up_command[1024];
            snprintf(up_command, sizeof(up_command), "%s -f \"%s\" up -d", COMPOSE_CMD, compose_file);

            result = execute_command_with_timeout(up_command, output_buffer, sizeof(output_buffer), compose_dir);
            if (result != 0 && result != -2) {
                printf(RED "Up command failed with exit code %d.\n" NC, result);
                free(file_copy);
                continue;
            }

            printf(GREEN "Service restarted.\n" NC);
        } else {
            printf(YELLOW "No new images, skipping restart.\n" NC);
        }

        free(file_copy);
    }
}

void pause_all_compose() {
    char output_buffer[4096];

    for (int i = 0; i < compose_file_count; i++) {
        const char *compose_file = compose_files[i];
        printf(CYAN "Stopping services in %s...\n" NC, compose_file);

        char *file_copy = strdup(compose_file);
        if (!file_copy) {
            perror("Failed to allocate memory");
            continue;
        }
        char *compose_dir = dirname(file_copy);

        char down_command[1024];
        snprintf(down_command, sizeof(down_command), "%s -f \"%s\" down", COMPOSE_CMD, compose_file);

        int result = execute_command_with_timeout(down_command, output_buffer, sizeof(output_buffer), compose_dir);
        if (result != 0 && result != -2) {
            printf(RED "Down command failed with exit code %d.\n" NC, result);
            free(file_copy);
            continue;
        }

        printf(BLUE "Services stopped.\n" NC);
        free(file_copy);
    }
}

void start_all_compose() {
    char output_buffer[4096];

    for (int i = 0; i < compose_file_count; i++) {
        const char *compose_file = compose_files[i];
        printf(CYAN "Starting services in %s...\n" NC, compose_file);

        char *file_copy = strdup(compose_file);
        if (!file_copy) {
            perror("Failed to allocate memory");
            continue;
        }
        char *compose_dir = dirname(file_copy);

        char up_command[1024];
        snprintf(up_command, sizeof(up_command), "%s -f \"%s\" up -d", COMPOSE_CMD, compose_file);

        int result = execute_command_with_timeout(up_command, output_buffer, sizeof(output_buffer), compose_dir);
        if (result != 0 && result != -2) {
            printf(RED "Up command failed with exit code %d.\n" NC, result);
            free(file_copy);
            continue;
        }

        printf(GREEN "Services started.\n" NC);
        free(file_copy);
    }
}

int is_valid_compose_file(const char *filepath) {
    FILE *fp = fopen(filepath, "r");
    if (!fp) return 0;

    char buffer[4096];
    size_t bytes_read = fread(buffer, 1, sizeof(buffer) - 1, fp);
    buffer[bytes_read] = '\0';
    fclose(fp);

    if (strstr(buffer, "services:") || strstr(buffer, "version:")) {
        return 1;
    }

    return 0;
}

void signal_handler(int sig) {
    (void)sig;
    printf(RED "\nExiting\n" NC);
    free_compose_files();
    exit(1);
}

void check_command() {
    FILE *fp;
    char docker_path[256] = {0};
    char compose_path[256] = {0};

    fp = popen("which docker", "r");
    if (fp != NULL && fgets(docker_path, sizeof(docker_path), fp) != NULL) {
        docker_path[strcspn(docker_path, "\n")] = 0;
        pclose(fp);

        if (access(docker_path, X_OK) == 0)
        {
            char command[512];
            snprintf(command, sizeof(command), "%s compose version > /dev/null 2>&1", docker_path);
            if (system(command) == 0) {
                strcpy(COMPOSE_CMD, "docker compose");
                strcpy(DOCKER_CMD, docker_path);
                printf(GREEN "Using docker compose\n" NC);
                return;
            }
        }
    }

    fp = popen("which podman-compose", "r");
    if (fp != NULL && fgets(compose_path, sizeof(compose_path), fp) != NULL) {
        compose_path[strcspn(compose_path, "\n")] = 0;
        pclose(fp);
        if (access(compose_path, X_OK) == 0) {
            fp = popen("which podman", "r");
            char podman_path[256];
            if (fp != NULL && fgets(podman_path, sizeof(podman_path), fp) != NULL) {
                podman_path[strcspn(podman_path, "\n")] = 0;
                pclose(fp);
                if (access(podman_path, X_OK) == 0) {
                    strcpy(COMPOSE_CMD, compose_path);
                    strcpy(DOCKER_CMD, podman_path);
                    printf(GREEN "Using podman-compose\n" NC);
                    return;
                }
            }
            if (fp) pclose(fp);
        }
    }
    if (fp) pclose(fp);

    printf(YELLOW "Trying alternative commands...\n" NC);

    if (system("command -v docker-compose > /dev/null 2>&1") == 0) {
        fp = popen("which docker", "r");
        if (fp != NULL && fgets(docker_path, sizeof(docker_path), fp) != NULL) {
            docker_path[strcspn(docker_path, "\n")] = 0;
            pclose(fp);
            if (access(docker_path, X_OK) == 0) {
                strcpy(COMPOSE_CMD, "docker-compose");
                strcpy(DOCKER_CMD, docker_path);
                printf(GREEN "Using docker-compose\n" NC);
                return;
            }
        }
        if (fp) pclose(fp);
    }

    printf(RED "No compatible compose command found.\n" NC);
    exit(1);
}

void find_compose_files() {
    compose_files = malloc(sizeof(char *) * 1000);
    if (!compose_files) {
        fprintf(stderr, RED "Memory allocation failed\n" NC);
        exit(1);
    }

    compose_file_count = 0;
    traverse_directories(".", 0);

    if (compose_file_count > 0) {
        printf(YELLOW "Found %d compose files", compose_file_count);

        if (exclude_pattern && *exclude_pattern) {
            printf(" (excluding '%s')", exclude_pattern);
        } else {
            printf(" (ignored directories containing 'ignore')");
        }

        printf(":\n" NC);

        for (int i = 0; i < compose_file_count; i++) {
            printf("%s\n", compose_files[i]);
        }

        for (int i = 0; i < compose_file_count; i++) {
            char abs_path[PATH_MAX];
            if (realpath(compose_files[i], abs_path) != NULL) {
                char *temp = strdup(abs_path);
                if (!temp) {
                    fprintf(stderr, RED "Memory allocation failed\n" NC);
                    exit(1);
                }
                free(compose_files[i]);
                compose_files[i] = temp;
            } else {
                fprintf(stderr, RED "Error converting path to absolute: %s\n" NC, compose_files[i]);
            }
        }
    } else {
        printf(YELLOW "No compose files found.\n" NC);
    }
}

void traverse_directories(const char *base_path, int depth) {
    if (depth > max_depth) return;
    DIR *dir;
    struct dirent *entry;

    if (!(dir = opendir(base_path))) return;

    while ((entry = readdir(dir)) != NULL) {
        char path[1024];
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0)
            continue;
        snprintf(path, sizeof(path), "%s/%s", base_path, entry->d_name);

        if (exclude_pattern && *exclude_pattern && strstr(path, exclude_pattern) != NULL) {
            continue;
        }

        struct stat statbuf;
        if (stat(path, &statbuf) == -1) continue;

        if (S_ISDIR(statbuf.st_mode)) {
            if (strstr(entry->d_name, "ignore") != NULL) continue;
            traverse_directories(path, depth + 1);
        } else {
            if (compose_file_count >= 1000) {
                fprintf(stderr, YELLOW "Warning: Reached maximum compose file limit (1000)\n" NC);
                break;
            }

            if (strcmp(entry->d_name, "compose.yaml") == 0 ||
                strcmp(entry->d_name, "compose.yml") == 0 ||
                strcmp(entry->d_name, "docker-compose.yaml") == 0 ||
                strcmp(entry->d_name, "docker-compose.yml") == 0) {

                if (is_valid_compose_file(path)) {
                    compose_files[compose_file_count] = strdup(path);
                    if (!compose_files[compose_file_count]) {
                        fprintf(stderr, RED "Memory allocation failed\n" NC);
                        closedir(dir);
                        exit(1);
                    }
                    compose_file_count++;
                } else if (verbose_mode) {
                    printf(YELLOW "Skipping non-compose file: %s\n" NC, path);
                }
            }
        }
    }
    closedir(dir);
}

void free_compose_files() {
    if (!compose_files) return;

    for (int i = 0; i < compose_file_count; i++) {
        free(compose_files[i]);
    }
    free(compose_files);
    compose_files = NULL;
}

void print_help() {
    printf(CYAN "+----------------------------------+\n" NC);
    printf(CYAN "|              " YELLOW "cpman" CYAN "               |\n" NC);
    printf(CYAN "|    " GREEN "Compose Project Manager" CYAN "       |\n" NC);
    printf(CYAN "+----------------------------------+\n\n" NC);

    printf(YELLOW "Usage:" NC "  cpman [OPTIONS]\n\n");

    printf(YELLOW "Options:\n" NC);
    printf("  " GREEN "-p PATH" NC "  Search path for compose files\n");
    printf("  " GREEN "-m MODE" NC "  Operation mode:\n");
    printf("           " BLUE "1" NC ": Stop, " BLUE "2" NC ": Start, " BLUE "3" NC ": Update (default)\n");
    printf("  " GREEN "-e, --exclude PATTERN" NC " Exclude files/directories matching PATTERN\n");
    printf("  " GREEN "-t, --timeout SECONDS" NC " Set command timeout (default: 60 seconds)\n");
    printf("  " GREEN "-d, --depth LEVEL" NC " Set maximum directory search depth (default: 2)\n");
    printf("  " GREEN "-v, --verbose" NC " Show command output on errors\n");
    printf("  " GREEN "--help" NC "    Show this help message\n\n");

    printf(YELLOW "Description:\n" NC);
    printf("  Manages Docker Compose projects in the specified\n");
    printf("  directory and its subdirectories.\n\n");

    printf(YELLOW "Example:\n" NC);
    printf("  cpman -p /projects -m 2 -t 120 -d 1\n\n");

    printf(YELLOW "Note:" NC " Directories containing 'ignore' are skipped.\n");
}

int parse_args(int argc, char *argv[], int *mode, char **path, char **exclude) {
    *mode = 3;
    *path = NULL;
    *exclude = NULL;

    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "-m") == 0 || strcmp(argv[i], "--mode") == 0) {
            if (i + 1 < argc) {
                *mode = atoi(argv[++i]);
                if (*mode < 1 || *mode > 3) {
                    fprintf(stderr, "Invalid mode: %d\n", *mode);
                    print_help();
                    return 0;
                }
            } else {
                fprintf(stderr, "Missing value for %s\n", argv[i]);
                print_help();
                return 0;
            }
        } else if (strcmp(argv[i], "-p") == 0 || strcmp(argv[i], "--path") == 0) {
            if (i + 1 < argc) {
                *path = argv[++i];
            } else {
                fprintf(stderr, "Missing value for %s\n", argv[i]);
                print_help();
                return 0;
            }
        } else if (strcmp(argv[i], "-e") == 0 || strcmp(argv[i], "--exclude") == 0) {
            if (i + 1 < argc) {
                *exclude = argv[++i];
            } else {
                fprintf(stderr, "Missing value for %s\n", argv[i]);
                print_help();
                return 0;
            }
        } else if (strcmp(argv[i], "-t") == 0 || strcmp(argv[i], "--timeout") == 0) {
            if (i + 1 < argc) {
                timeout_seconds = atoi(argv[++i]);
                if (timeout_seconds <= 0) {
                    fprintf(stderr, "Invalid timeout value: %d\n", timeout_seconds);
                    print_help();
                    return 0;
                }
            } else {
                fprintf(stderr, "Missing value for %s\n", argv[i]);
                print_help();
                return 0;
            }
        } else if (strcmp(argv[i], "-d") == 0 || strcmp(argv[i], "--depth") == 0) {
            if (i + 1 < argc) {
                max_depth = atoi(argv[++i]);
                if (max_depth < 0 || max_depth > 10) {
                    fprintf(stderr, "Invalid depth value: %d (must be between 0 and 10)\n", max_depth);
                    print_help();
                    return 0;
                }
            } else {
                fprintf(stderr, "Missing value for %s\n", argv[i]);
                print_help();
                return 0;
            }
        } else if (strcmp(argv[i], "-v") == 0 || strcmp(argv[i], "--verbose") == 0) {
            verbose_mode = 1;
        } else if (strcmp(argv[i], "--help") == 0) {
            print_help();
            exit(0);
        } else {
            fprintf(stderr, "Unknown option: %s\n", argv[i]);
            print_help();
            return 0;
        }
    }

    return 1;
}

