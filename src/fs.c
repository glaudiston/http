#include "fs.h"

// is_dir returns 1 if is a directory and 0 otherwise (even if error)
// if the return value is lower than 0 you can check the error with strerror(-retval)
int is_dir(char *sfile) {
  errno = 0;
  struct stat statbuf;
  if (stat(sfile, &statbuf) != 0) {
    int rv = errno;
    logger_debugf("is_dir: file stat fail [%s] %i: %s\n", sfile, rv,
                  strerror(rv));
    return -rv;
  }
  return ((statbuf.st_mode & S_IFDIR) == S_IFDIR);
}

size_t get_file_size(char *sfile) {
  struct stat statbuf;
  if (stat(sfile, &statbuf) != 0) {
    logger_errorf("get_file_size: file stat fail [%s] %i: %s\n", sfile, errno,
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
    logger_errorf("file open fail [%s] %i: %s\n", sfile, errno,
                  strerror(errno));
    return FS_ERR_OPEN_FAIL;
  }
  size_t byteCount = 0;
  byteCount += fread(buf, filesize, 1, fp);
  if (errno > 0) {
    if (errno == 21) {
      return FS_ERR_IS_DIR;
    }
    logger_errorf("fail to read the file [%s] %i: %s\n", sfile, errno,
                  strerror(errno));
    return FS_ERR_READ_FAIL;
  }
  if (fclose(fp) != 0) {
    logger_errorf("fail to close the file [%s] %i: %s\n", sfile, errno,
                  strerror(errno));
  }
  return byteCount * filesize;
}

int ends_with(const char *text, const char *substr) {
  int ts = strlen(text), ss = strlen(substr);
  return ts >= ss && (strncmp(&text[ts - ss], substr, ss) == 0);
}

int stream_file_content(int fd, char *file_path) {
  logger_debugf("streaming file %s\n", file_path);
  errno = 0;
  size_t filesize = get_file_size(file_path);
  if (filesize == 0) {
    char HTTP_OK_HEAD[] = "HTTP/2.0 204 No Content\n\
Content-Type: text/plain; charset=UTF-8\n\
\n";
    size_t bcnt = write(fd, HTTP_OK_HEAD, strlen(HTTP_OK_HEAD));
    if (bcnt <= 0) {
      return FS_ERR_WRITE_FAIL;
    }
    return FS_ERR_NONE;
  }
  // TODO write cache
  size_t bufsize = 1 << 13;
  char buf[bufsize];
  FILE *fp = fopen(file_path, "r");
  if (fp == NULL || errno > 0) {
    logger_errorf("stream file open fail [%s] %i: %s\n", file_path, errno,
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
      logger_errorf("stream fail to read the file [%s] %i: %s\n", file_path,
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
      char content_type[50];
      if (ends_with(file_path, ".html")) {
        sprintf(content_type, "text/html");
      } else if (ends_with(file_path, ".js")) {
        sprintf(content_type, "text/javascript");
      } else if (ends_with(file_path, ".css")) {
        sprintf(content_type, "text/css");
      } else {
        sprintf(content_type, "text/plain");
      }
      logger_debugf("content_type detected: %s\n", content_type);
      fflush(stderr);
      char HTTP_OK_HEAD[2048];
      sprintf(HTTP_OK_HEAD, "HTTP/2.0 200 OK\n\
Content-Type: %s; charset=UTF-8\n\
\n",
              content_type);
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
    logger_errorf("fail to close the file [%s] %i: %s\n", file_path, errno,
                  strerror(errno));
  }
  return FS_ERR_NONE;
}
