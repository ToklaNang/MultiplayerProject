#define GLOBAL_IMPLEMENTATION
#include "global.h"

#define GLOBAL_IMPLEMENTATION
#include <pthread.h>
#include "global.h"
#include <stdio.h>

typedef struct threadArgs threadArgs;
typedef struct drawQueue  drawQueue;

struct threadArgs {
  const char *ipAddr;
  const char *port;
};
struct drawQueue {
  playerState items[256];
  int         tail, head;
};

eventInfo currEvent;
uint32_t  assignedId        = 0;
SOCKET    client            = INVALID_SOCKET;
bool      clientIsReady     = false;
bool      clientShouldClose = false;

vecPlayerState prevState = _vectorEmpty(vecPlayerState);
vecPlayerState currState = _vectorEmpty(vecPlayerState);
bool           isWriting = false;

void queueClear(drawQueue *q)      { q->head = q->tail = 0; }
bool queueEmpty(drawQueue *q)      { return q->head == q->tail; }
playerState queuePop(drawQueue *q) { return q->items[q->head++ % 256]; }
void queuePush(drawQueue  *q, playerState x)
{
  if (q->tail - q->head > 256) return;
  q->items[q->tail++ % 256] = x;
}
void terminateClient()
{
  sendPacket(client, packetClient(PKT_PLAYER_LEFT, assignedId, NULL, 0));

  closesocket(client);
  clientShouldClose = true;
}
void *handleClientConnection(void *args)
{
  WSADATA wsaData;

  int iResult = WSAStartup(MAKEWORD(2, 2), &wsaData);
  if (iResult != 0) {
    clientShouldClose = true;
    return NULL;
  }
  threadArgs inputArgs = *(threadArgs*)args;

  client = createSocket(inputArgs.ipAddr, inputArgs.port);
  if (client == INVALID_SOCKET) {
    fprintf(stderr, "ERROR: Failed to create socket\n");
    
    clientShouldClose = true;
    WSACleanup();
    return NULL; 
  }

  // Halt the client to not join the server until the
  // Client GUI complete it's setup
  while (!clientIsReady) {}

  // Request to join server
  sendPacket(client, packetClient(PKT_REQUEST_JOIN, 0, NULL, 0));
  packetInfo p;

  int byteRecv = recvPacket(client, &p);
  if (byteRecv <= 0) {
    clientShouldClose = true;
    return NULL;
  }

  if (p.type == PKT_REJECT_JOIN) {
    char *rejectMsg = p.payload;
    
    fprintf(stderr, "CLIENT: Reject by server (Reason: %s)\n", rejectMsg);
    clientShouldClose = true;
    return NULL;
  }
  assignedId = ntohl(*(uint32_t*)p.payload);

  while (!clientShouldClose) {
    char eventPacket[sizeof(currEvent)];
    serializeEvent(eventPacket, currEvent);
    sendPacket(client, packetClient(PKT_PLAYER_UPDATE, assignedId, eventPacket, sizeof(eventPacket)));

    packetInfo p;
    recvPacket(client, &p);

    switch (p.type) {
    case PKT_FORCE_CLOSE: {
      terminateClient();
      break;
    }
    case PKT_WORLD_SNAP: {
      isWriting = true;
      
      int    amount = p.payloadSize / sizeof(playerState);
      size_t offset = 0;

      _vecDes(currState);
      for (int n = 0; n < amount; n++) {
        playerState state = unserializePlayer(p.payload + offset);
        _pushBack(currState, state);
      }

      isWriting = false;

      _vecDes(prevState);
      for (int n = 0; n < currState.len; n++)
        _pushBack(prevState, _at(currState, n));
      
      break;
    }
    default: break;
    }
  }

  WSACleanup();
  return NULL;
}
int consoleCtrlHandler(unsigned long ctrlType)
{
  terminateClient();
  return true; // Tell window that it's handled
}

int main(int argc, char **argv)
{
  if (argc < 3) {
    fprintf(stderr, "ERROR: Expected an argument (ip port)\n");
    return -1;
  }
  SetConsoleCtrlHandler(consoleCtrlHandler, true);

  struct threadArgs args;
  args.ipAddr = argv[1];
  args.port   = argv[2];

  // Create another thread to handle client->server connection
  // while the main thread handle main game
  pthread_t worker;
  pthread_create(&worker, NULL, handleClientConnection, (void*)&args);

  SetTraceLogLevel(LOG_NONE);
  InitWindow(CLIENT_WINDOW_WIDTH, CLIENT_WINDOW_HEIGHT, "Client");
  SetTargetFPS(60);

  clientIsReady = true;
  while (!WindowShouldClose() && !clientShouldClose) {
    BeginDrawing();
    ClearBackground(WHITE);

    currEvent.leftKeyDown  = (IsKeyDown(KEY_A))     ? true : false;
    currEvent.rightKeyDown = (IsKeyDown(KEY_D))     ? true : false;
    currEvent.jmpKeyDown   = (IsKeyDown(KEY_SPACE)) ? true : false;

    // If the world states is updating, fall back
    // to draw using the previus state instead to
    // avoid reading/writing at the same time
    if (isWriting) {
      for (int n = 0; n < prevState.len; n++) {
        playerState ps = _at(prevState, n);

        DrawRectangleV(ps.position, ps.scale, ps.color);
      }
    } else {
      for (int n = 0; n < currState.len; n++) {
        playerState ps = _at(currState, n);

        DrawRectangleV(ps.position, ps.scale, ps.color);
      }
    }

    EndDrawing();
  }
  terminateClient();

  pthread_join(worker, NULL);
  return 0;
}