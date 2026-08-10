#ifndef GLOBAL_H
#define GLOBAL_H

#define SOCKET_IMPLEMENTATION
#define WIN_API_REDUCED
#include "socket.h"
#include <string.h>
#include <raylib.h>
#include <stdlib.h>
#include <stdio.h>

#define REQUEST_TYPE_ENTRY 0
#define REQUEST_TYPE_EXIT  1

typedef struct playerInfo  playerInfo;
typedef struct requestInfo requestInfo;

struct playerInfo {
  uint32_t id;
  Vector2  pos;
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

Vector2    v2ToHost(Vector2 net);
Vector2    v2ToNet(Vector2 host);
playerInfo playerToHost(playerInfo net);
playerInfo playerToNet(playerInfo host);

char       *serializePlayer(playerInfo p);
playerInfo  unserializePlayer(char *packet);

#ifdef GLOBAL_IMPLEMENTATION

void sendRequest(SOCKET s, requestInfo ri)
{
  char   packet[sizeof(ri)];
  size_t offset = 0;

  _writePacket(packet, offset, (uint32_t)htonl(ri.type));
  _writePacket(packet, offset, (size_t)  htonl(ri.len));
  memcpy(packet + offset, ri.payload, ri.len);

  send(s, packet, sizeof(packet), 0);
}
void sendRequestTo(SOCKET s, requestInfo ri, SOCKADDR_IN *to, int len)
{
  char   packet[sizeof(ri)];
  size_t offset = 0;

  _writePacket(packet, offset, (uint32_t)htonl(ri.type));
  _writePacket(packet, offset, (size_t)  htonl(ri.len));
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

  out->payload = (char*)malloc(out->len);
  if (out->payload == NULL)
    return 0;
  
  memcpy(out->payload, packet + offset, out->len);

  return recvByte;
}
int recvRequestFrom(SOCKET s, requestInfo *out, SOCKADDR_IN *from, int *len)
{
  char   packet[1024];
  int    recvByte = recvfrom(s, packet, sizeof(packet), 0, (SOCKADDR*)from, len);
  size_t offset = 0; 

  out->type = ntohl(_readPacket(packet, offset, uint32_t));
  out->len  = ntohl(_readPacket(packet, offset, size_t));

  out->payload = (char*)malloc(out->len);
  if (out->payload == NULL)
    return 0;

  memcpy(out->payload, packet + offset, out->len);
  return recvByte;
}

Vector2    v2ToHost(Vector2 net)        { return (Vector2){ntohf(net.x), ntohf(net.y)};           }
Vector2    v2ToNet(Vector2 host)        { return (Vector2){htonf(host.x), htonf(host.y)};         }
playerInfo playerToHost(playerInfo net) { return (playerInfo){ntohl(net.id), v2ToHost(net.pos)};  }
playerInfo playerToNet(playerInfo host) { return (playerInfo){htonl(host.id), v2ToNet(host.pos)}; }

char *serializePlayer(playerInfo p)
{
  char       *packet = (char*)malloc(sizeof(p));
  playerInfo  net    = playerToNet(p);
  size_t      offset = 0;

  _writePacket(packet, offset, net.id);
  _writePacket(packet, offset, net.pos.x);
  _writePacket(packet, offset, net.pos.y);

  return packet;
}
playerInfo unserializePlayer(char *packet)
{
  playerInfo net;
  size_t     offset = 0;

  net.id    = _readPacket(packet, offset, uint32_t);
  net.pos.x = _readPacket(packet, offset, float);
  net.pos.y = _readPacket(packet, offset, float);

  return playerToHost(net);
}

#endif

#endif