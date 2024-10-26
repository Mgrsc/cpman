#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <unistd.h>
#include <dirent.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <limits.h>
#include "cpman.h"

char COMPOSE_CMD[256] = {0};
char DOCKER_CMD[256] = {0};
char **compose_files = NULL;
int compose_file_count = 0;
char *exclude_pattern = NULL;

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

void update_compose_files() {
    char before_pull[33];
    char after_pull[33];

    for (int i = 0; i < compose_file_count; i++) {
        const char *compose_file = compose_files[i];
        printf(CYAN "Updating %s...\n" NC, compose_file);

        strncpy(before_pull, get_image_id(compose_file), 33);
        before_pull[32] = '\0';

        char pull_command[1024];
        snprintf(pull_command, sizeof(pull_command), "%s -f \"%s\" pull > /dev/null 2>&1", COMPOSE_CMD, compose_file);

        pid_t pid = fork();
        if (pid == 0) {
            execl("/bin/sh", "sh", "-c", pull_command, NULL);
            exit(0);
        } else if (pid > 0) {
            loading_animation(pid);
            waitpid(pid, NULL, 0);
        } else {
            printf(RED "Failed to create process.\n" NC);
            exit(1);
        }

        strncpy(after_pull, get_image_id(compose_file), 33);
        after_pull[32] = '\0';

        if (strcmp(before_pull, after_pull) != 0) {
            printf(GREEN "New images pulled, restarting service...\n" NC);

            char down_command[1024];
            snprintf(down_command, sizeof(down_command), "%s -f \"%s\" down > /dev/null 2>&1", COMPOSE_CMD, compose_file);
            system(down_command);

            char up_command[1024];
            snprintf(up_command, sizeof(up_command), "%s -f \"%s\" up -d > /dev/null 2>&1", COMPOSE_CMD, compose_file);
            system(up_command);

            printf(GREEN "Service restarted.\n" NC);
        } else {
            printf(YELLOW "No new images, skipping restart.\n" NC);
        }
    }
}

void pause_all_compose() {
    for (int i = 0; i < compose_file_count; i++) {
        const char *compose_file = compose_files[i];
        printf(CYAN "Stopping services in %s...\n" NC, compose_file);

        char down_command[1024];
        snprintf(down_command, sizeof(down_command), "%s -f \"%s\" down > /dev/null 2>&1", COMPOSE_CMD, compose_file);

        system(down_command);

        printf(BLUE "Services stopped.\n" NC);
    }
}

void start_all_compose() {
    for (int i = 0; i < compose_file_count; i++) {
        const char *compose_file = compose_files[i];
        printf(CYAN "Starting services in %s...\n" NC, compose_file);

        char up_command[1024];
        snprintf(up_command, sizeof(up_command), "%s -f \"%s\" up -d > /dev/null 2>&1", COMPOSE_CMD, compose_file);

        system(up_command);

        printf(GREEN "Services started.\n" NC);
    }
}

char *get_image_id(const char *file) {
    char command[1024];
    snprintf(command, sizeof(command), "%s -f \"%s\" config | grep 'image:' | awk '{print $2}'", COMPOSE_CMD, file);

    FILE *fp = popen(command, "r");
    if (!fp) return NULL;

    char images[4096] = {0};
    char line[256];
    while (fgets(line, sizeof(line), fp) != NULL) {
        strcat(images, line);
    }
    pclose(fp);

    char digests[8192] = {0};

    char *image = strtok(images, "\n");
    while (image != NULL) {
        snprintf(command, sizeof(command), "%s image inspect --format='{{index .RepoDigests 0}}' \"%s\" 2>/dev/null", DOCKER_CMD, image);

        fp = popen(command, "r");
        if (!fp) {
            fprintf(stderr, "Failed to run command: %s\n", command);
            return NULL;
        }

        char digest[512] = {0};
        if (fgets(digest, sizeof(digest), fp) == NULL) {
            strcpy(digest, image); // Use image name if no digest found
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

void loading_animation(pid_t pid) {
    const char *spin = "|/-\\";
    int i = 0;
    while (1) {
        int status;
        pid_t result = waitpid(pid, &status, WNOHANG);
        if (result != 0) break;
        printf("\r%c Pulling...", spin[i % 4]);
        fflush(stdout);
        i++;
        usleep(100000); // Sleep for 0.1 seconds
    }
    printf("\r               \r"); // Clear the line
}


void signal_handler(int sig) {
    (void)sig;
    printf(RED "\nExiting\n" NC);
    free_compose_files();
    exit(1);
}

void check_command() {
    if (access("/usr/bin/docker", X_OK) == 0 &&
        system("/usr/bin/docker compose version > /dev/null 2>&1") == 0) {
        strcpy(COMPOSE_CMD, "/usr/bin/docker compose");
        strcpy(DOCKER_CMD, "/usr/bin/docker");
        printf(GREEN "Using docker compose\n" NC);
    } else if (access("/usr/local/bin/podman-compose", X_OK) == 0 &&
               access("/usr/bin/podman", X_OK) == 0) {
        strcpy(COMPOSE_CMD, "/usr/local/bin/podman-compose");
        strcpy(DOCKER_CMD, "/usr/bin/podman");
        printf(GREEN "Using podman-compose\n" NC);
    } else {
        printf(RED "No compatible compose command found.\n" NC);
        exit(1);
    }
}

void find_compose_files() {
    compose_files = malloc(sizeof(char *) * 1000);
    compose_file_count = 0;
    traverse_directories(".", 0);

    printf(YELLOW "Found %d compose files (ignored directories containing 'ignore'):\n" NC, compose_file_count);

    for (int i = 0; i < compose_file_count; i++) {
        printf("%s\n", compose_files[i]);
    }

    for (int i = 0; i < compose_file_count; i++) {
        char abs_path[PATH_MAX];
        if (realpath(compose_files[i], abs_path) != NULL) {
            char *temp = strdup(abs_path);
            free(compose_files[i]);
            compose_files[i] = temp;
        } else {
            fprintf(stderr, RED "Error converting path to absolute: %s\n" NC, compose_files[i]);
        }
    }
}

void traverse_directories(const char *base_path, int depth) {
    if (depth > 3) return;
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
            if (is_ignore_dir(entry->d_name)) continue;
            traverse_directories(path, depth + 1);
        } else {
            if (strcmp(entry->d_name, "compose.yaml") == 0 ||
                strcmp(entry->d_name, "compose.yml") == 0 ||
                strcmp(entry->d_name, "docker-compose.yaml") == 0 ||
                strcmp(entry->d_name, "docker-compose.yml") == 0) {
                compose_files[compose_file_count] = strdup(path);
                compose_file_count++;
            }
        }
    }
    closedir(dir);
}

int is_ignore_dir(const char *path) {
    return strstr(path, "ignore") != NULL;
}

void free_compose_files() {
    for (int i = 0; i < compose_file_count; i++) {
        free(compose_files[i]);
    }
    free(compose_files);
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
    printf("  " GREEN "-e, --exclude PATTERN" NC " Exclude files/directories matching PATTERN\n");
    printf("           " BLUE "1" NC ": Stop, " BLUE "2" NC ": Start, " BLUE "3" NC ": Update (default)\n");
    printf("  " GREEN "--help" NC "    Show this help message\n\n");

    printf(YELLOW "Description:\n" NC);
    printf("  Manages Docker Compose projects in the specified\n");
    printf("  directory and its subdirectories.\n\n");

    printf(YELLOW "Example:\n" NC);
    printf("  cpman -p /projects -m 2\n\n");

    printf(YELLOW "Note:" NC " 'ignore' directories are skipped.\n");
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
