#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <dirent.h>
#include <sys/stat.h>
#include <limits.h>
#include "config.h"
#include "utils.h"

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
