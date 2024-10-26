#ifndef COMPOSE_H
#define COMPOSE_H

#include <sys/types.h>

void main_menu(int mode);
void update_compose_files();
void pause_all_compose();
void start_all_compose();
char *get_image_id(const char *file);
void loading_animation(pid_t pid);

#endif

