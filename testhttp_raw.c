#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <stdlib.h>
#include "err.h"

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
  // czy nie są potrzebne spacje po srednikach? czy dobrze to zapisuje?

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

char *set_request(char *tested_http_address, char **resource, char **cookies) {
  if (http_address(tested_http_address)) {
    cut_http(&tested_http_address);
  }
  if (https_address(tested_http_address)) {
    cut_https(&tested_http_address);
  }

  char *x = strchr(tested_http_address, '/');

  if (x != NULL) {
    *resource = malloc(strlen(tested_http_address) * sizeof(char));
    strcpy(*resource, tested_http_address);
    *resource += (x - tested_http_address);
    tested_http_address[x - tested_http_address] = 0;
  }
  else {
    *resource = malloc(sizeof(char));
    *resource[0] = '/';
  }

  char *message = malloc(62 + strlen(*resource) + strlen(tested_http_address) + strlen(*cookies));
  sprintf(message, "GET %s HTTP/1.1\r\nHost: %s\r\nCookie: %s\r\nConnection: close\n\r\n", *resource, tested_http_address, *cookies);

  return message;
}

void send_request(int *sock, char **message) {
  ssize_t len = strnlen(*message, 1000000); //8KB ??
  if (write(*sock, *message, len) != len) {
    syserr("partial / failed write");
  }
}

void receive_response(int *sock, char *buffer) {
  char buffer_tmp[1000005];
  ssize_t rcv_len;

  memset(buffer, 0, sizeof(*buffer)); //ok??
  memset(buffer_tmp, 0, sizeof(buffer_tmp)); //ok??
  rcv_len = read(*sock, buffer, sizeof(buffer));

  while (rcv_len > 0) {
    rcv_len = read(*sock, buffer_tmp, sizeof(buffer_tmp));
    if (rcv_len > 0) {
      strcat(buffer, buffer_tmp);
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

void print_report(char *buffer) {
  char tmp_buffer[strlen(buffer) + 1]; //może inaczej? bez rozmiaru?
  int dlen = strlen(buffer);
  if (dlen > 0)
  {
      char *pfound = strstr(buffer, "Set-Cookie:");
      while (pfound != NULL)
      {
          int pos = pfound - buffer; //iterator pos ustawiamy na 'S'
          pos += 12; //przesunięcie iteratora tuż za 'Set-Cookie: '
          strcpy(tmp_buffer, buffer);
          char *cookie = strtok(tmp_buffer + pos, ";");
          printf ("%s\n", cookie);
          pfound = strstr(buffer + pos + 1, "Set-Cookie:");
      }
  }
  printf ("Dlugosc zasobu: %d\n", 1);
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

int main(int argc, char *argv[]) {
  int sock;
  char *resource, *cookies, *message;
  char buffer[1000005];
  struct addrinfo addr_hints, *addr_result;

  if (argc < 3) {
    fatal("Usage: %s <connection_address>:<port> <cache file> <tested_http_address>\n", argv[0]);
  }

  set_connection(&sock, argv[1], &addr_hints, &addr_result);

  freeaddrinfo(addr_result);

  cookies = read_cookies(argv[2]);
  message = set_request(argv[3], &resource, &cookies);

  send_request(&sock, &message);

  receive_response(&sock, buffer);

  printf ("%s\n", buffer);

  if (!check_ok_status(buffer)) {
    printf ("%s\n", strtok(buffer, "\r"));
  }
  else {
    print_report(buffer);
  }

  return 0;
}
