#include "http_accept.h"

int do_accept_tcp_request(int fd_tcp_server_socket, int fd_epoll) {
  // Accept requests code block start
  printf("waiting a new tcp connection...\n");
  fflush(stdout);
  struct sockaddr_in cli_addr;
  cli_addr.sin_family = AF_INET;
  socklen_t cli_addrsize = sizeof(struct sockaddr_in);
  int fd_tcp_client_socket =
      accept(fd_tcp_server_socket, (struct sockaddr *)&cli_addr, &cli_addrsize);
  if (fd_tcp_client_socket == -1) {
    fprintf(stderr, "fail to accept a connection %i: %s\n", errno,
            strerror(errno));
    return 1;
  }
  struct tcp_event_data *d = malloc(sizeof(struct tcp_event_data));
  int i = getnameinfo((struct sockaddr *)&cli_addr, cli_addrsize,
                      d->client_host, sizeof(d->client_host), d->client_port,
                      sizeof(d->client_port), 0);
  if (i != 0) {
    fprintf(stderr, "fail to get client address information %i: %s\n", i,
            gai_strerror(i));
    return 2;
  }
  printf("Connection accepted from %s:%s\n", d->client_host, d->client_port);
  d->fd_tcp_client_socket = fd_tcp_client_socket;
  struct epoll_event epoll_evt = {.events = EPOLLIN, .data = {.ptr = d}};
  i = epoll_ctl(fd_epoll, EPOLL_CTL_ADD, fd_tcp_client_socket, &epoll_evt);
  if (i != 0) {
    fprintf(stderr, "fail to add client %s:%s to epoll %i, %i: %s\n",
            d->client_host, d->client_port, i, errno, strerror(errno));
    fprintf(stderr, "fd epoll %i\nfd cli %i", fd_epoll, fd_tcp_client_socket);
    return 3;
  }
  return 0;
}
