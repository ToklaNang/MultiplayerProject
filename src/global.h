#ifndef GLOBAL_H
#define GLOBAL_H

#define SOCKET_IMPLEMENTATION
#define WIN_API_REDUCED
#include "socket.h"
#include <string.h>
#include <raylib.h>
#include <stdlib.h>
#include <stdio.h>

#define REQUEST_TYPE_ENTRY    0
#define REQUEST_TYPE_EXIT     1
#define REQUEST_TYPE_UPDATE   2
#define REQUEST_TYPE_GETSTATE 3

typedef struct playerInfo  playerInfo;
typedef struct requestInfo requestInfo;

struct playerInfo {
  uint32_t id;
  Color    color;
  Vector2  pos, size;
};
struct requestInfo {
  uint32_t  type;
  size_t    len;
  char     *payload;
};

void sendRequest(SOCKET s, requestInfo ri);
void sendRequestTo(SOCKET s, requestInfo ri, SOCKADDR_IN *to, int len);
int  recvRequest(SOCKET s, requestInfo *out);
int  recvRequestFrom(SOCKET s, requestInfo *out, SOCKADDR_IN *from, int *len);

char       *serializePlayer(playerInfo p);
playerInfo  unserializePlayer(char *packet);

#ifdef GLOBAL_IMPLEMENTATION

void sendRequest(SOCKET s, requestInfo ri)
{
  char   packet[sizeof(uint32_t) + sizeof(size_t) + ri.len];
  size_t offset = 0;

  _writePacket(packet, offset, (uint32_t)htonl(ri.type));
  _writePacket(packet, offset, (size_t)  htonl(ri.len));

  if (ri.len > 0 && ri.payload != NULL)
    memcpy(packet + offset, ri.payload, ri.len);

  send(s, packet, sizeof(packet), 0);
}
void sendRequestTo(SOCKET s, requestInfo ri, SOCKADDR_IN *to, int len)
{
  char   packet[sizeof(uint32_t) + sizeof(size_t) + ri.len];
  size_t offset = 0;

  _writePacket(packet, offset, (uint32_t)htonl(ri.type));
  _writePacket(packet, offset, (size_t)  htonl(ri.len));

  if (ri.len > 0 && ri.payload != NULL)
    memcpy(packet + offset, ri.payload, ri.len);

  sendto(s, packet, sizeof(packet), 0, (SOCKADDR*)to, len);
}
int recvRequest(SOCKET s, requestInfo *out)
{
  char   packet[1024];
  int    recvByte = recv(s, packet, sizeof(packet), 0);
  size_t offset = 0; 

  out->type = ntohl(_readPacket(packet, offset, uint32_t));
  out->len  = ntohl(_readPacket(packet, offset, size_t));
  
  if (out->len <= 0) {
    out->payload = NULL;
    goto endFunc;
  }

  out->payload = (char*)malloc(out->len);
  if (out->payload == NULL)
    return 0;
  memcpy(out->payload, packet + offset, out->len);

  endFunc:
  return recvByte;
}
int recvRequestFrom(SOCKET s, requestInfo *out, SOCKADDR_IN *from, int *len)
{
  char   packet[1024];
  int    recvByte = recvfrom(s, packet, sizeof(packet), 0, (SOCKADDR*)from, len);
  size_t offset = 0; 

  out->type = ntohl(_readPacket(packet, offset, uint32_t));
  out->len  = ntohl(_readPacket(packet, offset, size_t));

  if (out->len <= 0) {
    out->payload = NULL;
    goto endFunc;
  }

  out->payload = (char*)malloc(out->len);
  if (out->payload == NULL)
    return 0;
  memcpy(out->payload, packet + offset, out->len);
  
  endFunc:
  return recvByte;
}
char *serializePlayer(playerInfo p)
{
  char *packet = (char*)malloc(sizeof(p));
  if (packet == NULL)
    return NULL;

  size_t offset = 0;
  _writePacket(packet, offset, htonl(p.id));
  _writePacket(packet, offset, htonf(p.pos.x));
  _writePacket(packet, offset, htonf(p.pos.y));
  _writePacket(packet, offset, htonf(p.size.x));
  _writePacket(packet, offset, htonf(p.size.y));

  _writePacket(packet, offset, p.color.r);
  _writePacket(packet, offset, p.color.g);
  _writePacket(packet, offset, p.color.b);
  _writePacket(packet, offset, p.color.a);

  return packet;
}
playerInfo unserializePlayer(char *packet)
{ 
  playerInfo host   = {0};
  size_t     offset = 0;

  host.id     = ntohl(_readPacket(packet, offset, uint32_t));
  host.pos.x  = ntohf(_readPacket(packet, offset, uint32_t));
  host.pos.y  = ntohf(_readPacket(packet, offset, uint32_t));
  host.size.x = ntohf(_readPacket(packet, offset, uint32_t));
  host.size.y = ntohf(_readPacket(packet, offset, uint32_t));

  host.color.r = _readPacket(packet, offset, uint8_t);
  host.color.g = _readPacket(packet, offset, uint8_t);
  host.color.b = _readPacket(packet, offset, uint8_t);
  host.color.a = _readPacket(packet, offset, uint8_t);

  return host;
}

#endif

#endif