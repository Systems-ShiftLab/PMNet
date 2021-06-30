//#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <sys/types.h>  //optional
#include <sys/socket.h>
#include <netinet/in.h> //For hton and sockaddr_in struct
#include <arpa/inet.h>

#define STREAM 1
#define DATAGRAM 0
#define MAX_CONNECTION_BACKLOG 8
#define BLOCKING 0
#define NONBLOCKING 1
int socketHandler_listen(int port, int type, int blocking);
int socketHandler_acceptConnection(int sockHandler, void (*connectionHandler)(int sockHandler2));
int socketHandler_closeConnection(int sockHandler);
int socketHandler_connect(const char* ip, int port, int type, int blocking);
int socketHandler_recv_bytes(int sockHandler, char* buffer, size_t bufferSize);
int socketHandler_send_bytes(int sockHandler, char* buffer, size_t bufferSize);
int socketHandler_send_bytes_no_wait0(int sockHandler, char* buffer, size_t bufferSize);
int socketHandler_send_bytes_to(int sockHandler, char* buffer, size_t bufferSize, const char* ip, int port);
int socketHandler_recv_bytes_from(int sockHandler, char* buffer, size_t bufferSize, struct sockaddr_in* addressStruct, size_t* addressStruct_size);