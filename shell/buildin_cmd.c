#include "debug.h"
#include "dir.h"
#include "fs.h"
#include "string.h"
#include "syscall.h"

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
