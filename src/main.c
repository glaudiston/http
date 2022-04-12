#include "main.h"

int main(int argc, char *argv[]) {
  struct context ctx;
  ctx.port_number = get_server_port(argc, argv);
  get_static_path(ctx.static_path, argc, argv);

  if ((ctx.fd_epoll = prepareHttpClientPoll()) < 0) {
    logger_errorf("fail to create http event poll %i: %s\n", errno,
            strerror(errno));
    return 1;
  }

  if (listenTCPThread(&ctx) != 0) {
    logger_errorf( "fail to create a thread to listen the tcp socket %i: %s\n",
            errno, strerror(errno));
    return 2;
  }

  // TODO DDOS mitigation: create a promise poll to asynchronous processing the
  // http requests and responses to avoid overload main accepting thread
  char *received_data = malloc(HTTP_REQUEST_DATA_CHUNK_SIZE);
  // wait for connection and data from tcp sockets
  struct epoll_event event[MAXEVENTS];
  for (;;) {
    int event_count = http_wait_events(ctx.fd_epoll, event);
    http_process_events(&ctx, event_count, received_data, event);
  }
}
