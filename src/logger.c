#ifndef _LOGGER_C_
#define _LOGGER_C_
#include "logger.h"

void logger_reportf(const char *level, const char *fmt, ...) {
  if (isatty(fileno(stderr))){
    int c=37;
    if ( memcmp(level, "DEBUG", 5) == 0 ) {
	    c = 36;
    } else if ( memcmp(level, "INFO", 4) == 0 ) {
	    c = 32;
    } else if ( memcmp(level, "WARN", 4) == 0 ) {
	    c = 33;
    } else if ( memcmp(level, "ERROR", 4) == 0 ) {
	    c = 31;
    }
    fprintf(stderr, "\n[\e[%im%s\e[0m]\t", c, level);
  } else {
    fprintf(stderr, "\n[%s]\t", level);
  }
  va_list ap;
  va_start(ap, fmt);
  vfprintf(stderr, fmt, ap);
  va_end(ap);
}
#endif
