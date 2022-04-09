#ifndef _HTTP_STRUCTS_H_
#define _HTTP_STRUCTS_H_
struct tcp_event_data {
  int fd_tcp_client_socket;
  char client_host[50];
  char client_port[50];
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

struct response_cache {
  char *template_content;
} response_cache = {.template_content = 0};

#endif
