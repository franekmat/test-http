#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <stdlib.h>
#include "err.h"

#define MAX_SIZE 1000005

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
  char *port = strtok(NULL, ":"); //może zrobić to ładniej?

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
  int cookies_len;
  FILE *f = fopen(filename, "rb"); //rb??

  if (!f) {
    syserr("read_cookies");
  }

  fseek(f, 0, SEEK_END);
  cookies_len = ftell(f);
  fseek(f, 0, SEEK_SET);
  cookies = malloc(cookies_len);
  if (cookies) {
    fread(cookies, 1, cookies_len, f);
  }
  else {
    syserr("read_cookies");
  }
  fclose(f);

  int ii = 0;
  for (; ii < cookies_len; ii++) {
    if ((cookies)[ii] == '\n') {
      (cookies)[ii] = ';';
    }
  }
  if ((cookies)[strlen(cookies) - 1] == ';') {
    (cookies)[strlen(cookies) - 1] = 0;
  }

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
  char resource[MAX_SIZE];

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

  char *message = malloc(62 + strlen(resource) + strlen(tested_http_address) + strlen(*cookies));
  sprintf(message, "GET %s HTTP/1.1\r\nHost: %s\r\nCookie: %s\r\nConnection: close\n\r\n", resource, tested_http_address, *cookies);

  return message;
}

void send_request(int *sock, char **message) {
  ssize_t len = strnlen(*message, 1000000); //8KB ??
  if (write(*sock, *message, len) != len) {
    syserr("partial / failed write");
  }
}

void receive_header(int *sock, char *buffer) {
  char buffer_tmp[MAX_SIZE];
  ssize_t rcv_len = 1;
  while (rcv_len > 0) {
    memset(buffer_tmp, 0, sizeof(buffer_tmp));
    rcv_len = read(*sock, buffer_tmp, sizeof(buffer_tmp)); //ile ma ten sizeof?

    if (rcv_len > 0) {
      strcat(buffer, buffer_tmp);
    }

    char *pfound = strstr(buffer, "\r\n\r\n");
    if (pfound != NULL) {
      return;
    }
  }
}

int check_ok_status(char *buffer) {
  char status[16];
  strncpy(status, buffer, 15);
  status[15] = '\0';

  if (strcmp("HTTP/1.1 200 OK", status) != 0) {
    return 0;
  }
  return 1;
}

void print_cookies(char *buffer) {
  char tmp_buffer[strlen(buffer) + 1]; //może inaczej? bez rozmiaru?
  int dlen = strlen(buffer);
  if (dlen > 0)
  {
      char *pfound = strstr(buffer, "Set-Cookie: ");
      while (pfound != NULL)
      {
          int pos = pfound - buffer; //iterator pos ustawiamy na 'S'
          pos += strlen("Set-Cookie: ");
          strcpy(tmp_buffer, buffer);
          char *cookie = strtok(tmp_buffer + pos, ";");
          printf ("%s\n", cookie);
          pfound = strstr(buffer + pos + 1, "Set-Cookie:");
      }
  }
}

void extract_substring(char *res, char *s, size_t from, size_t to) {
  memcpy(res, s + from, to - from);
  res[to - from] = '\0';
}

int get_not_chunked_length(char *buffer) {
  char *pfound = strstr(buffer, "Content-Length: ");

  if (pfound != NULL) {
    char *pfound_end = strstr(pfound, "\r\n");
    size_t from = pfound - buffer + strlen("Content-Length: "); //czy int zamiast size_t?
    size_t to = pfound_end - buffer;
    char res[to - from + 1];
    extract_substring(res, buffer, from, to);
    return atoi(res);
  }

  return -1;
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

void just_read_content(int *sock, char *buffer) {
  char buffer_tmp[MAX_SIZE];
  ssize_t rcv_len = 1;

  while (rcv_len > 0) {
    memset(buffer_tmp, 0, sizeof(buffer_tmp));
    rcv_len = read(*sock, buffer_tmp, sizeof(buffer_tmp));
  }
}

int receive_content(int *sock, char *buffer) {
  char buffer_tmp[MAX_SIZE];
  ssize_t rcv_len = 1;
  int ovr_res = 0;

  while (rcv_len > 0) {
    memset(buffer_tmp, 0, sizeof(buffer_tmp));
    rcv_len = read(*sock, buffer_tmp, sizeof(buffer_tmp));

    if (rcv_len > 0) {
      strcat(buffer, buffer_tmp);
    }

    int content_length = get_content_length(buffer);
    int overall_length = (strstr(buffer, "\r\n") - buffer) + strlen("\r\n") + content_length + strlen("\r\n");
    if (content_length == -1) {
      continue;
    }
    int i = 0;
    if (overall_length <= strlen(buffer)) {
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
  char buffer[MAX_SIZE];

  receive_header(sock, buffer);

  if (!check_ok_status(buffer)) {
    printf ("%s\n", strtok(buffer, "\r\n")); //tak naprawde sam slash starczy
    return;
  }

  print_cookies(buffer);

  int content_length = get_not_chunked_length(buffer);

  if (content_length == -1) {
    cut_header(buffer);
    content_length = receive_content(sock, buffer);
  }
  else {
    just_read_content(sock, buffer);
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

  free(cookies);
  free(message);

  return 0;
}
