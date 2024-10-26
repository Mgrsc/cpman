#ifndef UTILS_H
#define UTILS_H

void signal_handler(int sig);
void check_command();
void find_compose_files();
void traverse_directories(const char *base_path, int depth);
int is_ignore_dir(const char *path);
void free_compose_files();
void print_help();
int parse_args(int argc, char *argv[], int *mode, char **path, char **exclude);

#endif

