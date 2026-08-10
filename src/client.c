#define GLOBAL_IMPLEMENTATION
#include <pthread.h>
#include "global.h"

#define WINDOW_WIDTH  1280
#define WINDOW_HEIGHT 720
#define WINDOW_TITLE  "MP Proj"

typedef struct threadArgs threadArgs;

struct threadArgs {
  char *ipAddr; 
  char *port;
};

playerInfo player;
SOCKET     client            = INVALID_SOCKET;
BOOL       clientShouldClose = FALSE;

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
  sendRequest(client, ri);

  // Get server assigned id
  uint32_t net;
  recv(client, (char*)&net, sizeof(net), 0);

  player.id = ntohl(net);

  while (!clientShouldClose) {

  }

  // Exit request
  ri.payload = serializePlayer(player);
  ri.type    = REQUEST_TYPE_EXIT;
  ri.len     = sizeof(player);
  sendRequest(client, ri);

  closesocket(client);
  free(ri.payload);
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

  player.id  = 0; // Not assigned yet
  player.pos = (Vector2){0.0f, 0.0f};

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
    ClearBackground(WHITE);

    EndDrawing();
  }
  clientShouldClose = TRUE;

  pthread_join(worker, NULL);
  CloseWindow();
  return 0;
}