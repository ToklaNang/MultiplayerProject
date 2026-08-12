#define GLOBAL_IMPLEMENTATION
#include <time.h>
#include <stdio.h>
#include "global.h"
#include <pthread.h>
#include <raymath.h>

#define SERVER_TPS              60
#define CLIENT_TIMEOUT_INTERVAL 300

#define GRAVITY  9.81f
#define AIR_DRAG 0.99f
#define FRICTION 0.85f

uint32_t      serverIdCounter   = 1;
uint32_t      serverCurrTick    = 0;
bool          serverShouldClose = false;
SOCKET        server            = INVALID_SOCKET;
vecPlayerInfo players           = _vectorEmpty(vecPlayerInfo);

void handleClientPacket(packetInfo p, SOCKADDR_IN sender, int len)
{ 
  const char *ipAddr = inet_ntoa(sender.sin_addr);

  switch (p.type) {
  case PKT_REQUEST_JOIN: {
    bool shouldJoin = true;

    for (int n = 0; n < players.len; n++) {
      playerInfo currP = _at(players, n);

      if (strcmp(ipAddr, inet_ntoa(currP.ipAddr.sin_addr)) != 0) 
        continue;
      
      shouldJoin = false; 
      break;
    }

    if (shouldJoin) {
      playerInfo newPlayer;
      newPlayer.id             = serverIdCounter++;
      newPlayer.lastSeen       = serverCurrTick;
      newPlayer.state.onGround = false;
      newPlayer.ipAddr         = sender;
      newPlayer.state.velocity = (Vector2){  0.0f,   0.0f};
      newPlayer.state.position = (Vector2){  0.0f,   0.0f};
      newPlayer.state.scale    = (Vector2){100.0f, 100.0f};
      newPlayer.state.color    = (Color)  {rand() % 255, rand() % 255, rand() % 255, 255};
      
      _pushBack(players, newPlayer);

      uint32_t idNet = htonl(newPlayer.id);
      packetInfo p   = packetServer(PKT_ACCEPT_JOIN, (char*)&idNet, sizeof(idNet));
      sendPacketTo(server, &sender, len, p);
      
      printf("SERVER: Player %d joined [%s]\n", newPlayer.id, ipAddr);
    } else {
      packetInfo p = packetServer(PKT_REJECT_JOIN, "Already in server", 18);
      sendPacketTo(server, &sender, len, p);
    }

    break;
  }
  case PKT_PLAYER_UPDATE: {
    for (int n = 0; n < players.len; n++) {
      playerInfo currP = _at(players, n);

      if (currP.id != p.clientId) continue;
      
      players.items[n].event    = unserializeEvent(p.payload);
      players.items[n].lastSeen = serverCurrTick;
    }
    break;
  }
  case PKT_PLAYER_LEFT: {
    for (int n = 0; n < players.len; n++) {
      playerInfo currP = _at(players, n);

      if (currP.id != p.clientId) continue;

      printf("SERVER: Player %d left [%s]\n", currP.id, ipAddr);
      _popIndex(players, n);
    }
    break;
  }
  default: break;  
  }
}
void *initiateServer(void *args)
{
  WSADATA wsaData;

  int iResult = WSAStartup(MAKEWORD(2, 2), &wsaData);
  if (iResult != 0) {
    fprintf(stderr, "ERROR: Failed to initialize winsock\n");
    serverShouldClose = true;
    return NULL;
  }

  server = createSocket(NULL, (char*)args);
  if (server == INVALID_SOCKET) {
    fprintf(stderr, "ERROR: Failed to create socket\n");
    
    serverShouldClose = true;
    WSACleanup();
    return NULL;
  }
  printf("INFO: Server bound to port %s\n", (char*)args);

  packetInfo p;

  while (!serverShouldClose) {
    // Handle client->server connection
    memset(&p, 0, sizeof(p));

    SOCKADDR_IN sender;
    int         len = sizeof(sender);

    int byteRecv = recvPacketFrom(server, &sender, &len, &p);
    if (byteRecv <= 0) continue;
    
    handleClientPacket(p, sender, len);
  }

  WSACleanup();
}
void playerLoop(playerInfo *p, double deltaTime)
{
  // Register keystroke event
  if (p->event.leftKeyDown && p->state.onGround)
    p->state.velocity.x -= 100.f;
  
  if (p->event.rightKeyDown && p->state.onGround)
    p->state.velocity.x += 100.0f;

  if (p->event.jmpKeyDown && p->state.onGround)
    p->state.velocity.y = -450.0f;

  // Physic
  p->state.velocity.y += GRAVITY;
  p->state.velocity.x *= (p->state.onGround)
    ? FRICTION : AIR_DRAG;

  Vector2 step = Vector2Scale(p->state.velocity, deltaTime);
  
  if (p->state.position.y + p->state.scale.y + step.y >= CLIENT_WINDOW_HEIGHT) {
    float   penetration = (p->state.position.y + p->state.scale.y + step.y) - CLIENT_WINDOW_HEIGHT;
    Vector2 floorNorm   = {0.0f, -1.0f};

    step = Vector2Add(step, Vector2Scale(floorNorm, penetration));

    p->state.velocity.y = 0.0f;
    p->state.onGround   = true;
  } else
    p->state.onGround = false;

  p->state.position = Vector2Add(p->state.position, step);
}
int consoleCtrlHandler(unsigned long ctrlType)
{
  for (int n = 0; n < players.len; n++) {
    playerInfo currP = _at(players, n);

    SOCKADDR_IN addr = currP.ipAddr;
    int         len  = sizeof(addr);

    sendPacketTo(server, &addr, len, packetServer(PKT_FORCE_CLOSE, NULL, 0));
  }

  closesocket(server);
  serverShouldClose = true;
  return true; // Tell window that it's handled
}

int main(int argc, char **argv)
{
  if (argc < 2) {
    fprintf(stderr, "ERROR: Expected an argument (port)\n");
    return -1;
  }
  SetConsoleCtrlHandler(consoleCtrlHandler, true);

  srand(time(NULL));

  // Create another thread to handle the networking while
  // the main thread simulate the game world
  pthread_t worker;
  pthread_create(&worker, NULL, initiateServer, (void*)argv[1]);

  LARGE_INTEGER frequency;
  LARGE_INTEGER previous;
  LARGE_INTEGER current;

  QueryPerformanceFrequency(&frequency);
  QueryPerformanceCounter(&previous);

  double tickInterval = (double)(frequency.QuadPart / SERVER_TPS);
  double accumulator  = 0.0f;

  while (!serverShouldClose) {
    QueryPerformanceCounter(&current);

    double elapses  = (double)(current.QuadPart - previous.QuadPart);
    accumulator    += elapses;
    previous        = current;

    while (accumulator >= tickInterval) {
      // Find player who's unresponsive (lost connection)
      // and remove them from player list
      for (int n = 0; n < players.len; n++) {
        playerInfo currP = _at(players, n);

        if (serverCurrTick - currP.lastSeen >= CLIENT_TIMEOUT_INTERVAL) {
          const char *ipAddr = inet_ntoa(currP.ipAddr.sin_addr);
          printf("SERVER: Player %d got kicked [%s]\n", currP.id, ipAddr);

          _popIndex(players, n);
          n = 0; // Reset and look again
        }
      }

      // Send snapshot of the world to each player
      char payload[sizeof(playerState) * players.len];
        
      for (int n = 0; n < players.len; n++) {
        playerState pState = _at(players, n).state;
        size_t      offset = 0;

        serializePlayer(payload + offset, pState);
        offset += sizeof(pState);
      }
      packetInfo p = packetServer(PKT_WORLD_SNAP, payload, sizeof(payload));

      for (int n = 0; n < players.len; n++) {
        playerInfo currP = _at(players, n);

        SOCKADDR_IN to  = currP.ipAddr;
        int         len = sizeof(to);

        sendPacketTo(server, &to, len, p);
      }

      // Perform player loop (physics, ...)
      for (int n = 0; n < players.len; n++)
        playerLoop(_atP(players, n), 1.0f / SERVER_TPS);
        
      serverCurrTick++;
      accumulator -= tickInterval;
    }
  }

  pthread_join(worker, NULL);
  return 0;
}