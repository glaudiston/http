#ifndef _STRUCT_CONTEXT_H_
#define _STRUCT_CONTEXT_H_

#include <pthread.h>

struct context {
  int fd_epoll;
  int port_number;
  char static_path[1 << 10];
  pthread_t thread_tcp_listen;
  pthread_attr_t thread_tcp_listen_attr;
};
#endif
