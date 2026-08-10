#define SOCKET_IMPLEMENTATION
#include "socket.h"
#include <stdio.h>

#define PORT "6996"

SOCKET server            = INVALID_SOCKET;
BOOL   serverShouldClose = FALSE;

int consoleCtrlHandler(DWORD ctrlType)
{
  closesocket(server);
  serverShouldClose = TRUE;
  
  return TRUE;
}

int main()
{
  SetConsoleCtrlHandler(consoleCtrlHandler, TRUE);
  WSADATA wsaData;

  // Winsock version 2.2
  int iResult = WSAStartup(MAKEWORD(2, 2), &wsaData);
  if (iResult != 0) {
    fprintf(stderr, "Failed to initialize winsock\n");
    return -1;
  }

  SOCKET server = createSocket(NULL, PORT);
  if (server == INVALID_SOCKET) {
    fprintf(stderr, "Failed to create socket\n");
    
    WSACleanup();
    return -2;
  }
  printf("Server bound to port %s\n", PORT);

  while (!serverShouldClose) {
  
  }

  closesocket(server);
  WSACleanup();
  return 0;
}