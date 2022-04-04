#include "main.h"

struct context {
  int fd_epoll;
  int port_number;
  char static_path[1 << 10];
  pthread_t thread_tcp_listen;
  pthread_attr_t thread_tcp_listen_attr;
};

void *do_listen_tcp(void *args_ptr) {
  struct context *ctx = (struct context *)args_ptr;

  printf("Starting http ipv4 server on port %i...\n", ctx->port_number);

  // set the listen on a ipv4 socket to serve files to browsers
  int domain = AF_INET;
  int type = SOCK_STREAM;
  int protocol = IPPROTO_TCP;
  int fd_tcp_server_socket = socket(domain, type, protocol);

  int level = SOL_SOCKET;
  int optname = SO_REUSEADDR;
  int val = 1;
  const void *optval = &val;
  socklen_t optlen = sizeof(val);
  if (setsockopt(fd_tcp_server_socket, level, optname, optval, optlen) != 0) {
    fprintf(stderr, "setsockopt errno %i: %s", errno, strerror(errno));
    char *thread_ret = malloc(sizeof(char));
    char c = 1;
    memcpy(thread_ret, &c, 1);
    return thread_ret;
  }

  struct sockaddr_in addr;
  addr.sin_family = AF_INET;
  addr.sin_addr.s_addr = inet_addr("0.0.0.0");
  addr.sin_port = htons(ctx->port_number);
  if (bind(fd_tcp_server_socket, (struct sockaddr *)&addr, sizeof(addr)) != 0) {
    fprintf(stderr, "bind fail %i: %s\n", errno, strerror(errno));
    char *thread_ret = malloc(sizeof(char));
    char c = 2;
    memcpy(thread_ret, &c, 1);
    return thread_ret;
  }

  char buf[10];
  getFileContent("/proc/sys/net/ipv4/tcp_max_syn_backlog", &buf[0]);
  int backlog = atoi(buf);
  if (listen(fd_tcp_server_socket, backlog) != 0) {
    fprintf(stderr, "listen fails %i: %s\n", errno, strerror(errno));
    char *thread_ret = malloc(sizeof(char));
    char c = 3;
    memcpy(thread_ret, &c, 1);
    return thread_ret;
  }
  int r = 0;
  for (;;) {
    if ((r = do_accept_tcp_request(fd_tcp_server_socket, ctx->fd_epoll)) != 0) {
      fprintf(stderr, "fail to accept request %i\n", r);
      continue;
    }
  }
  /*
  readv(2) to receive data;
  send(2) to send data;
  */
};

struct http_header {
  char name[127];
  char value[4096];
};

struct http_request {
  char method[7];
  char path[8001]; // size following recommendation defined on RFC2616
  char protocol[10];
  struct http_header *headers;
  int header_count;
  char *body;
};

// wcpy copy the first word delibited by delim to target
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

int parse_request(char *data, struct http_request *req) {
  char line[1024];
  int ln = 0;
  req->headers = malloc(sizeof(struct http_header));
  req->header_count = 0;
  char parsing_headers = 1;
  for (int i = 0; i < strlen(data); i++) {
    if (parsing_headers) {
      i += lcpy(&data[i], &line[0]);
      ln++;
      int s = 0;
      if (ln == 1) {
        s += wcpy(&line[s], &req->method[0]);
        s++;
        s += wcpy(&line[s], &req->path[0]);
        s++;
        s += wcpy(&line[s], &req->protocol[0]);
        s++;
        printf(
            "\nfirst line parsed: \n method:[%s]\n path:[%s]\n protocol:[%s]\n",
            req->method, req->path, req->protocol);
      }
      req->headers = realloc(req->headers,
                             sizeof(struct http_header) * ++req->header_count);
      // resize readers +1
      struct http_header *h = &req->headers[req->header_count - 1];
      s += wcpy(&line[s], h->name);
      s += lcpy(&line[s], h->value);
    } else {
      req->body = &data[i];
    }
  }
  // check if path is too long
  if (strlen(req->path) > sizeof(req->path) - 1) {
    return 414; // too long
  }
  return 0;
}

struct response_cache {
  char *template_content;
} response_cache = {.template_content = 0};

int response_with_list_template(struct context *ctx, int fd, char *path) {
  if (response_cache.template_content == NULL) {
    response_cache.template_content = malloc(1024);
    int rv;
    if ((rv = getFileContent("./http-templates/list.html",
                             response_cache.template_content)) < 0) {
      fprintf(stderr, "Request fail path[%s] ret %i\n", path, rv);
      // internal_server_error();
      return 1;
    }
  }
  char buff[1 << 10];
  sprintf(&buff[0], "HTTP/2.0 200 OK\n\
Content-Type: text/html; charset=UTF-8\n\
\n\
%s",
          response_cache.template_content);
  printf("\nRESPONSE:\n%s\n", buff);
  ssize_t r = write(fd, buff, strlen(buff));
  printf("wrote r=%li to resp, strlen(buff)=%li", r, strlen(buff));
  if (r != strlen(buff)) {
    return 1;
  }
  return 0;
}

int response_with_static_resource(struct context *ctx, int fd, char *abspath) {
  // TODO write cache
  char file_content[1 << 13];
  int rv;
  if ((rv = getFileContent(abspath, file_content)) < 0) {
    if (rv == -3) // is a directory
      return response_with_list_template(ctx, fd, abspath);
    fprintf(stderr, "Request fail file path[%s] ret %i\n", abspath, rv);
    // internal_server_error();
    return 1;
  }
  char buff[1 << 14];
  sprintf(&buff[0], "HTTP/2.0 200 OK\n\
Content-Type: text/plain; charset=UTF-8\n\
\n\
%s",
          file_content);
  printf("\nRESPONSE:\n%s\n", buff);
  ssize_t r = write(fd, buff, strlen(buff));
  printf("wrote r=%li to resp, strlen(buff)=%li", r, strlen(buff));
  if (r != strlen(buff)) {
    return 1;
  }
  return 0;
}

int response_with_server_api(struct context *ctx, int fd, char *path) {
  char buff[1 << 14];
  char json[1 << 13];
  printf("api requested %s\n", path);
  if (strcmp(path, "/.api/list") == 0) {
    char files[1 << 12];
    files[0] = 0;
    struct dirent *de;
    DIR *dr = opendir(ctx->static_path);
    if (dr == NULL) {
      fprintf(stderr, "ERROR: unable to open directory");
    }
    while ((de = readdir(dr)) != NULL) {
      if (de->d_name[0] == '.')
        continue;
      sprintf(&files[strlen(files)], ", \"%s\"", de->d_name);
    }
    closedir(dr);
    sprintf(json, "{ \"status\": \"OK\", \"entries\": [ \".\" %s ] }", files);
  } else {
    sprintf(json, "{ \"message\": \"invalid server api\" }");
  }
  sprintf(&buff[0], "HTTP/2.0 200 OK\n\
Content-Type: application/json; charset=UTF-8\n\
\n\
%s",
          json);
  printf("\nAPI RESPONSE:\n%s\n", buff);
  ssize_t r = write(fd, buff, strlen(buff));
  if (r != strlen(buff)) {
    return 1;
  }
  return 0;
}

int process_http_request(struct context *ctx, int fd, char *data_in) {
  struct http_request req;
  int s = parse_request(data_in, &req);
  if (s > 0) {
    if (s == 414) {
      char buf[] = "HTTP/2.0 414 URI Too long\n\n";
      long unsigned int rv = write(fd, buf, strlen(buf));
      if (rv != sizeof(buf)) {
        fprintf(stderr, "WARN expected write %li bytes, but wrote %li",
                sizeof(buf), rv);
        return 1;
      }
      return 0;
    }
    char buf[] = "HTTP/2.0 500 internal server error at request parsing\n\n";
    long unsigned int rv = write(fd, buf, strlen(buf));
    if (rv != strlen(buf)) {
      fprintf(stderr, "WARN, expected to write %li, but wrote %li\n",
              strlen(buf), rv);
    }
    return 0;
  }
  if (strncmp(req.path, "/.api/", 6) == 0) {
    return response_with_server_api(ctx, fd, req.path);
  }
  char relativepath[1 << 15];
  sprintf(relativepath, "./%s%s", ctx->static_path, req.path);
  if (access(relativepath, F_OK) == 0) {
    return response_with_static_resource(ctx, fd, relativepath);
  }
  char buff[] = "HTTP/2.0 404 OK\n\
Content-Type: text/html; charset=UTF-8\n\
\n\
<!doctype html><html><body>Not found</body></html>\r\n";
  ssize_t r = write(fd, buff, strlen(buff));
  if (r != strlen(buff)) {
    return 1;
  }
  return 0;
}

// prepare a poll to manage the connected clients
// retuns fd_epoll on success, or -1 on error
static inline int prepareHttpClientPoll() {
  int fd_epoll;
  if ((fd_epoll = epoll_create(1)) == -1) {
    return -1;
  }
  return fd_epoll;
}

// start the thread to listen the tcp port and wait for http input
// update the ctx object with the thread and thread_attr
// return 0 on success
static inline int listenTCPThread(struct context *ctx) {
  if (pthread_create(&ctx->thread_tcp_listen, &ctx->thread_tcp_listen_attr,
                     do_listen_tcp, ctx) != 0) {
    fprintf(stderr, "fail to create the tcp server accept thread %i: %s\n",
            errno, strerror(errno));
    return -1;
  }
  return 0;
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

  // TODO DDOS mitigation: put this event on a free promises poll position
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

static inline int get_server_port(int argc, char *argv[]) {
#define DEFAULT_SERVER_PORT 8080
  int user_selected_port = 0;
  if (argc > 1)
    user_selected_port = atoi(argv[1]);
  if (user_selected_port == 0) {
    logger_warnf("invalid argument port. using port %i. \nsample use: %s "
                 "8080 /path/to/static/files",
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
    fprintf(stderr, "ERROR: unable to open directory [%s]", static_path);
  }
  closedir(dr);
}

int main(int argc, char *argv[]) {
  struct context ctx;
  ctx.port_number = get_server_port(argc, argv);
  get_static_path(ctx.static_path, argc, argv);

  if ((ctx.fd_epoll = prepareHttpClientPoll()) < 0) {
    fprintf(stderr, "fail to create epoll %i: %s\n", errno, strerror(errno));
    return 1;
  }

  if (listenTCPThread(&ctx) != 0) {
    fprintf(stderr, "fail to create a thread to listen the tcp socket %i: %s\n",
            errno, strerror(errno));
    return 2;
  }

  // TODO DDOS mitigation: create a promise poll to asynchronous processing the
  // http requests and responses to avoid overload main accepting thread
  char *received_data = malloc(HTTP_REQUEST_DATA_CHUNK_SIZE);
  // wait for connection and data from tcp sockets
  struct tcp_event_data *d;
#define MAXEVENTS 3
  struct epoll_event event[MAXEVENTS];
  for (;;) {
    int fd_tcp_ready = epoll_wait(ctx.fd_epoll, event, MAXEVENTS, 1000);
    if (fd_tcp_ready == -1) {
      fprintf(stderr, "epoll event checking failed %i: %s\n", errno,
              strerror(errno));
    }
    for (int i = 0; i < fd_tcp_ready; i++) {
      d = (struct tcp_event_data *)event[i].data.ptr;
      process_epoll_event(&ctx, d, received_data);
    }
    // TODO DDOS mitigation: wait MAXEVENTS promises to be free on the promises
    // POLL, or timeout?
    printf(".");
    fflush(stdout);
  }
}
