#ifndef _LOGGER_H_
#define _LOGGER_H_
#include <stdarg.h>
#include <stdio.h>
#define logger_errorf(fmt, ...)                                                \
  { logger_reportf("ERROR", fmt, __VA_ARGS__); }
#define logger_warnf(fmt, ...)                                                 \
  { logger_reportf("WARN", fmt, __VA_ARGS__); }
#define logger_infof(fmt, ...)                                                 \
  { logger_reportf("INFO", fmt, __VA_ARGS__); }
#define logger_debugf(fmt, ...)                                                \
  { logger_reportf("DEBUG", fmt, __VA_ARGS__); }
extern void logger_reportf(const char *level, const char *fmt, ...);
#endif
