/*
 * Author: Zhang Xun
 * Time: 2023-12-11
 */
#include "debug.h"
#include "file.h"
#include "stdint.h"
#include "stdio.h"
#include "string.h"
#include "syscall.h"

#define CMD_LEN 128
#define MAX_ARG_NR 16

static char cmd_line[CMD_LEN] = {0};
char cwd_buf[64] = {0};

void print_prompt() { printf("[Peach@localhost %s]$ ", cwd_buf); }

static void readline(char *buf, int32_t count) {
  ASSERT(buf != NULL && count > 0);
  char *pos = buf;
  while (read(STDIN_NO, pos, 1) != -1 && (pos - buf) < count) {
    switch (*pos) {
    case '\n':
    case '\r':
      *pos = 0;
      putchar('\n');
      return;
    case '\b':
      if (buf[0] != '\b') {
        /* BACKSPACE: Avoid deleting command prompt  */
        --pos;
        putchar('\b');
      }
      break;
    default:
      putchar(*pos);
      pos++;
    }
  }
  printf("readline: can't find enter_key in the cmd_line, max num of char is "
         "128\n");
}

void zx_shell() {
  cwd_buf[0] = '/';
  while (1) {
    print_prompt();
    memset(cmd_line, 0, CMD_LEN);
    readline(cmd_line, CMD_LEN);
    if (cmd_line[0] == 0) {
      /* Only a carriage return character in the command  */
      continue;
    }
  }
  PANIC("shell: something wrong!");
}
