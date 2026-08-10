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
#define _writePacket(buff, offset, data) \
do {                                     \
  __typeof__(data) _data   = data;       \
                                         \
  size_t _len = sizeof(_data);           \
  memcpy(buff + offset, &_data, _len);   \
  offset += _len;                        \
} while (0)                             

// Read data from byte
#define _readPacket(buff, offset, outType) \
({                                         \
  outType _data;                           \
                                           \
  size_t _len = sizeof(_data);             \
  memcpy(&_data, buff + offset, _len);     \
  offset += _len; _data;                   \
})


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