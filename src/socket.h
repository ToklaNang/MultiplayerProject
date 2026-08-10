#ifndef SOCKET_H
#define SOCKET_H

#include <winsock2.h>
#include <ws2tcpip.h>

// Create a socket
// If hostname is NULL, create server
SOCKET createSocket(const char *hostname, const char *port);

#ifdef SOCKET_IMPLEMENTATION

SOCKET createSocket(const char *hostname, const char *port)
{
  BOOL server = (hostname == NULL)
    ? TRUE : FALSE;

  struct addrinfo *res = NULL,
                   hints;
  
  ZeroMemory(&hints, sizeof(hints));
  hints.ai_family   = AF_INET;
  hints.ai_socktype = SOCK_DGRAM;
  hints.ai_protocol = IPPROTO_UDP;

  if (server)
    hints.ai_flags = AI_PASSIVE;

  int iResult = getaddrinfo(hostname, port, &hints, &res);
  if (iResult != 0)
    return INVALID_SOCKET;

  SOCKET sock = socket(res->ai_family, res->ai_socktype, res->ai_protocol);
  if (sock == INVALID_SOCKET) {
    freeaddrinfo(res);
    return INVALID_SOCKET;
  }

  if (server) {
    iResult = bind(sock, res->ai_addr, res->ai_addrlen);
    freeaddrinfo(res);
    
    if (iResult != 0)
      return INVALID_SOCKET;

    return sock;
  }
  connect(sock, res->ai_addr, res->ai_addrlen);
  freeaddrinfo(res);

  return sock;
}

#endif

#endif