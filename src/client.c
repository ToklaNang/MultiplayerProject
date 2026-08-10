#define SOCKET_IMPLEMENTATION
#include "socket.h"
#include <stdio.h>

SOCKET client            = INVALID_SOCKET;
BOOL   clientShouldClose = FALSE;

int consoleCtrlHandler(DWORD ctrlType)
{
  closesocket(client);
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
  WSADATA wsaData;

  // Winsock version 2.2
  int iResult = WSAStartup(MAKEWORD(2, 2), &wsaData);
  if (iResult != 0) {
    fprintf(stderr, "Failed to initialize winsock\n");
    return -2;
  }

  client = createSocket(argv[1], argv[2]);
  if (client == INVALID_SOCKET) {
    fprintf(stderr, "Failed to create socket\n");
    
    WSACleanup();
    return -3;
  }

  char msg[1024];
  
  while (!clientShouldClose) {
    printf("Enter msg: ");
    
    if (fgets(msg, sizeof(msg), stdin))
      send(client, msg, sizeof(msg), 0);
    else
      break;
  }

  closesocket(client);
  WSACleanup();
  return 0;
}