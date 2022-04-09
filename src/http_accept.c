#include "http_accept.h"

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
    fprintf(stderr, "fail to add client %s:%s to epoll(%i) %i, %i: %s\n",
            d->client_host, d->client_port, fd_epoll, i, errno,
            strerror(errno));
    fprintf(stderr, "fd epoll %i\nfd cli %i", fd_epoll, fd_tcp_client_socket);
    return 3;
  }
  return 0;
}

#include "byte_parser.c"

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
