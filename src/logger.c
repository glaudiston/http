#include "logger.h"
void logger_warnf(const char *fmt, ...) {
  fprintf(stderr, "[WARN]\t");
  va_list ap;
  va_start(ap, fmt);
  vfprintf(stderr, fmt, ap);
  va_end(ap);
}
