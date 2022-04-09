#include "byte_parser.h"

// wcpy copy the first word delimited by delim char to target
// returns the size of the word.
int wcpyb(const char *line, char *target, char *delim, int delim_cnt) {
  int s = 0;
  int ls = strlen(line);
  char c;
  for (int i = 0; i < ls; i++) {
    c = line[i];
    for (int j = 0; j < delim_cnt; j++) {
      if (c == delim[j]) {
        target[s] = '\0';
        return s;
      }
    }
    target[s++] = c;
  }
  target[s] = '\0';
  return s;
}
// wcpy copy the first word to target
// returns the size of the word.
int wcpy(const char *line, char *target) {
  char delimlist[] = {' ', '\r', '\n'};
  return wcpyb(line, target, delimlist, 3);
}
// lcpy copy the first line to target
// returns the size of the line.
int lcpy(const char *line, char *target) {
  char delimlist[] = {'\r', '\n'};
  return wcpyb(line, target, delimlist, 2);
}
