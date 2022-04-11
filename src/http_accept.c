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
    char *thread_ret = (char *)malloc(sizeof(char));
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
    char *thread_ret = (char *)malloc(sizeof(char));
    char c = 2;
    memcpy(thread_ret, &c, 1);
    return thread_ret;
  }

  char buf[10];
  getFileContent("/proc/sys/net/ipv4/tcp_max_syn_backlog", &buf[0]);
  int backlog = atoi(buf);
  if (listen(fd_tcp_server_socket, backlog) != 0) {
    fprintf(stderr, "listen fails %i: %s\n", errno, strerror(errno));
    char *thread_ret = (char *)malloc(sizeof(char));
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
  struct tcp_event_data *d =
      (struct tcp_event_data *)malloc(sizeof(struct tcp_event_data));
  int i = getnameinfo((struct sockaddr *)&cli_addr, cli_addrsize,
                      d->client_host, sizeof(d->client_host), d->client_port,
                      sizeof(d->client_port), 0);
  if (i != 0) {
    fprintf(stderr, "fail to get client address information %i: %s\n", i,
            gai_strerror(i));
    return 2;
  }
  printf("\nConnection accepted from %s:%s\n", d->client_host, d->client_port);
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
  req->headers = (struct http_header *)malloc(sizeof(struct http_header));
  req->header_count = 0;
  char parsing_headers = 1;
  for (int i = 0; i < strlen(data); i++) {
    if (parsing_headers) {
      i += lcpy(&data[i], &line[0]);
      if (strlen(line) == 0)
        continue;
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
        continue;
      }
      req->headers = (struct http_header *)realloc(
          req->headers, sizeof(struct http_header) * ++req->header_count);
      // resize readers +1
      struct http_header *h = &req->headers[req->header_count - 1];
      s += wcpy(&line[s], h->name);
      s += lcpy(&line[s + 1], h->value);
      h->name[strlen(h->name) - 1] = '\0'; // remove the last byte ':'
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

int sanatize_path(char *path, char *sanatized_path) {
  sanatized_path[0] = 0;
  char valid_char[256];
  int i;
  for (i = 0; i < 256; i++) {
    valid_char[i] = 0;
  }
  char valid_char_list[66] =
      "0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz-.:/";
  for (i = 0; i < strlen(valid_char_list); i++) {
    valid_char[(int)valid_char_list[i]] = 1;
  }

  fprintf(stderr, "\n-- checking path [%s][%li]\n", path, strlen(path));
  int rv = 0;
  char c;
  char lc = 0;
  int pp = 0;  // path pos
  int hp = 0;  // host pos
  int ptp = 0; // port pos
  char has_protocol = 0;
  char parsing_host = 0;
  char parsing_port = 0;
  char protocol[50];
  protocol[0] = 0;
  char sanatized_host[200];
  sanatized_host[0] = 0;
  char s_port[5];
  s_port[0] = 0;
  int len_path = strlen(path);
  for (i = 0; i < len_path; i++) {
    c = path[i];
    if (c == ':' && protocol[0] == 0) {
      // check if we are at protocol part
      strcpy(protocol, sanatized_path);
      sanatized_path[0] = 0;
      protocol[pp] = 0; // ensure close the str;
      logger_debugf("protocol %s\n", protocol);
      has_protocol = 1;
      pp = 0;
      continue;
    }
    if (has_protocol) {
      if (protocol[0] != 0 && c == ':') {
        parsing_port = 1;
        // parse port
      }
      if (parsing_host && c == '/') {
        parsing_host = 0;
        parsing_port = 0;
        logger_debugf("host %s\n", sanatized_host);
        pp = 0;
      } else if (c == '/' && lc == c && pp == 0) {
        parsing_host = 1;
        pp = 0;
        continue;
      }
    }
    if (!valid_char[(int)c] || (c == '.' && lc == c /* avoid '..' */)) {
      rv = 1;
      break;
    }
    fprintf(stderr, "%i=[%c]", pp, c);
    if (parsing_host) {
      sanatized_host[hp++] = c;
    } else if (parsing_port) {
      s_port[ptp++] = c;
    } else {
      if (pp == 0 && c == '/') { // let's ignore the / on the first char
        if (i > len_path - 2) {
          pp++;
        }
      } else
        sanatized_path[pp++] = c;
    }
    lc = c;
  }
  sanatized_path[pp] = 0;
  s_port[ptp] = 0;
  logger_debugf("sanatize [%i], protocol [%s], host [%s] path [%s]\n", rv,
                protocol, sanatized_host, sanatized_path);
  fflush(stderr);
  return rv;
}

int response_with_server_api(struct context *ctx, int fd, char *path) {
  char buff[1 << 14];
  char json[1 << 13];
  printf("api requested \"%s\"\n", path);
  char sanatized_path[4096];
  char target_sanatized_path[5119];
  logger_debugf("sanatize_path \"%s\"\n", path);
  sanatize_path(path, &sanatized_path[0]);
  snprintf(target_sanatized_path, 5119, "%s%s", ctx->static_path,
           sanatized_path);
  char files[1 << 12];
  files[0] = 0;
  files[1] = 0;
  struct dirent *de;
  DIR *dr = opendir(target_sanatized_path);
  if (dr == NULL) {
    fprintf(stderr, "ERROR: unable to open directory \"%s\"",
            target_sanatized_path);
    sprintf(json, "{ \"status\": \"OPEN_FAIL\", \"path\": \"%s\" }",
            sanatized_path);
  } else {
    while ((de = readdir(dr)) != NULL) {
      sprintf(&files[strlen(files)], ", \"%s\"", de->d_name);
    }
    closedir(dr);
    sprintf(json, "{ \"status\": \"OK\", \"entries\": [ %s ] }", &files[1]);
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

int response_with_list_template(struct context *ctx, int fd) {
  logger_debugf("respond with list template\n");
  if (response_cache.template_content == NULL) {
    response_cache.template_content = (char *)malloc(1024);
    int rv;
    if ((rv = getFileContent("./http-templates/list.html",
                             response_cache.template_content)) < 0) {
      fprintf(stderr, "Request html template list.html fail ret %i\n", rv);
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

int response_with_static_resource(struct context *ctx, int fd, char *filepath) {
  return stream_file_content(fd, filepath);
}

int get_header_value(struct http_request *req, char *header_name, char *buf) {
  int i = 0;
  int rv = -1;
  for (i = 0; i < req->header_count; i++) {
    if (strcmp(req->headers[i].name, header_name) == 0) {
      struct http_header *h = &req->headers[i];
      strcpy(buf, h->value);
      rv = 0;
      break;
    }
  }
  return rv;
}

int process_http_request(struct context *ctx, int fd, char *data_in) {
  struct http_request req;
  int errcode = parse_request(data_in, &req);
  if (errcode > 0) {
    if (errcode == 414) {
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
  char referer_path[4096];
  referer_path[0] = 0;
  get_header_value(&req, "Referer", &referer_path[0]);
  if (strncmp(req.path, "/.api/", 6) == 0) {
    // TODO user Referer header
    return response_with_server_api(ctx, fd, referer_path);
  }
  char relativepath[1 << 15];
  char sanatized_path[1 << 13];
  char sanatized_refer[1 << 13];
  logger_debugf("sanatize refer \"%s\"\n", referer_path);
  sanatize_path(referer_path, &sanatized_refer[0]);
  logger_debugf("sanatized refer \"%s\"\n", sanatized_refer);
  if (sanatized_refer[1] == 0) {
    sanatized_refer[0] = 0;
  } else {
    if (sanatized_refer[0] != 0 &&
        sanatized_refer[strlen(sanatized_refer) - 1] != '/') {
      sanatized_refer[strlen(sanatized_refer)] = '/';
      sanatized_refer[strlen(sanatized_refer)] = 0;
    }
  }
  logger_debugf("sanatize  path: \"%s\"\n", req.path);
  sanatize_path(req.path, &sanatized_path[0]);
  logger_debugf("sanatized  path: \"%s\"\n", sanatized_path);
  if (sanatized_refer[0] != 0 && is_dir(sanatized_refer)) {
    logger_debugf("need to put it relative to referer [%s]", sanatized_refer);
    sprintf(relativepath, "%s%s%s", ctx->static_path, &sanatized_refer[0],
            &sanatized_path[0]);
  } else {
    logger_debugf("no Referer header.");
    sprintf(relativepath, "%s%s", ctx->static_path, &sanatized_path[0]);
  }
  logger_debugf("accessing \"%s\"...\n", relativepath);
  if (is_dir(relativepath)) {
    return response_with_list_template(ctx, fd);
  }
  if ((errcode = access(relativepath, F_OK)) == 0) {
    errcode = stream_file_content(fd, relativepath);
    if (errcode == 0) {
      return 0;
    } else {
      char buf[] = "HTTP/2.0 500 internal server error\n\
Content-Type: text/plain; charset=UTF-8\n\
stream_file_content\n\n";
      long unsigned int rv = write(fd, buf, strlen(buf));
      if (rv != strlen(buf)) {
        fprintf(stderr, "WARN, expected to write %li, but wrote %li\n",
                strlen(buf), rv);
      }
      return 500;
    }
  }
  logger_debugf("error %i accessing %s", errcode, relativepath);
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
