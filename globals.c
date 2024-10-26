#include <stddef.h>
#include "config.h"

char COMPOSE_CMD[256] = {0};
char DOCKER_CMD[256] = {0};
char **compose_files = NULL;
int compose_file_count = 0;
char *exclude_pattern = NULL;

