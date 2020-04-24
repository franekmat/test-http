#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <stdlib.h>
#include "err.h"

#define BUFFER_SIZE 1000005
#define COOKIE_MAX_SIZE 5000
#define COOKIES_MAX_NUMBER 500

int hex2dec(char *value) {
  int len = strlen(value) - 1, base = 1, res = 0;

  for (; len >= 0; len--) {
    if ((value)[len] >= '0' && (value)[len] <= '9') {
      res += ((value)[len] - '0') * base;
      base *= 16;
    }
    else if ((value)[len] >= 'a' && (value)[len] <= 'f') {
      res += ((value)[len] - 'a' + 10) * base;
      base *= 16;
    }
    else if ((value)[len] >= 'A' && (value)[len] <= 'F') {
      res += ((value)[len] - 'A' + 10) * base;
      base *= 16;
    }
  }

  return res;
}

void set_connection(int *sock, char *address_port, struct addrinfo *addr_hints, struct addrinfo **addr_result) {
  char *host = strtok(address_port, ":");
  char *port = strtok(NULL, ":");

  memset(addr_hints, 0, sizeof(struct addrinfo));
  addr_hints->ai_family = AF_INET;
  addr_hints->ai_socktype = SOCK_STREAM;
  addr_hints->ai_protocol = IPPROTO_TCP;

  int err = getaddrinfo(host, port, addr_hints, addr_result);
  if (err == EAI_SYSTEM) {
    syserr("getaddrinfo: %s", gai_strerror(err));
  }
  else if (err != 0) {
    fatal("getaddrinfo: %s", gai_strerror(err));
  }

  *sock = socket((*addr_result)->ai_family, (*addr_result)->ai_socktype, (*addr_result)->ai_protocol);
  if (sock < 0) {
    syserr("socket");
  }
  if (connect(*sock, (*addr_result)->ai_addr, (*addr_result)->ai_addrlen) < 0) {
    syserr("connect");
  }
}

char *read_cookies(char *filename) {
  char *cookies = 0;
  size_t len = 0;
  FILE *fp = fopen(filename, "r");

  if (!fp) {
    syserr("read_cookies");
  }
  fseek(fp, 0, SEEK_END);
  len = ftell(fp);

  if (len == 0) {
    return NULL;
  }

  rewind(fp);

  cookies = calloc(1, len + 1);
  if (!cookies) {
    fclose(fp);
    syserr("read_cookies");
  }

  if (fread(cookies, len, 1, fp) != 1) {
    fclose(fp);
    syserr("read_cookies");
  }

  fclose(fp);

  char tmp[len * 2];

  int i = 0, pos = 0;
  for (; i < len; i++) {
    if (cookies[i] != '\n') {
      tmp[pos] = cookies[i];
      pos++;
    }
    else if (i + 1 < len) {
      tmp[pos] = ';';
      tmp[pos + 1] = ' ';
      pos += 2;
    }
  }
  tmp[pos] = '\0';
  strcpy(cookies, tmp);

  return cookies;
}

int http_address(char *s) {
  if (s[0] == 'h' && s[1] == 't' && s[2] == 't' && s[3] == 'p' && s[4] == ':'
      && s[5] == '/' && s[6] == '/') {
        return 1;
      }
  return 0;
}

int https_address(char *s) {
  if (s[0] == 'h' && s[1] == 't' && s[2] == 't' && s[3] == 'p' && s[4] == 's'
      && s[5] == ':' && s[6] == '/' && s[7] == '/') {
        return 1;
      }
  return 0;
}

void cut_http(char **s) {
  (*s) += 7;
}

void cut_https(char **s) {
  (*s) += 8;
}

char *set_request(char *tested_http_address, char **cookies) {
  if (http_address(tested_http_address)) {
    cut_http(&tested_http_address);
  }
  if (https_address(tested_http_address)) {
    cut_https(&tested_http_address);
  }

  char *x = strchr(tested_http_address, '/');
  char resource[strlen(tested_http_address) + 1];

  if (x != NULL) {
    strcpy(resource, tested_http_address);
    int pos = x - tested_http_address;
    int i = 0;
    for (; pos < strlen(tested_http_address); i++, pos++) {
      resource[i] = tested_http_address[pos];
    }
    resource[i] = '\0';
    tested_http_address[x - tested_http_address] = '\0';
  }
  else {
    resource[0] = '/';
    resource[1] = '\0';
  }

  char *message;
  char *host = strtok(tested_http_address, ":");

  if (*cookies != NULL) {
    int size = strlen("GET  HTTP/1.1\r\nHost: \r\nCookie: \r\nConnection: close\r\n\r\n");
    // printf ("%d + %lu + %lu + %lu\n", size, strlen(resource), strlen(host), strlen(*cookies));
    message = malloc(size + strlen(resource) + strlen(host) + strlen(*cookies));
    sprintf(message, "GET %s HTTP/1.1\r\nHost: %s\r\nCookie: %s\r\nConnection: close\r\n\r\n", resource, host, *cookies);
  }
  else {
    int size = strlen("GET  HTTP/1.1\r\nHost: \r\nConnection: close\r\n\r\n");
    // printf ("%d + %lu + %lu\n", size, strlen(resource), strlen(host));
    message = malloc(size + strlen(resource) + strlen(host));
    sprintf(message, "GET %s HTTP/1.1\r\nHost: %s\r\nConnection: close\r\n\r\n", resource, host);
  }

  // printf ("%s\n", message);

  return message;
}

void send_request(int *sock, char **message) {
  ssize_t len = strlen(*message);
  if (write(*sock, *message, len) != len) {
    syserr("partial / failed write");
  }
}

void receive_header(int *sock, char *buffer) {
  char buffer_tmp[BUFFER_SIZE];
  ssize_t rcv_len = 1;

  while (rcv_len > 0) {
    if (rcv_len != 1) {
      memset(buffer_tmp, 0, rcv_len);
    }
    else {
      memset(buffer_tmp, 0, BUFFER_SIZE);
    }
    rcv_len = read(*sock, buffer_tmp, BUFFER_SIZE - 1);

    if (rcv_len < 0) {
      syserr("read");
    }
    else if (rcv_len > 0) {
      strcat(buffer, buffer_tmp);
    }
    else {
      break;
    }

    char *pfound = strstr(buffer, "\r\n\r\n");
    if (pfound != NULL) {
      return;
    }
  }
}

int check_ok_status(char *buffer) {
  int ok_size = strlen("HTTP/1.1 200 OK");
  char status[ok_size + 1];
  strncpy(status, buffer, ok_size);
  status[ok_size] = '\0';

  if (strcmp("HTTP/1.1 200 OK", status) != 0) {
    return 0;
  }
  return 1;
}

int exist_same_cookie(char cookies_list[COOKIES_MAX_NUMBER][COOKIE_MAX_SIZE], int cookies_cnt, char *cookie) {
  int i = 0;
  for (; i < cookies_cnt; i++) {
    if (strcmp(cookie, cookies_list[i]) == 0) {
      return 1;
    }
  }
  return 0;
}

void get_cookies_from_response(char *buffer, char cookies_list[COOKIES_MAX_NUMBER][COOKIE_MAX_SIZE], int *cookies_cnt) {
  char tmp_buffer[strlen(buffer) + 1];
  size_t dlen = strlen(buffer);
  if (dlen > 0)
  {
    char *pfound = strstr(buffer, "Set-Cookie: ");
    while (pfound != NULL)
    {
      size_t pos = pfound - buffer;
      pos += strlen("Set-Cookie: ");
      strcpy(tmp_buffer, buffer);
      char *cookie = strtok(tmp_buffer + pos, ";");

      if (!exist_same_cookie(cookies_list, *cookies_cnt, cookie)) {
        strcpy(cookies_list[*cookies_cnt], cookie);
        (*cookies_cnt)++;
      }

      pfound = strstr(buffer + pos + 1, "Set-Cookie:");
    }
  }
}

void print_cookies(char *buffer) {
  char cookies_list[COOKIES_MAX_NUMBER][COOKIE_MAX_SIZE];
  int cookies_cnt = 0;

  get_cookies_from_response(buffer, cookies_list, &cookies_cnt);

  int i = 0;
  for (; i < cookies_cnt; i++) {
    printf ("%s\n", cookies_list[i]);
  }
}

void extract_substring(char *res, char *s, size_t from, size_t to) {
  memcpy(res, s + from, to - from);
  res[to - from] = '\0';
}

int check_if_chunked(char *buffer) {
  char *pfound = strstr(buffer, "Transfer-Encoding: chunked");
  if (pfound != NULL) {
    return 1;
  }
  return 0;
}

void cut_header(char *buffer) {
  char *pfound = strstr(buffer, "\r\n\r\n");
  int pos = pfound - buffer + strlen("\r\n\r\n");
  int i = 0;
  while (pos < strlen(buffer)) {
    buffer[i] = buffer[pos];
    i++;
    pos++;
  }
  buffer[i] = '\0';
}

int get_content_length(char *buffer) {
  int from = 0;
  char *pfound_end = strstr(buffer, "\r\n");
  if (pfound_end == NULL) {
    return -1;
  }

  size_t to = pfound_end - buffer;
  char res[to + 1];
  extract_substring(res, buffer, from, to);

  return hex2dec(res);
}

int just_read_content(int *sock) {
  char buffer_tmp[BUFFER_SIZE];
  ssize_t rcv_len = 1;
  int res = 0;

  while (rcv_len > 0) {
    if (rcv_len != 1) {
      memset(buffer_tmp, 0, rcv_len);
    }
    else {
      memset(buffer_tmp, 0, BUFFER_SIZE);
    }
    rcv_len = read(*sock, buffer_tmp, BUFFER_SIZE - 1);
    if (rcv_len < 0) {
      syserr("read");
    }
    res += rcv_len;
  }

  return res;
}

int receive_content(int *sock, char *buffer) {
  char buffer_tmp[BUFFER_SIZE];
  ssize_t rcv_len = 1;
  int ovr_res = 0;

  while (rcv_len > 0 || strlen(buffer) > 0) { //while we read something last time or we have sth in the buffer
    if (rcv_len != 0) {
      if (rcv_len != 1) {
        memset(buffer_tmp, 0, rcv_len);
      }
      else {
        memset(buffer_tmp, 0, BUFFER_SIZE);
      }
      rcv_len = read(*sock, buffer_tmp, BUFFER_SIZE - 1);
      if (rcv_len < 0) {
        syserr("read");
      }
      else if (rcv_len > 0) {
        strcat(buffer, buffer_tmp);
      }
    }

    int content_length = get_content_length(buffer);
    int overall_length = (strstr(buffer, "\r\n") - buffer) + strlen("\r\n") + content_length + strlen("\r\n");

    // printf ("oczekuję %d, mam teraz %lu, a potrzebuję w sumie %d\n", content_length, strlen(buffer), overall_length);

    if (content_length == -1) {
      continue;
    }
    else {
      ssize_t rcv_len2 = 1;
      while (rcv_len2 > 0 && overall_length > strlen(buffer)) {
        overall_length -= strlen(buffer);
        if (rcv_len2 != 1) {
          memset(buffer, 0, rcv_len2);
        }
        else {
          memset(buffer, 0, BUFFER_SIZE);
        }
        rcv_len2 = read(*sock, buffer, BUFFER_SIZE - 1);
        if (rcv_len2 < 0) {
          syserr("read");
        }
      }
      int i = 0;
      ovr_res += content_length;
      // printf ("added %d\n", content_length);
      while (overall_length < strlen(buffer)) {
        buffer[i] = buffer[overall_length];
        i++;
        overall_length++;
      }
      buffer[i] = '\0';
    }
  }

  return ovr_res;
}

void handle_response(int *sock) {
  char buffer[BUFFER_SIZE];
  int content_length;
  receive_header(sock, buffer);

  if (!check_ok_status(buffer)) {
    char *pfound = strstr(buffer, "\r\n");
    int pos = pfound - buffer;
    buffer[pos] = '\0';
    printf ("%s\n", buffer);
    return;
  }

  print_cookies(buffer);

  if (check_if_chunked(buffer)) {
    cut_header(buffer);
    content_length = receive_content(sock, buffer);
  }
  else {
    cut_header(buffer);
    content_length = strlen(buffer) + just_read_content(sock);
  }

  printf ("Dlugosc zasobu: %d\n", content_length);
}

int main(int argc, char *argv[]) {
  int sock;
  struct addrinfo addr_hints, *addr_result = NULL;

  if (argc < 3) {
    fatal("Usage: %s <connection_address>:<port> <cookies_file> <tested_http_address>\n", argv[0]);
  }

  set_connection(&sock, argv[1], &addr_hints, &addr_result);
  freeaddrinfo(addr_result);

  char *cookies = read_cookies(argv[2]);
  char *message = set_request(argv[3], &cookies);

  send_request(&sock, &message);
  handle_response(&sock);

  // printf ("raz\n");
  free(cookies);
  // printf ("dwa\n");
  free(message);
  // printf ("trzy\n");

  return 0;
}
