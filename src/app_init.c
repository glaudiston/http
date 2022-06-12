#include "app_init.h"

static inline int get_server_port(int argc, char *argv[]) {
#define DEFAULT_SERVER_PORT 8080
  int user_selected_port = 0;
  if (argc > 1)
    user_selected_port = atoi(argv[1]);
  if (user_selected_port == 0) {
    logger_warnf("invalid argument port. using port %i. \nsample use: %s 8080 "
                 "/path/to/static/files\n",
                 DEFAULT_SERVER_PORT, argv[0]);
    user_selected_port = DEFAULT_SERVER_PORT;
  }
  return argc == 1 ? DEFAULT_SERVER_PORT : user_selected_port;
}

void get_static_path(char *static_path, int argc, char *argv[]) {
  if (argc > 2) {
    strcpy(static_path, argv[2]);
  } else {
    sprintf(static_path, ".");
  }
  DIR *dr = opendir(static_path);
  if (dr == NULL) {
    logger_errorf("unable to open directory [%s]\n", static_path);
  }
  closedir(dr);
  if (static_path[strlen(static_path) - 1] != '/')
    static_path[strlen(static_path)] = '/';
  static_path[strlen(static_path)] = 0;
}

// start the thread to listen the tcp port and wait for http input
// update the ctx object with the thread and thread_attr
// return 0 on success
static inline int listenTCPThread(struct context *ctx) {
  if ( pthread_attr_init(&ctx->thread_tcp_listen_attr) != 0 ) {
	  logger_errorf("unexpected error preparing the thread attribute init %s", strerror(errno));
  }
  int err = pthread_create(&ctx->thread_tcp_listen, &ctx->thread_tcp_listen_attr,
                     do_listen_tcp, ctx);
  if ( err != 0 ) {
    logger_errorf("fail to create the tcp server accept thread %i: %s\n", errno,
                  strerror(errno));
    return -1;
  }
  return 0;
}
