#ifndef SOCKET_H
#define SOCKET_H

// This header is a small header which provide helper
// function such as htonf, ntonf which the winsock2 here
// doesn't provide. It also provide native window messagebox

#ifdef WIN32_API_REDUCED
#define WIN32_LEAN_AND_MEAN
#define NOUSER
#define NOGDI
#endif

#include <winsock2.h>
#include <ws2tcpip.h>
#include <stdbool.h>
#include <string.h>
#include <stdint.h>

#define _writeToByte(_buff, _offset, _data) \
do {                                        \
  __typeof__(_data) __data = _data;         \
                                            \
  size_t __len = sizeof(__data);            \
  memcpy(_buff + _offset, &__data, __len);  \
  _offset += __len;                         \
} while (0)

#define _readFromByte(_buff, _offset, _type) \
({                                           \
  _type __data;                              \
                                             \
  size_t __len = sizeof(__data);             \
  memcpy(&__data, _buff + _offset, __len);   \
  _offset += __len; __data;                  \
})

uint32_t htonf(float host);
float    ntohf(uint32_t net);
SOCKET   createSocket(const char *hostname, const char *port);

#ifdef SOCKET_IMPLEMENTATION

uint32_t htonf(float host)
{
  uint32_t net;

  memcpy(&net, &host, sizeof(net));
  return htonl(net);
}
float ntohf(uint32_t net)
{
  uint32_t hostl = ntohl(net);
  float    host;

  memcpy(&host, &hostl, sizeof(host));
  return host;
}
SOCKET createSocket(const char *hostname, const char *port)
{
  SOCKET sock     = INVALID_SOCKET;
  bool   isServer = (hostname == NULL)
    ? true : false;

  struct addrinfo *res = NULL,
                   hints;

  ZeroMemory(&hints, sizeof(hints));
  hints.ai_family   = AF_INET;
  hints.ai_socktype = SOCK_DGRAM;
  hints.ai_protocol = IPPROTO_UDP;

  if (isServer)
    hints.ai_flags = AI_PASSIVE;

  int iResult = getaddrinfo(hostname, port, &hints, &res);
  if (iResult != 0)
    goto endFunc;

  sock = socket(res->ai_family, res->ai_socktype, res->ai_protocol);
  if (sock == INVALID_SOCKET)
    goto endFunc;

  if (isServer) {
    int iResult = bind(sock, res->ai_addr, res->ai_addrlen);
    
    if (iResult != 0) {
      freeaddrinfo(res);
      goto endFunc;
    }

  } else 
    connect(sock, res->ai_addr, res->ai_addrlen);

  freeaddrinfo(res);

  endFunc:
  return sock;
}


#endif

#endif