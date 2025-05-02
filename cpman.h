#ifndef CPMAN_H
#define CPMAN_H

#include <sys/types.h>
#include <stddef.h>
#include <limits.h>

#define GREEN "\033[0;32m"
#define YELLOW "\033[0;33m"
#define BLUE  "\033[0;34m"
#define CYAN  "\033[0;36m"
#define RED   "\033[0;31m"
#define NC    "\033[0m"

extern char COMPOSE_CMD[256];
extern char DOCKER_CMD[256];
extern char **compose_files;
extern int compose_file_count;
extern char *exclude_pattern;
extern int verbose_mode;
extern int timeout_seconds;
extern int max_depth;

void main_menu(int mode);
void update_compose_files();
void pause_all_compose();
void start_all_compose();

char *get_image_id(const char *file);
int execute_command_with_timeout(const char *command, char *output, size_t output_size, const char *work_dir);
void signal_handler(int sig);
void check_command();
void find_compose_files();
void traverse_directories(const char *base_path, int depth);
int is_valid_compose_file(const char *filepath);
void free_compose_files();
void print_help();
int parse_args(int argc, char *argv[], int *mode, char **path, char **exclude);

#endif // CPMAN_H
