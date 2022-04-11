#include "event_poll_proxy.h"

#define MAXEVENTS 3

// prepare a poll to manage the connected clients
// retuns fd_epoll on success, or -1 on error
static inline int prepareHttpClientPoll() {
  int fd_epoll;
  if ((fd_epoll = epoll_create(1)) == -1) {
    return -1;
  }
  return fd_epoll;
}

static inline int http_wait_events(int fd_epoll, struct epoll_event *event) {
  int fd_tcp_ready = epoll_wait(fd_epoll, event, MAXEVENTS, 1000);
  if (fd_tcp_ready == -1) {
    fprintf(stderr, "epoll event checking failed %i: %s\n", errno,
            strerror(errno));
  }
  return fd_tcp_ready;
}

#define HTTP_REQUEST_DATA_CHUNK_SIZE 4096
// collect event data chunk and stores it on received_data up to
// HTTP_REQUEST_DATA_CHUNK_SIZE bytes
static inline void collect_event_data_chunk(struct tcp_event_data *d,
                                            char *received_data) {
  printf("reading data from %s:%s\n", d->client_host, d->client_port);
  int r = read(d->fd_tcp_client_socket, &received_data[0],
               HTTP_REQUEST_DATA_CHUNK_SIZE);
  if (r > 0) {
    printf("data received from %s:%s\t(%i bytes):\n%s\n", d->client_host,
           d->client_port, r, &received_data[0]);
  }
}

static inline void process_epoll_event(struct context *ctx,
                                       struct tcp_event_data *d,
                                       char *received_data) {
  collect_event_data_chunk(d, received_data);

  // TODO DDOS mitigation: put this event queue on a free promises poll position
  if (process_http_request(ctx, d->fd_tcp_client_socket, &received_data[0]) !=
      0) {
    fprintf(stderr, "fail to process http request: %s", &received_data[0]);
  }

  // clean up this event
  if (close(d->fd_tcp_client_socket) != 0) {
    fprintf(stderr, "fail to close client connection on fd %i, %i: %s",
            d->fd_tcp_client_socket, errno, strerror(errno));
  }
  free(d);
}

static inline void http_process_events(struct context *ctx, int event_count,
                                       char *received_data,
                                       struct epoll_event *event) {
  struct tcp_event_data *d;
  for (int i = 0; i < event_count; i++) {
    d = (struct tcp_event_data *)event[i].data.ptr;
    process_epoll_event(ctx, d, received_data);
  }
  // TODO DDOS mitigation: wait MAXEVENTS promises to be free on the promises
  // POLL, or timeout?
  printf(".");
  fflush(stdout);
}
