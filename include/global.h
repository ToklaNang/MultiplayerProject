#ifndef GLOBAL_H
#define GLOBAL_H

// This header file is like the bridges between the two
// client & server. It contain global stuff that both client
// and server need, helper function for both side to help
// communicate easily

#define SOCKET_IMPLEMENTATION
#define WIN32_API_REDUCED 
#include "socket.h"
#include <vector.h>
#include <stdlib.h>
#include <raylib.h>
#include <raymath.h>

#define CLIENT_WINDOW_WIDTH  1280
#define CLIENT_WINDOW_HEIGHT 720
#define SERVER_TPS           60

#define GRAVITY  9.81f
#define FRICTION 0.85f

#define PLAYER_MOVE_SPEED 70.0f
#define PLAYER_JUMP_FORCE 450.0f

#define PKT_REQUEST_JOIN  0
#define PKT_ACCEPT_JOIN   1
#define PKT_REJECT_JOIN   2
#define PKT_PLAYER_UPDATE 3
#define PKT_WORLD_SNAP    4
#define PKT_PLAYER_LEFT   5
#define PKT_FORCE_CLOSE   6

typedef struct playerState         playerState; 
typedef struct playerInfo          playerInfo;
typedef struct packetInfo          packetInfo;
typedef struct eventInfo           eventInfo;
typedef _vectorObject(playerInfo)  vecPlayerInfo;
typedef _vectorObject(playerState) vecPlayerState;

struct playerState 
{
  uint32_t id;

  Color   color;
  Vector2 scale;
  bool    onGround;
  Vector2 position;
  Vector2 velocity;
};
struct playerInfo {
  SOCKADDR_IN ipAddr;
  uint64_t    lastSeen;
  playerState state;
};
struct packetInfo {
  uint32_t type;
  uint32_t clientId;

  uint64_t  payloadSize;
  char     *payload;
};

int sendPacket(SOCKET s, packetInfo p);
int recvPacket(SOCKET s, packetInfo *out);
int sendPacketTo(SOCKET s, SOCKADDR_IN *to, int len, packetInfo p);
int recvPacketFrom(SOCKET s, SOCKADDR_IN *from, int *len, packetInfo *out);

packetInfo packetServer(uint32_t type, char *payload, size_t len);
packetInfo packetClient(uint32_t type, uint32_t id, char *payload, size_t len);

void        serializePlayer(char *packet, playerState p);
playerState unserializePlayer(char *packet);

#ifdef GLOBAL_IMPLEMENTATION

int sendPacket(SOCKET s, packetInfo p)
{
  size_t packetSize = sizeof(uint32_t) + // Packet type
                      sizeof(uint32_t) + // Client id
                      sizeof(size_t)   + // Payload size
                      p.payloadSize;     // Payload

  char   packet[packetSize];
  size_t offset = 0;

  _writeToByte(packet, offset, (uint32_t)htonl(p.type));
  _writeToByte(packet, offset, (uint32_t)htonl(p.clientId));
  _writeToByte(packet, offset, (size_t)  htonl(p.payloadSize));

  if (p.payloadSize > 0 && p.payload != NULL)
    memcpy(packet + offset, p.payload, p.payloadSize);

  return send(s, packet, sizeof(packet), 0);
}
int recvPacket(SOCKET s, packetInfo *out)
{
  char   packet[1024];
  int    recvByte = recv(s, packet, sizeof(packet), 0);
  size_t offset   = 0;

  out->type        = ntohl(_readFromByte(packet, offset, uint32_t));
  out->clientId    = ntohl(_readFromByte(packet, offset, uint32_t));
  out->payloadSize = ntohl(_readFromByte(packet, offset, size_t));

  if (out->payloadSize <= 0) {
    out->payload = NULL;
    goto endFunc;
  }

  out->payload = (char*)malloc(out->payloadSize);
  if (out->payload == NULL)
    return 0;
  memcpy(out->payload, packet + offset, out->payloadSize);

  endFunc:
  return recvByte;
}
int sendPacketTo(SOCKET s, SOCKADDR_IN *to, int len, packetInfo p)
{
  size_t packetSize = sizeof(uint32_t) + // Packet type
                      sizeof(uint32_t) + // Client id
                      sizeof(size_t)   + // Payload size
                      p.payloadSize;     // Payload

  char   packet[packetSize];
  size_t offset = 0;

  _writeToByte(packet, offset, (uint32_t)htonl(p.type));
  _writeToByte(packet, offset, (uint32_t)htonl(p.clientId));
  _writeToByte(packet, offset, (size_t)  htonl(p.payloadSize));

  if (p.payloadSize > 0 && p.payload != NULL)
    memcpy(packet + offset, p.payload, p.payloadSize);

  return sendto(s, packet, sizeof(packet), 0, (SOCKADDR*)to, len);
}
int recvPacketFrom(SOCKET s, SOCKADDR_IN *from, int *len, packetInfo *out)
{
  char   packet[1024];
  int    recvByte = recvfrom(s, packet, sizeof(packet), 0, (SOCKADDR*)from, len);
  size_t offset   = 0; 

  out->type        = ntohl(_readFromByte(packet, offset, uint32_t));
  out->clientId    = ntohl(_readFromByte(packet, offset, uint32_t));
  out->payloadSize = ntohl(_readFromByte(packet, offset, size_t));

  if (out->payloadSize <= 0) {
    out->payload = NULL;
    goto endFunc;
  }

  out->payload = (char*)malloc(out->payloadSize);
  if (out->payload == NULL)
    return 0;
  memcpy(out->payload, packet + offset, out->payloadSize);

  endFunc:
  return recvByte;
}
packetInfo packetServer(uint32_t type, char *payload, size_t len)
{
  packetInfo p;
  p.type        = type;
  p.payload     = payload;
  p.payloadSize = len;

  return p;
}
packetInfo packetClient(uint32_t type, uint32_t id, char *payload, size_t len)
{
  packetInfo p;
  p.type        = type;
  p.clientId    = id;
  p.payload     = payload;
  p.payloadSize = len;

  return p;
}
void serializePlayer(char *packet, playerState p)
{
  size_t offset = 0;
  _writeToByte(packet, offset, htonl(p.id));
  _writeToByte(packet, offset, htonf(p.position.x));
  _writeToByte(packet, offset, htonf(p.position.y));
  _writeToByte(packet, offset, htonf(p.velocity.x));
  _writeToByte(packet, offset, htonf(p.velocity.y));
  _writeToByte(packet, offset, htonf(p.scale.x));
  _writeToByte(packet, offset, htonf(p.scale.y));

  _writeToByte(packet, offset, p.onGround);

  _writeToByte(packet, offset, p.color.r);
  _writeToByte(packet, offset, p.color.g);
  _writeToByte(packet, offset, p.color.b);
  _writeToByte(packet, offset, p.color.a);
}
playerState unserializePlayer(char *packet)
{
  playerState result;
  size_t      offset = 0;

  result.id         = ntohl(_readFromByte(packet, offset, uint32_t));
  result.position.x = ntohf(_readFromByte(packet, offset, uint32_t));
  result.position.y = ntohf(_readFromByte(packet, offset, uint32_t));
  result.velocity.x = ntohf(_readFromByte(packet, offset, uint32_t));
  result.velocity.y = ntohf(_readFromByte(packet, offset, uint32_t));
  result.scale.x    = ntohf(_readFromByte(packet, offset, uint32_t));
  result.scale.y    = ntohf(_readFromByte(packet, offset, uint32_t));
  
  result.onGround = _readFromByte(packet, offset, uint8_t);

  result.color.r = _readFromByte(packet, offset, uint8_t);
  result.color.g = _readFromByte(packet, offset, uint8_t);
  result.color.b = _readFromByte(packet, offset, uint8_t);
  result.color.a = _readFromByte(packet, offset, uint8_t);

  return result;
}

#endif

#endif