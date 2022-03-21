#include "fs.h"

// load the file contents on buf argument
// returns <0 error code on error or the bytes read on success
int getFileContent(char *sfile, char *buf) {
  // first get the size of the file
  struct stat statbuf;
  if (stat(sfile, &statbuf) != 0) {
    fprintf(stderr, "file stat fail [%s] %i: %s", sfile, errno,
            strerror(errno));
    return -1;
  }
  FILE *fp = fopen(sfile, "r");
  if (fp == NULL) {
    fprintf(stderr, "file open fail [%s] %i: %s", sfile, errno,
            strerror(errno));
    return -2;
  }
  size_t byteCount = 0;
  byteCount += fread(buf, statbuf.st_size, 1, fp);
  if (errno > 0) {
    fprintf(stderr, "fail to read the file [%s] %i: %s", sfile, errno,
            strerror(errno));
    return -3;
  }
  if (fclose(fp) != 0) {
    fprintf(stderr, "fail to close the file [%s] %i: %s", sfile, errno,
            strerror(errno));
  }
  return byteCount;
}
