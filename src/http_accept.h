#ifndef _HTTP_ACCEPT_H_
#define _HTTP_ACCEPT_H_
#include "fs.h"
#include "http_structs.h"
#include "logger.c"
#include "struct_context.h"
#include <arpa/inet.h>
#include <dirent.h>
#include <errno.h>
#include <netdb.h>
#include <netinet/in.h>
#include <netinet/ip.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/epoll.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>
extern void *do_listen_tcp(void *args_ptr);
extern int do_accept_tcp_request(int fd_tcp_server_socket, int fd_epoll);
extern int process_http_request(struct context *ctx, int fd, char *data_in);
#endif
