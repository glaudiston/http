#ifndef _HTTP_ACCEPT_H_
#include "http_structs.h"
#include <errno.h>
#include <netdb.h>
#include <netinet/ip.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/epoll.h>
#include <sys/socket.h>
extern int do_accept_tcp_request(int fd_tcp_server_socket, int fd_epoll);
#endif
