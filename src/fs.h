#ifndef _FS_H_
#define _FS_H_
#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#define FS_ERR_NONE 0
#define FS_ERR_OPEN_FAIL -1
#define FS_ERR_IS_DIR -2
#define FS_ERR_READ_FAIL -3
#define FS_ERR_WRITE_FAIL -4

extern int getFileContent(char *sfile, char *buf);
#endif
