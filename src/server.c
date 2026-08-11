#define GLOBAL_IMPLEMENTATION
#include "global.h"
#include <vector.h>

typedef _vectorObject(playerInfo) vecPlayerInfo;

SOCKET        server            = INVALID_SOCKET;
vecPlayerInfo players           = _vectorEmpty(vecPlayerInfo);
BOOL          serverShouldClose = FALSE;
uint32_t      serverIdCounter   = 1;

void handleClientRequest(requestInfo ri, SOCKADDR_IN from, int len)
{
  const char *ipAddr = inet_ntoa(from.sin_addr);

  switch (ri.type) {
  case REQUEST_TYPE_ENTRY: {
    playerInfo p = unserializePlayer(ri.payload); 
    p.id         = serverIdCounter++;

    uint32_t net = htonl(p.id);
    sendto(server, (char*)&net, sizeof(net), 0, (SOCKADDR*)&from, len);

    printf("Player %d joined [%s]\n", p.id, ipAddr);
    _pushBack(players, p);
    break;
  }
  case REQUEST_TYPE_UPDATE: {  
    playerInfo p = unserializePlayer(ri.payload);

    int index = -1;

    for (int n = 0; n < players.len; n++) {
      playerInfo currP = _at(players, n);
      if (currP.id != p.id) continue;

      index = n;
      break;
    }

    if (index == -1) break;

    players.items[index] = p;
    break;
  }
  case REQUEST_TYPE_GETSTATE: {
    uint32_t net = htonl(players.len);
    sendto(server, (char*)&net, sizeof(net), 0, (SOCKADDR*)&from, len);

    for (int n = 0; n < players.len; n++) {
      playerInfo currP = _at(players, n);

      char *packet = serializePlayer(currP);
      if (packet == NULL) continue;

      sendto(server, packet, sizeof(currP), 0, (SOCKADDR*)&from, len);
      free(packet);
    }
    break;
  }
  case REQUEST_TYPE_EXIT: {
    playerInfo p = unserializePlayer(ri.payload);

    for (int n = 0; n < players.len; n++) {
      playerInfo currP = _at(players, n);
      if (currP.id != p.id) continue;

      _popIndex(players, n);
      
      printf("Player %d left [%s]\n", p.id, ipAddr);
      break;
    }
    break;
  }
  default: break;
  }

  free(ri.payload);
}
int consoleCtrlHandler(DWORD ctrlType)
{
  closesocket(server);
  serverShouldClose = TRUE;
  
  return TRUE;
}

int main(int argc, char **argv)
{
  if (argc < 2) {
    fprintf(stderr, "Expected an argument (port)\n");
    return -1;
  }

  SetConsoleCtrlHandler(consoleCtrlHandler, TRUE);
  WSADATA wsaData;

  // Winsock version 2.2
  int iResult = WSAStartup(MAKEWORD(2, 2), &wsaData);
  if (iResult != 0) {
    fprintf(stderr, "Failed to initialize winsock\n");
    return -2;
  }

  server = createSocket(NULL, argv[1]);
  if (server == INVALID_SOCKET) {
    fprintf(stderr, "Failed to create socket\n");
    
    WSACleanup();
    return -3;
  }
  printf("Server bound to port %s\n", argv[1]);
  
  requestInfo ri;

  while (!serverShouldClose) {
    // Vertify player data
    for (int n = 0; n < players.len; n++) {
      playerInfo currP = _at(players, n);
    
      if (currP.id > 0) continue;
      
      printf("Unresponsive player removed\n");
      _popIndex(players, n); 
      n = 0; // Reset (Since we modify the list while iterating through)
    }
    memset(&ri, 0, sizeof(ri));

    SOCKADDR_IN from;
    int         len = sizeof(from);

    int recvByte = recvRequestFrom(server, &ri, &from, &len);
    if (recvByte <= 0) continue;

    handleClientRequest(ri, from, len);
  }

  closesocket(server);
  WSACleanup();
  return 0;
}