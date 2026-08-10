#define SOCKET_IMPLEMENTATION
#include "socket.h"
#include <string.h>
#include <stdio.h>

SOCKET server            = INVALID_SOCKET;
BOOL   serverShouldClose = FALSE;

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

  char buff[1024];

  while (!serverShouldClose) {
    memset(buff, 0, sizeof(buff));
    
    SOCKADDR_IN from;
    int         len = sizeof(from);
    
    int byteRecv = recvfrom(server, buff, sizeof(buff), 0, (SOCKADDR*)&from, &len);

    if (byteRecv > 0)
      printf("%s", buff);
  }

  closesocket(server);
  WSACleanup();
  return 0;
}