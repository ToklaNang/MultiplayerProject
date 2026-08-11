#define GLOBAL_IMPLEMENTATION
#include <pthread.h>
#include "global.h"
#include <raymath.h>
#include <vector.h>
#include <stdlib.h>
#include <time.h>

#define WINDOW_WIDTH  1280
#define WINDOW_HEIGHT 720
#define WINDOW_TITLE  "MP Proj"

typedef struct threadArgs         threadArgs;
typedef struct drawQueue          drawQueue;
typedef _vectorObject(playerInfo) vecPlayerInfo;

struct threadArgs {
  char *ipAddr; 
  char *port;
};
struct drawQueue {
  playerInfo items[256];
  int tail, head;
};

drawQueue     dq;
playerInfo    player;
SOCKET        client            = INVALID_SOCKET;
BOOL          clientShouldClose = FALSE;

void queueClear(drawQueue *q) { q->head = q->tail = 0; }
bool queueEmpty(drawQueue *q) { return q->head == q->tail; }
playerInfo queuePop(drawQueue *q) { return q->items[q->head++ % 256]; }
void queuePush(drawQueue  *q, playerInfo x)
{
  if (q->tail - q->head > 256) return;
  q->items[q->tail++ % 256] = x;
}

void *handleServerConnection(void *args)
{
  threadArgs ta = *(threadArgs*)args;
  WSADATA    wsaData;

  // Winsock version 2.2
  int iResult = WSAStartup(MAKEWORD(2, 2), &wsaData);
  if (iResult != 0) {
    fprintf(stderr, "Failed to initialize winsock\n");
    return NULL;
  }

  client = createSocket(ta.ipAddr, ta.port);
  if (client == INVALID_SOCKET) {
    fprintf(stderr, "Failed to create socket\n");
    
    WSACleanup();
    return NULL;
  }

  requestInfo ri;

  // Entry request
  ri.payload = serializePlayer(player);
  ri.type    = REQUEST_TYPE_ENTRY;
  ri.len     = sizeof(player);
  sendRequest(client, ri); free(ri.payload);

  // Get server assigned id
  uint32_t net;
  recv(client, (char*)&net, sizeof(net), 0);

  player.id = ntohl(net);

  while (!clientShouldClose) {
    ri.payload = serializePlayer(player);
    ri.type    = REQUEST_TYPE_UPDATE;
    ri.len     = sizeof(player);
    sendRequest(client, ri); free(ri.payload);

    ri.len     = 0;
    ri.payload = NULL;
    ri.type    = REQUEST_TYPE_GETSTATE;
    sendRequest(client, ri);

    uint32_t net;     recv(client, (char*)&net, sizeof(net), 0);
    uint32_t pCount = ntohl(net);

    for (int n = 0; n < pCount; n++) {
      char packet[sizeof(player)];
      recv(client, packet, sizeof(packet), 0);

      playerInfo p = unserializePlayer(packet);
      queuePush(&dq, p);
    }
  }

  // Exit request
  ri.payload = serializePlayer(player);
  ri.type    = REQUEST_TYPE_EXIT;
  ri.len     = sizeof(player);
  sendRequest(client, ri); free(ri.payload);

  closesocket(client);
  WSACleanup();
}
int consoleCtrlHandler(DWORD ctrlType)
{
  clientShouldClose = TRUE; 
  return TRUE;
}

int main(int argc, char **argv)
{
  if (argc < 3) {
    fprintf(stderr, "Expected 2 arguments (ip, port)\n");
    return -1;
  }
  SetConsoleCtrlHandler(consoleCtrlHandler, TRUE);
  
  queueClear(&dq);
  srand(time(NULL));
  player.id    = 0; // Not assigned yet
  player.color = (Color)  {rand() % 255, rand() % 255, rand() % 255, 255};
  player.size  = (Vector2){100.0f, 100.0f};
  player.pos   = (Vector2){WINDOW_WIDTH / 2.0f - 50.0f, 20.0f};

  float   gravity  = 9.81f;
  bool    onGround = false;
  Vector2 velocity = {0.0f, 0.0f};

  // Create another thread to handle network while the
  // main thread run the game
  pthread_t worker;

  threadArgs args;
  args.ipAddr = argv[1];
  args.port   = argv[2];

  pthread_create(&worker, NULL, handleServerConnection, (void*)&args);

  SetTraceLogLevel(LOG_WARNING);
  
  InitWindow(WINDOW_WIDTH, WINDOW_HEIGHT, WINDOW_TITLE);
  SetWindowState(FLAG_VSYNC_HINT);

  while (!WindowShouldClose() || clientShouldClose) {
    BeginDrawing();
    ClearBackground(DARKGRAY);

    if (IsKeyDown(KEY_A))
      velocity.x -= 100.0f;
    if (IsKeyDown(KEY_D))
      velocity.x += 100.0f;
    if (IsKeyPressed(KEY_SPACE) && onGround)
      velocity.y -= 450.0f;

    velocity.x *= 0.75f;
    velocity.y += gravity;

    Vector2 step = Vector2Scale(velocity, GetFrameTime());
    if (player.pos.y + player.size.y + step.y >= WINDOW_HEIGHT) {
      float penetration = (player.pos.y + player.size.y + step.y) - WINDOW_HEIGHT;

      Vector2 norm = {0.0f, -1.0f};
      step = Vector2Add(step, Vector2Scale(norm, penetration));

      velocity.y = 0.0f;
      onGround   = true;
    } else
      onGround = false;

    player.pos = Vector2Add(player.pos, step);

    while (!queueEmpty(&dq)) {
      playerInfo p = queuePop(&dq);
      DrawRectangleV(p.pos, p.size, p.color);
    }
    
    EndDrawing();
  }
  clientShouldClose = TRUE;

  pthread_join(worker, NULL);
  CloseWindow();
  return 0;
}