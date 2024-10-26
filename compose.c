#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/wait.h>
#include <limits.h>
#include "config.h"
#include "compose.h"

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
