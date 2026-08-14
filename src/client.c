#define GLOBAL_IMPLEMENTATION
#include "global.h"

#define GLOBAL_IMPLEMENTATION
#define RAYGUI_IMPLEMENTATION
#include <pthread.h>
#include <raygui.h>
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

playerState queuePop(drawQueue *q);
void        queuePush(drawQueue  *q, playerState x);
void        queueClear(drawQueue *q);
bool        queueEmpty(drawQueue *q);

void  terminateClient();
int   consoleCtrlHandler(unsigned long ctrlType);
void *handleClientConnection(void *args);

void playerLoop(playerState *p, float deltaTime);
void drawPlayer(playerState p);

uint64_t  currServerTick    = 0;
SOCKET    client            = INVALID_SOCKET;
bool      clientIsReady     = false;
bool      clientShouldClose = false;

vecPlayerState  prevState = _vectorEmpty(vecPlayerState);
vecPlayerState  currState = _vectorEmpty(vecPlayerState);
playerState     player;
pthread_mutex_t lock;

int main()
{
  SetConsoleCtrlHandler(consoleCtrlHandler, true);

  struct threadArgs args;
  args.ipAddr = "stank-portage.tun.ply.gg";
  args.port   = "47292";

  // Create another thread to handle client->server connection
  // while the main thread handle main game
  pthread_t worker;
  pthread_create(&worker, NULL, handleClientConnection, (void*)&args);

  SetTraceLogLevel(LOG_WARNING);
  SetConfigFlags(FLAG_WINDOW_ALWAYS_RUN);

  InitWindow(CLIENT_WINDOW_WIDTH, CLIENT_WINDOW_HEIGHT, "Client");
  SetTargetFPS(60);

  clientIsReady = true;

  while (!clientShouldClose && !WindowShouldClose()) {
    BeginDrawing();
    ClearBackground(WHITE);

    pthread_mutex_lock(&lock);

    for (int n = 0; n < currState.len; n++)
      drawPlayer(_at(currState, n));
    drawPlayer(player);

    pthread_mutex_unlock(&lock);

    DrawFPS(10, 10);
    EndDrawing();
  }
  terminateClient();

  pthread_join(worker, NULL);
  return 0;
}
void queueClear(drawQueue *q)      
{ 
  q->head = q->tail = 0;
}
bool queueEmpty(drawQueue *q)
{ 
  return q->head == q->tail; 
}
playerState queuePop(drawQueue *q) 
{ 
  return q->items[q->head++ % 256];
}
void queuePush(drawQueue  *q, playerState x)
{
  if (q->tail - q->head > 256) return;
  q->items[q->tail++ % 256] = x;
}
int consoleCtrlHandler(unsigned long ctrlType)
{
  terminateClient();
  return true; // Tell window that it's handled
}
void terminateClient()
{
  sendPacket(client, packetClient(PKT_PLAYER_LEFT, player.id, NULL, 0));

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

  // Spin wait the client until the GUI setup
  // is completed
  while (!clientIsReady);

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
  
  pthread_mutex_lock(&lock);
  player = unserializePlayer(p.payload);
  pthread_mutex_unlock(&lock);

  while (!clientShouldClose) {
    pthread_mutex_lock(&lock);
    playerLoop(&player, 1.0f / SERVER_TPS);

    char payload[sizeof(player)];
    serializePlayer(payload, player);
    sendPacket(client, packetClient(PKT_PLAYER_UPDATE, player.id, payload, sizeof(payload)));
    
    pthread_mutex_unlock(&lock);

    packetInfo p;
    recvPacket(client, &p);

    switch (p.type) {
    case PKT_FORCE_CLOSE: {
      terminateClient();
      break;
    }
    case PKT_WORLD_SNAP: {
      pthread_mutex_lock(&lock);

      vecPlayerState tmp = currState;
      currState          = prevState;
      prevState          = tmp;

      int    amount = p.payloadSize / sizeof(playerState);
      size_t offset = 0;

      _vecDes(prevState);

      currServerTick = ntohl(_readFromByte(p.payload, offset, uint32_t));
      for (int n = 0; n < amount; n++) {
        playerState state  = unserializePlayer(p.payload + offset);
        offset            += sizeof(playerState);

        if (state.id == player.id)
          continue;

        _pushBack(prevState, state);
      }

      pthread_mutex_unlock(&lock);
      
      break;
    }
    default: break;
    }
  }

  WSACleanup();
  return NULL;
}
void playerLoop(playerState *p, float deltaTime)
{
  if (IsKeyDown(KEY_A))
    p->velocity.x -= PLAYER_MOVE_SPEED;
  
  if (IsKeyDown(KEY_D))
    p->velocity.x += PLAYER_MOVE_SPEED;

  if (IsKeyDown(KEY_SPACE) && p->onGround)
    p->velocity.y = -PLAYER_JUMP_FORCE;

  p->velocity.y += GRAVITY;
  p->velocity.x *= FRICTION;

  Vector2 step = Vector2Scale(p->velocity, deltaTime);
  
  if (p->position.y + p->scale.y + step.y >= CLIENT_WINDOW_HEIGHT) {
    float   penetration = (p->position.y + p->scale.y + step.y) - CLIENT_WINDOW_HEIGHT;
    Vector2 floorNorm   = {0.0f, -1.0f};

    step = Vector2Add(step, Vector2Scale(floorNorm, penetration));

    p->velocity.y = 0.0f;
    p->onGround   = true;
  } else
    p->onGround = false;

  p->position = Vector2Add(p->position, step);
}
void drawPlayer(playerState p)
{
  int         fontSize     = 20;
  const char *overheadText = TextFormat("Player %d", p.id);
  int         textWidth    = MeasureText(overheadText, fontSize);

  Vector2 pos = {
    (p.position.x + p.scale.x / 2.0f) -  (textWidth / 2.0f),
    p.position.y - 30.0f  
  };

  DrawText(overheadText, pos.x, pos.y, fontSize, BLACK);
  DrawRectangleV(p.position, p.scale, p.color);
}