/*
 * Author: Zhang Xun
 * Time: 2023-12-12
 */
#include "buildin_cmd.h"
#include "debug.h"
#include "dir.h"
#include "fs.h"
#include "global.h"
#include "stdint.h"
#include "stdio.h"
#include "string.h"
#include "syscall.h"

extern char final_path[MAX_PATH_LEN];

/**
 * convert_path() - Simplify a given absolute file path.
 * @old_abs_path: The original absolute path containing symbols like '..' and
 * '.'.
 * @new_abs_path: Buffer to store the simplified absolute path.
 *
 * This function processes the 'old_abs_path' to eliminate special directory
 * symbols like '..' (parent directory) and '.' (current directory). The
 * resulting simplified path is stored in 'new_abs_path'. It handles cases like
 * "/a/b/../c" which would simplify to "/a/c". The function assumes
 * 'old_abs_path' starts with '/', indicating it is an absolute path.
 *
 * Context: Used in shell or filesystem-related code for converting a
 * potentially complex path into a canonical absolute path without redundant
 * components.
 */
static void convert_path(char *old_asb_path, char *new_abs_path) {
  assert(old_asb_path[0] == '/');

  /* name_buf is used to store the parsed directory name of each level in
   * old_asb_path  */
  char name_buf[MAX_FILE_NAME_LEN] = {0};
  char *sub_path = old_asb_path;
  sub_path = path_parse(old_asb_path, name_buf);

  if (name_buf[0] == 0) {
    /* old_asb_path is root_dir  */
    new_abs_path[0] = '/';
    new_abs_path[1] = 0;
    return;
  }

  new_abs_path[0] = 0;
  strcat(new_abs_path, "/");
  while (name_buf[0] != 0) {
    if (!strcmp("..", name_buf)) {
      char *slash_ptr = strrchr(new_abs_path, '/');
      if (slash_ptr != new_abs_path) {
        /* '/' as root_dir  */
        *slash_ptr = 0;
      } else {
        /* '/' as path separator  */
        *(slash_ptr + 1) = 0;
      }
    } else if (strcmp(".", name_buf)) {
      if (strcmp(new_abs_path, "/")) {
        /* avoid "//" in the begining of new_abs_path*/
        strcat(new_abs_path, "/");
      }
      strcat(new_abs_path, name_buf);
    } else {
      // name_buf is '/', do nothing
    }
    memset(name_buf, 0, MAX_FILE_NAME_LEN);
    if (sub_path) {
      sub_path = path_parse(sub_path, name_buf);
    }
  }
}

/**
 * make_clear_abs_path() - Convert a relative or absolute path to a simplified
 * absolute path.
 * @path: The original file path, which can be relative or absolute.
 * @final_path: Buffer to store the resulting simplified absolute path.
 *
 * This function takes a file path, which can be either relative or absolute,
 * and converts it into a simplified, absolute path. It first checks if the path
 * is absolute. If not, it prepends the current working directory to make it
 * absolute. Then, it calls 'convert_path' to eliminate any '..' or '.' symbols,
 * simplifying the path. The final, clean absolute path is stored in
 * 'final_path'.
 *
 * Context: Particularly useful in a shell environment for path resolution,
 * ensuring that paths are in a consistent and simplified absolute format.
 */
void make_clear_abs_path(char *path, char *final_path) {
  char abs_path[MAX_PATH_LEN] = {0};

  /******** construct absolute path from 'path' ********/
  if (path[0] != '/') {
    memset(abs_path, 0, MAX_PATH_LEN);
    if (getcwd(abs_path, MAX_PATH_LEN) != NULL) {
      if (!((abs_path[0] == '/') && (abs_path[1] == 0))) {
        strcat(abs_path, "/");
      }
    }
  }
  strcat(abs_path, path);

  /******** remove '.' and '..' in old absolute path ********/
  convert_path(abs_path, final_path);
}

/* command pwd  */
void buildin_pwd(uint32_t argc, char **argv UNUSED) {
  if (argc != 1) {
    printf("pwd: too many arguments!\n");
    return;
  } else {
    if (getcwd(final_path, MAX_PATH_LEN) != NULL) {
      printf("%s\n", final_path);
    } else {
      printf("pwd: get current working directory failed\n");
    }
  }
}

/**
 * buildin_cd() - The built-in function for the 'cd' command in a shell.
 * @argc: The number of arguments passed to the command.
 * @argv: An array of strings representing the arguments passed to the command.
 *
 * This function implements the 'cd' (change directory) command. It changes the
 * current working directory of the process to the directory specified in
 * argv[1]. If no argument is provided (argc == 1), it changes the directory to
 * the root ('/'). It also handles error checking for too many arguments or
 * invalid directory paths.
 *
 * Context: Typically used in a shell environment where users can change the
 * current working directory using the 'cd' command. Return: Returns the new
 * absolute path after changing directories, or NULL if an error occurs.
 */
char *buildin_cd(uint32_t argc, char **argv) {
  /* too much argument  */
  if (argc > 2) {
    printf("cd: too many arguments!\n");
    return NULL;
  }

  if (argc == 1) {
    /* no argument for command cd, change directory to root dir (so root_dir is
     * the default argument of command cd ) */
    final_path[0] = '/';
    final_path[1] = 0;
  } else {
    /* convert path to canonical */
    make_clear_abs_path(argv[1], final_path);
  }

  if (chdir(final_path) == -1) {
    printf("cd: no such directory: %s\n", final_path);
    return NULL;
  }
  return final_path;
}

/**
 * buildin_ls() - The built-in function for the 'ls' command in a shell.
 * @argc: The number of arguments passed to the command.
 * @argv: An array of strings representing the arguments passed to the command.
 *
 * This function implements the 'ls' (list) command. It lists the contents of a
 * directory specified by the user, or the current directory if no path is
 * given. The function supports options '-l' for long listing format and '-help'
 * for help information. It handles displaying directory entries, their inode
 * numbers, and file sizes, and optionally includes additional information in
 * long format.
 *
 * Context: Used in a shell environment to list the contents of directories and
 * provide information about files. It emulates the common behavior of the Unix
 * 'ls' command.
 */
void buildin_ls(uint32_t argc, char **argv) {
  char *pathname = NULL;
  struct stat file_stat;
  memset(&file_stat, 0, sizeof(struct stat));

  uint32_t arg_path_nr = 0;
  bool long_info = false;
  uint32_t arg_idx = 1;
  while (arg_idx < argc) {
    if (argv[arg_idx][0] == '-') {
      /******** handle option argument********/
      if (!strcmp(argv[arg_idx], "-l")) {
        long_info = true;
      } else if (!strcmp(argv[arg_idx], "--help")) {
        printf("Usage: ls [OPTION]... [FILE]...\nlist all files in the "
               "current directory if no option\n\n  -l            list all all "
               "information\n  --help        for help\n");
        return;
      } else {
        printf("ls: invalid option %s\nMore info with: 'ls --help'.\n",
               argv[arg_idx]);
        return;
      }
    } else {
      /******** handle path argument ********/
      if (arg_path_nr == 0) {
        pathname = argv[arg_idx];
        arg_path_nr = 1;
      } else {
        printf("ls: too many arguments\n");
        return;
      }
    }
    /* next argument  */
    arg_idx++;
  }

  if (pathname == NULL) {
    /******** not path argument, which means "ls" or "ls -l". print all files
     * under cwd ********/
    if (getcwd(final_path, MAX_PATH_LEN) != NULL) {
      pathname = final_path;
    } else {
      printf("ls: getcwd for default path failed\n");
      return;
    }
  } else {
    /******** have path argument, convert it to a canonical absolute
     * path********/
    make_clear_abs_path(pathname, final_path);
    pathname = final_path;
  }

  /******** get the attributes of the specified file ********/
  if (stat(pathname, &file_stat) == -1) {
    printf("ls: Specified path '%s' doesn't exist.\n", pathname);
    return;
  }

  if (file_stat.st_filetype == FT_DIRECTORY) {
    /******** paathname is a directory, so traverse all dir entry in specified
     * directory ********/
    struct dir *dir = opendir(pathname);
    struct dir_entry *dir_e = NULL;
    char sub_pathname[MAX_PATH_LEN] = {0};
    uint32_t pathname_len = strlen(pathname);
    uint32_t last_char_idx = pathname_len - 1;
    memcpy(sub_pathname, pathname, pathname_len);

    /* make sure there is a path delimiter '\' at the end of sub_pathname,
     * because the file name in the directory will be appended later.  */
    if (sub_pathname[last_char_idx] != '/') {
      sub_pathname[pathname_len] = '/';
      pathname_len++;
    }

    rewinddir(dir);

    if (long_info) {
      /******** handle 'ls -l', print long info
       * : total size, file type, inode_NO, file size, filename ********/
      char f_type;
      printf("total: %d\n", file_stat.st_size);
      while ((dir_e = readdir(dir))) {
        f_type = 'd';
        if (dir_e->f_type == FT_REGULAR) {
          f_type = '-';
        }
        sub_pathname[pathname_len] = 0;
        strcat(sub_pathname, dir_e->filename);
        memset(&file_stat, 0, sizeof(struct stat));
        if (stat(sub_pathname, &file_stat) == -1) {
          printf("ls: Specified path '%s' doesn't exist.\n", dir_e->filename);
          return;
        }
        printf("%c %d %d %s\n", f_type, dir_e->i_NO, file_stat.st_size,
               dir_e->filename);
      }
    } else {
      /******** with option '-l', Just simply print the filename ********/
      while ((dir_e = readdir(dir))) {
        printf("%s ", dir_e->filename);
      }
      printf("\n");
    }
    closedir(dir);
  } else {
    /******** the specified file 'pathname' is a regular file ********/
    if (long_info) {
      printf("- %d %d %s\n", file_stat.st_ino, file_stat.st_size, pathname);
    } else {
      printf("%s\n", pathname);
    }
  }
}

void buildin_ps(uint32_t argc, char **argv UNUSED) {
  if (argc != 1) {
    printf("ps: too many arguments\n");
    return;
  }
  ps();
}
void buildin_clear(uint32_t argc, char **argv UNUSED) {
  if (argc != 1) {
    printf("clear: too many arguments\n");
    return;
  }
  clear();
}

int32_t buildin_mkdir(uint32_t argc, char **argv) {
  int32_t ret_val = -1;
  if (argc != 2) {
    printf("mkdir: too many arguments\n");
  } else {
    make_clear_abs_path(argv[1], final_path);
    if (strcmp("/", final_path)) {
      /* Not the root directory  */
      if (mkdir(final_path) == 0) {
        ret_val = 0;
      } else {
        printf("mkdir: create directory %s failed.\n", argv[1]);
      }
    }
  }
  return ret_val;
}

int32_t buildin_rmdir(uint32_t argc, char **argv) {
  int32_t ret_val = -1;
  if (argc != 2) {
    printf("rmdir: too many arguments\n");
  } else {
    make_clear_abs_path(argv[1], final_path);
    if (strcmp("/", final_path)) {
      /* Not the root directory  */
      if (rmdir(final_path) == 0) {
        ret_val = 0;
      } else {
        printf("rmdir: remove directory %s failed.\n", argv[1]);
      }
    }
  }
  return ret_val;
}

int32_t buildin_rm(uint32_t argc, char **argv) {
  int32_t ret_val = -1;
  if (argc != 2) {
    printf("rm: too many arguments\n");
  } else {
    make_clear_abs_path(argv[1], final_path);
    if (strcmp("/", final_path)) {
      /* Not the root directory  */
      if (unlink(final_path) == 0) {
        ret_val = 0;
      } else {
        printf("rm: delete %s failed.\n", argv[1]);
      }
    }
  }
  return ret_val;
}
