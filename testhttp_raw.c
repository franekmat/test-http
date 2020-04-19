#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <stdlib.h>

#include "err.h"

int main(int argc, char *argv[]) {

  int sock, err;
  char *host;
  char *port;
  struct addrinfo addr_hints;
  struct addrinfo *addr_result;

  if (argc < 3) {
    return 1;
    // fatal("Usage: %s <connection_address>:<port> <cache file> <tested_http_address>\n", argv[0]);
  }

  host = strtok(argv[1], ":");
  port = strtok(NULL, ":"); //może zrobić to ładniej?

  memset(&addr_hints, 0, sizeof(struct addrinfo));
  addr_hints.ai_family = AF_INET;
  addr_hints.ai_socktype = SOCK_STREAM;
  addr_hints.ai_protocol = IPPROTO_TCP;
  err = getaddrinfo(host, port, &addr_hints, &addr_result);
  if (err == EAI_SYSTEM) {
    return 1;
    // syserr("getaddrinfo: %s", gai_strerror(err));
  }
  else if (err != 0) {
    return 1;
    // fatal("getaddrinfo: %s", gai_strerror(err));
  }

  sock = socket(addr_result->ai_family, addr_result->ai_socktype, addr_result->ai_protocol);
  if (sock < 0) {
    return 1;
    // syserr("socket");
  }

  if (connect(sock, addr_result->ai_addr, addr_result->ai_addrlen) < 0) {
    return 1;
    // syserr("connect");
  }

  freeaddrinfo(addr_result);

  char *cookies = 0;
  int cookies_len;
  FILE *f = fopen(argv[2], "rb"); //rb??

  if (f) {
    fseek(f, 0, SEEK_END);
    cookies_len = ftell(f);
    fseek(f, 0, SEEK_SET);
    cookies = malloc(cookies_len);
    if (cookies) {
      fread(cookies, 1, cookies_len, f);
    }
    else {
      return 1;
      //errrrror
    }
    fclose(f);
  }
  else {
    return 1;
    //errrrror
  }

  int ii = 0;
  for (; ii < cookies_len; ii++) {
    if (cookies[ii] == '\n') {
      cookies[ii] = ';';
    }
  }
  if (cookies[strlen(cookies) - 1] == ';') {
    cookies[strlen(cookies) - 1] = 0;
  }
  // czy nie są potrzebne spacje po srednikach? czy dobrze to zapisuje?

  // char str[] = "GET / HTTP/1.1\r\nUser-Agent: curl/7.16.3 libcurl/7.16.3 OpenSSL/0.9.7l zlib/1.2.3\r\nHost: www.example.com\r\nAccept-Language: en, mi\r\n\r\n";
  // char str[] = "GET /hello.txt HTTP/1.1\r\nHost: example.com\r\n\r\n";
  char *str = "GET %s HTTP/1.1\r\nHost: %s\r\nCookie: %s\r\nConnection: close\n\r\n\r\n";
  char message[1024];
  sprintf(message, str, "/", argv[3], cookies);
    // printf("\n-----\n%s\n-----\n", message);

  ssize_t len, rcv_len;

  len = strnlen(message, 1000000);

  if (write(sock, message, len) != len) {
    return 1;
    // syserr("partial / failed write");
  }

  char buffer[4096]; //bez podawania rozmiaru lepiej?
  memset(buffer, 0, sizeof(buffer));
  rcv_len = read(sock, buffer, sizeof(buffer) - 1);
  if (rcv_len < 0) {
    return 1;
    // syserr("read");
  }
  // printf("read from socket: %zd bytes: %s\n", rcv_len, buffer);

  // printf("%.*s\n", 15, buffer);
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
    printf ("%s\n", status);
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
