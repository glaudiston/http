#ifndef _LOGGER_H_
#define _LOGGER_H_
#include <stdarg.h>
#include <stdio.h>
extern void logger_errorf(const char *fmt, ...);
extern void logger_warnf(const char *fmt, ...);
extern void logger_debugf(const char *fmt, ...);
#endif
