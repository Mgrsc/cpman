#ifndef CONFIG_H
#define CONFIG_H

#include <stddef.h>
#include <limits.h>

// Define color codes
#define GREEN "\033[0;32m"
#define YELLOW "\033[0;33m"
#define BLUE  "\033[0;34m"
#define CYAN  "\033[0;36m"
#define RED   "\033[0;31m"
#define NC    "\033[0m" // No Color

// Global variables
extern char COMPOSE_CMD[256];
extern char DOCKER_CMD[256];
extern char **compose_files;
extern int compose_file_count;
extern char *exclude_pattern;

#endif

