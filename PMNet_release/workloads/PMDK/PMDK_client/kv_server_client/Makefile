all: client

client: client.cpp ../kv_server_common/common.h ../socketHandler/socketHandler.c ../socketHandler/socketHandler.h
	g++ client.cpp ../socketHandler/socketHandler.c -O3 -o client

clean:
	-rm client
