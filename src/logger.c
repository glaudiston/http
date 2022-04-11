#ifndef _LOGGER_C_
#define _LOGGER_C_
#include "logger.h"
void logger_warnf(const char *fmt, ...) {
  fprintf(stderr, "\n[WARN]\t");
  va_list ap;
  va_start(ap, fmt);
  vfprintf(stderr, fmt, ap);
  va_end(ap);
}
void logger_debugf(const char *fmt, ...) {
  fprintf(stderr, "\n[DEBUG]\t");
  va_list ap;
  va_start(ap, fmt);
  vfprintf(stderr, fmt, ap);
  va_end(ap);
}
#endif
