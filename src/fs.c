#include "fs.h"

int is_dir(char *sfile) {
  struct stat statbuf;
  if (stat(sfile, &statbuf) != 0) {
    logger_debugf("file stat fail [%s] %i: %s\n", sfile, errno,
                  strerror(errno));
    return -1;
  }
  return statbuf.st_mode & S_IFDIR;
}

size_t get_file_size(char *sfile) {
  struct stat statbuf;
  if (stat(sfile, &statbuf) != 0) {
    fprintf(stderr, "file stat fail [%s] %i: %s\n", sfile, errno,
            strerror(errno));
    return -1;
  }
  return statbuf.st_size;
}

// load the file contents on buf argument
// returns <0 error code on error or the bytes read on success
int getFileContent(char *sfile, char *buf) {
  errno = 0;
  // first get the size of the file
  size_t filesize = get_file_size(sfile);
  FILE *fp = fopen(sfile, "r");
  if (fp == NULL || errno > 0) {
    fprintf(stderr, "file open fail [%s] %i: %s\n", sfile, errno,
            strerror(errno));
    return FS_ERR_OPEN_FAIL;
  }
  size_t byteCount = 0;
  byteCount += fread(buf, filesize, 1, fp);
  if (errno > 0) {
    if (errno == 21) {
      return FS_ERR_IS_DIR;
    }
    fprintf(stderr, "fail to read the file [%s] %i: %s\n", sfile, errno,
            strerror(errno));
    return FS_ERR_READ_FAIL;
  }
  if (fclose(fp) != 0) {
    fprintf(stderr, "fail to close the file [%s] %i: %s\n", sfile, errno,
            strerror(errno));
  }
  return byteCount * filesize;
}

int stream_file_content(int fd, char *file_path) {
  errno = 0;
  size_t filesize = get_file_size(file_path);
  // TODO write cache
  size_t bufsize = 1 << 13;
  char buf[bufsize];
  FILE *fp = fopen(file_path, "r");
  if (fp == NULL || errno > 0) {
    fprintf(stderr, "stream file open fail [%s] %i: %s\n", file_path, errno,
            strerror(errno));
    return FS_ERR_OPEN_FAIL;
  }
  size_t readcnt = 0;
  size_t sentbytes = 0;
  char wrote_response_headers = 0;
  clearerr(fp);
  while (!ferror(fp)) {
    readcnt = fread(buf, 1, bufsize, fp);
    if (errno > 0) {
      if (errno == 21) {
        return FS_ERR_IS_DIR;
      }
      fprintf(stderr, "stream fail to read the file [%s] %i: %s\n", file_path,
              errno, strerror(errno));
      return FS_ERR_READ_FAIL;
    }
    if (readcnt <= 0) {
      int errcode = ferror(fp);
      if (errcode > 0) {
        logger_debugf("ERROR File returned %i\n", errcode);
      }
      if (sentbytes != filesize) {
        return FS_ERR_WRITE_FAIL;
      }
      break;
    }
    if (!wrote_response_headers) {
      char HTTP_OK_HEAD[] = "HTTP/2.0 200 OK\n\
Content-Type: text/plain; charset=UTF-8\n\
\n";
      size_t bcnt = write(fd, HTTP_OK_HEAD, strlen(HTTP_OK_HEAD));
      if (bcnt <= 0) {
        return FS_ERR_WRITE_FAIL;
      }
      wrote_response_headers = 1;
    }
    logger_debugf("stream %lu bytes read from file \"%s\"\n", readcnt,
                  file_path);
    size_t r = write(fd, buf, readcnt);
    printf("wrote r=%lu to resp, %lu, %lu", r, bufsize, readcnt);
    if (r != readcnt) {
      return FS_ERR_WRITE_FAIL;
    }
    sentbytes += readcnt;
  }
  if (fclose(fp) != 0) {
    fprintf(stderr, "fail to close the file [%s] %i: %s\n", file_path, errno,
            strerror(errno));
  }
  return FS_ERR_NONE;
}
