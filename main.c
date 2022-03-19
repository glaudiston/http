#include <arpa/inet.h>
#include <errno.h>
#include <netdb.h>
#include <netinet/ip.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/epoll.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

// load the file contents on buf argument
// returns <0 error code on error or the bytes read on success
int getFileContent(char *sfile, char *buf) {
  // first get the size of the file
  struct stat statbuf;
  if (stat(sfile, &statbuf) != 0) {
    fprintf(stderr, "file stat fail [%s] %i: %s", sfile, errno,
            strerror(errno));
    return -1;
  }
  FILE *fp = fopen(sfile, "r");
  if (fp == NULL) {
    fprintf(stderr, "file open fail [%s] %i: %s", sfile, errno,
            strerror(errno));
    return -2;
  }
  ssize_t byteCount = 0;
  byteCount += fread(buf, statbuf.st_size, 1, fp);
  if (errno > 0) {
    fprintf(stderr, "fail to read the file [%s] %i: %s", sfile, errno,
            strerror(errno));
    return -3;
  }
  if (fclose(fp) != 0) {
    fprintf(stderr, "fail to close the file [%s] %i: %s", sfile, errno,
            strerror(errno));
  }
  return byteCount;
}

struct tcp_event_data {
  int fd_tcp_client_socket;
  char client_host[50];
  char client_port[50];
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
    fprintf(stderr, "fail to add client %s:%s to epoll %i, %i: %s\n",
            d->client_host, d->client_port, i, errno, strerror(errno));
    fprintf(stderr, "fd epoll %i\nfd cli %i", fd_epoll, fd_tcp_client_socket);
    return 3;
  }
  return 0;
}

struct do_listen_tcp_args {
  int fd_epoll;
};
void *do_listen_tcp(void *args_ptr) {
  struct do_listen_tcp_args *args = (struct do_listen_tcp_args *)args_ptr;

  int portNumber = 8080;
  printf("Starting http ipv4 server on port %i...\n", portNumber);
  fflush(stdout);

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
  addr.sin_port = htons(portNumber);
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
    if ((r = do_accept_tcp_request(fd_tcp_server_socket, args->fd_epoll)) !=
        0) {
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
  char name[50];
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

int process_http_request(int fd, char *data_in) {
  struct http_request req;
  int s = parse_request(data_in, &req);
  if (s > 0) {
    if (s == 414) {
      char buf[] = "HTTP/2.0 414 URI Too long\n\n";
      write(fd, buf, strlen(buf));
      return 0;
    }
    char buf[] = "HTTP/2.0 500 internal server error at request parsing\n\n";
    write(fd, buf, strlen(buf));
    return 0;
  }
  if (strcmp(req.path, "/") == 0) {
    if (response_cache.template_content == NULL) {
      response_cache.template_content = malloc(1024);
      if (getFileContent("./http-templates/list.html",
                         response_cache.template_content) != 0) {
        fprintf(stderr, "Request fail path[%s]", req.path);
        // internal_server_error();
        return 1;
      }
    }
    char buff[6000];
    sprintf(&buff[0], "HTTP/2.0 200 OK\n\
Content-Type: text/html; charset=UTF-8\n\
\n\
%s",
            response_cache.template_content);
    // TODO list files
    printf("\nRESPONSE:\n%s\n", buff);
    ssize_t r = write(fd, buff, strlen(buff));
    printf("wrote r=%li to resp, strlen(buff)=%li", r, strlen(buff));
    if (r != strlen(buff)) {
      return 1;
    }
    return 0;
  }
  char buff[5000] = "HTTP/2.0 404 OK\n\
Content-Type: text/html; charset=UTF-8\n\
\n\
<!doctype html><html><body>Not found</body></html>\r\n";
  ssize_t r = write(fd, buff, strlen(buff));
  if (r != strlen(buff)) {
    return 1;
  }
  return 0;
}

int main() {
  // prepare a poll to manage the connected clients
  int fd_epoll;
  if ((fd_epoll = epoll_create(1)) == -1) {
    fprintf(stderr, "fail to create epoll %i: %s\n", errno, strerror(errno));
    return 2;
  }

  // start the thread to listen the tcp port and wait for http input
  pthread_t thread_tcp_listen;
  pthread_attr_t thread_tcp_listen_attr;
  struct do_listen_tcp_args do_listen_tcp_args = {.fd_epoll = fd_epoll};
  if (pthread_create(&thread_tcp_listen, &thread_tcp_listen_attr, do_listen_tcp,
                     &do_listen_tcp_args) != 0) {
    fprintf(stderr, "fail to create the tcp server accept thread %i: %s\n",
            errno, strerror(errno));
  }
  ssize_t r;
  char *received_data = malloc(1024);
  for (;;) {
    struct epoll_event event[3];
    int fd_tcp_ready = epoll_wait(fd_epoll, event, 3, 1000);
    if (fd_tcp_ready == -1) {
      fprintf(stderr, "epoll event checking failed %i: %s\n", errno,
              strerror(errno));
    }
    for (int i = 0; i < fd_tcp_ready; i++) {
      fflush(stdout);
      struct tcp_event_data *d = (struct tcp_event_data *)event[i].data.ptr;
      printf("reading data from %s:%s\n", d->client_host, d->client_port);
      r = read(d->fd_tcp_client_socket, &received_data[0], 1024);
      if (r > 0) {
        printf("data received from %s:%s: [%s]", d->client_host, d->client_port,
               &received_data[0]);
      }
      if (process_http_request(d->fd_tcp_client_socket, &received_data[0]) !=
          0) {
        fprintf(stderr, "fail to process http request: %s", &received_data[0]);
      }
      if (close(d->fd_tcp_client_socket) != 0) {
        fprintf(stderr, "fail to close client connection on fd %i, %i: %s",
                d->fd_tcp_client_socket, errno, strerror(errno));
      }
      free(event[i].data.ptr);
    }
    printf(".");
    fflush(stdout);
  }
}
