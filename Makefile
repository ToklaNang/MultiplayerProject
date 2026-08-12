include = -I ./include
flags   = -static -lws2_32 

cxx = gcc

build-client: src/client.c
	$(cxx) src/client.c raylib.dll $(flags) $(include) -o client.exe

build-server: src/server.c
	$(cxx) src/server.c raylib.dll $(flags) $(include) -o server.exe