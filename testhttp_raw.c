#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <stdlib.h>
#include <errno.h>
#include <stdarg.h>

void syserr(const char *fmt, ...)
{
  va_list fmt_args;
  int errno1 = errno;

  fprintf(stderr, "ERROR: ");
  va_start(fmt_args, fmt);
  vfprintf(stderr, fmt, fmt_args);
  va_end(fmt_args);
  fprintf(stderr, " (%d; %s)\n", errno1, strerror(errno1));
  exit(EXIT_FAILURE);
}

void fatal(const char *fmt, ...)
{
  va_list fmt_args;

  fprintf(stderr, "ERROR: ");
  va_start(fmt_args, fmt);
  vfprintf(stderr, fmt, fmt_args);
  va_end(fmt_args);
  fprintf(stderr, "\n");
  exit(EXIT_FAILURE);
}

char *read_cookies(char *filename) {
  char *cookies = 0;
  int cookies_len;
  FILE *f = fopen(filename, "rb"); //rb??

  if (f) {
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
  }
  else {
    syserr("read_cookies");
  }

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

char *set_request(char *tested_http_address, char **cookies) {
  char *message = malloc(66 + strlen(tested_http_address) + strlen(*cookies));
  sprintf(message, "GET %s HTTP/1.1\r\nHost: %s\r\nCookie: %s\r\nConnection: close\n\r\n\r\n", "/", tested_http_address, *cookies);

  return message;
}

void send_request(int *sock, char **message) {
  ssize_t len = strnlen(*message, 1000000); //8KB ??
  if (write(*sock, *message, len) != len) {
    syserr("partial / failed write");
  }
}

int main(int argc, char *argv[]) {

  int sock, err;
  char *host, *port, *cookies, *message;
  char buffer[1000005], buffer_tmp[1000005];
  struct addrinfo addr_hints, *addr_result;
  ssize_t rcv_len;

  if (argc < 3) {
    fatal("Usage: %s <connection_address>:<port> <cache file> <tested_http_address>\n", argv[0]);
  }

  host = strtok(argv[1], ":");
  port = strtok(NULL, ":"); //może zrobić to ładniej?

  memset(&addr_hints, 0, sizeof(struct addrinfo));
  addr_hints.ai_family = AF_INET;
  addr_hints.ai_socktype = SOCK_STREAM;
  addr_hints.ai_protocol = IPPROTO_TCP;
  err = getaddrinfo(host, port, &addr_hints, &addr_result);
  if (err == EAI_SYSTEM) {
    syserr("getaddrinfo: %s", gai_strerror(err));
  }
  else if (err != 0) {
    fatal("getaddrinfo: %s", gai_strerror(err));
  }
  sock = socket(addr_result->ai_family, addr_result->ai_socktype, addr_result->ai_protocol);
  if (sock < 0) {
    syserr("socket");
  }
  if (connect(sock, addr_result->ai_addr, addr_result->ai_addrlen) < 0) {
    syserr("connect");
  }

  freeaddrinfo(addr_result);

  cookies = read_cookies(argv[2]);
  message = set_request(argv[3], &cookies);

  send_request(&sock, &message);

  memset(buffer, 0, sizeof(buffer));
  memset(buffer_tmp, 0, sizeof(buffer_tmp));
  rcv_len = read(sock, buffer, sizeof(buffer));

  printf ("%s\n\n\n\n", buffer);


  while (rcv_len > 0) {
    rcv_len = read(sock, buffer_tmp, sizeof(buffer_tmp));
    // printf("read from socket: %zd bytes\n", rcv_len);
    printf ("%s\n\n\n\n", buffer_tmp);
    write(sock, buffer_tmp, rcv_len); //czy to potrzebne?

    if (rcv_len > 0) {
      strcat(buffer, buffer_tmp);
    }
  }


  char status[20]; //bez rozmiaru?
  strncpy(status, buffer, 15);
  status[15] = '\0';

  char tmp_buffer[sizeof(buffer) + 1]; //może inaczej? bez rozmiaru?


  /*
  Jeśli implementacja przyjmuje ograniczenia na liczbę przyjmowanych ciasteczek
   i ich długość, to ograniczenia te powinny zostać dobrane zgodnie z założeniami
   przyjętymi w standardach HTTP dla rozwiązań ogólnego przeznaczenia. Dodatkowo przy
   liczeniu długości przesyłanego zasobu należy uwzględnić możliwość, że zasób był
   wysłany w częściach (kodowanie przesyłowe chunked).
   */

  if (strcmp("HTTP/1.1 200 OK", status) != 0) {
    printf ("%s\n", strtok(buffer, "\r"));
  }
  else {
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


  return 0;
}
