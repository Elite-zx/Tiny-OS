#ifndef __SHELL_BUILDIN_CMD_H
#define __SHELL_BUILDIN_CMD_H
#include "global.h"
#include "stdint.h"

void make_clear_abs_path(char *path, char *final_path);
char *buildin_cd(uint32_t argc, char **argv);
void buildin_ls(uint32_t argc, char **argv);
void buildin_pwd(uint32_t argc, char **argv UNUSED);
#endif
