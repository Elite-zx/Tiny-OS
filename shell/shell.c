/*
 * Author: Zhang Xun
 * Time: 2023-12-11
 */
#include "buildin_cmd.h"
#include "debug.h"
#include "file.h"
#include "fs.h"
#include "global.h"
#include "stdint.h"
#include "stdio.h"
#include "string.h"
#include "syscall.h"

#define MAX_ARG_NR 16

static char cmd_line[MAX_PATH_LEN] = {0};
char final_path[MAX_PATH_LEN] = {0};
char cwd_buf[MAX_PATH_LEN] = {0};

void print_prompt() { printf("[Peach@localhost %s]$ ", cwd_buf); }

static void readline(char *buf, int32_t count) {
  assert(buf != NULL && count > 0);
  char *pos = buf;
  while (read(STDIN_NO, pos, 1) != -1 && (pos - buf) < count) {
    switch (*pos) {
    case '\n':
    case '\r':
      /* end of command  */
      *pos = 0;
      putchar('\n');
      return;
    case '\b':
      /* handle BACKSPACE: Avoid deleting command prompt  */
      if (buf[0] != '\b') {
        --pos;
        putchar('\b');
      }
      break;
    case 'l' - 'a':
      /* handle Ctrl+l, clear screen but maintain the current line  */
      *pos = 0;
      clear();
      print_prompt();
      printf("%s", buf);
      break;
    case 'u' - 'a':
      /* handle Ctrl+l, clear what I put in the current line  */
      while (buf != pos) {
        putchar('\b');
        *(pos--) = 0;
      }
      break;
    default:
      /* regular character, just print  */
      putchar(*pos);
      pos++;
    }
  }
  printf("readline: can't find enter_key in the cmd_line, max num of char is "
         "128\n");
}

static int32_t cmd_parse(char *cmd_str, char **argv, char token) {
  assert(cmd_str != NULL);

  int32_t arg_idx = 0;
  while (arg_idx < MAX_ARG_NR) {
    /* clear argv  */
    argv[arg_idx] = NULL;
    arg_idx++;
  }

  char *next = cmd_str;
  int32_t argc = 0;
  while (*next) {
    while (*next == token) {
      /* skip over the token (whitespace) before parameter*/
      next++;
    }

    if (*next == 0) {
      /* end of command  */
      break;
    }

    /* record the command line parameters */
    argv[argc] = next;

    while (*next && *next != token) {
      /* find the end of current parameter  */
      next++;
    }

    if (*next == token) {
      /* set the end of current parameter to '\0'  */
      *next++ = 0;
    }

    if (argc > MAX_ARG_NR) {
      return -1;
    }
    argc++;
  }
  return argc;
}

char *argv[MAX_ARG_NR];
int32_t argc = -1;

void zx_shell() {
  cwd_buf[0] = '/';
  while (1) {
    print_prompt();
    memset(final_path, 0, MAX_PATH_LEN);
    memset(cmd_line, 0, MAX_PATH_LEN);
    readline(cmd_line, MAX_PATH_LEN);
    if (cmd_line[0] == 0) {
      /* Only a carriage return character in the command  */
      continue;
    }
    argc = -1;
    argc = cmd_parse(cmd_line, argv, ' ');
    if (argc == -1) {
      printf("number of parameters exceeds maximum allowed (%d) \n",
             MAX_ARG_NR);
      continue;
    }
    if (!strcmp("ls", argv[0])) {
      buildin_ls(argc, argv);
    } else if (!strcmp("cd", argv[0])) {
        if (buildin_cd(argc, argv) != NULL) {
          memset(cwd_buf, 0, MAX_PATH_LEN);
          strcpy(cwd_buf, final_path);
        }
      }
    else if (!strcmp("pwd", argv[0])) {
      buildin_pwd(argc, argv);
    } else {
      printf("command not found: %s", argv[0]);
    }
  }
  PANIC("shell: you should't be here :‑( !");
}
