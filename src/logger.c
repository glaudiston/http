#ifndef _LOGGER_C_
#define _LOGGER_C_
#include "logger.h"

void logger_reportf(const char *level, const char *fmt, ...) {
  fprintf(stderr, "\n[%s]\t", level);
  va_list ap;
  va_start(ap, fmt);
  vfprintf(stderr, fmt, ap);
  va_end(ap);
}
#endif
