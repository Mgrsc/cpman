#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <unistd.h>
#include "config.h"
#include "utils.h"
#include "compose.h"

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

