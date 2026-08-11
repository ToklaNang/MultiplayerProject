#ifndef SOCKET_H
#define SOCKET_H

#ifdef WIN_API_REDUCED
#define WIN32_LEAN_AND_MEAN
#define VC_EXTRALEAN
#define NOUSER
#define NOGDI
#endif

#include <winsock2.h>
#include <ws2tcpip.h>
#include <stdint.h>

// Write data to byte
#define _writePacket(_buff, _offset, _data) \
do {                                        \
  __typeof__(_data) __data = _data;         \
                                            \
  size_t __len = sizeof(__data);            \
  memcpy(_buff + _offset, &__data, __len);  \
  _offset += __len;                         \
} while (0)

// Read data from byte
#define _readPacket(_buff, _offset, _outType) \
({                                            \
  _outType __data;                            \
                                              \
  size_t __len = sizeof(__data);              \
  memcpy(&__data, _buff + _offset, __len);    \
  _offset += __len; __data;                   \
})

uint32_t htonf(float value);
float ntohf(uint32_t net);

// Create a socket
// If hostname is NULL, create server
SOCKET createSocket(const char *hostname, const char *port);

#ifdef SOCKET_IMPLEMENTATION

uint32_t htonf(float value)
{
  uint32_t result;
  memcpy(&result, &value, sizeof(result));

  return htonl(result);
}
float ntohf(uint32_t net)
{
  uint32_t host = ntohl(net);
  float   result;
 
  memcpy(&result, &host, sizeof(result));
  return result;
}
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