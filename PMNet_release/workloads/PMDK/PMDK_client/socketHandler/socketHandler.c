#include "socketHandler.h"

int socketHandler_listen(int port, int type, int blocking){
    int sockHandler = -1;
    int socketOptions = 0;
    int returnVal=-1;
    struct sockaddr_in addressStruct;
    
    // Create a socket
    
    sockHandler = socket(AF_INET, (type == STREAM) ? SOCK_STREAM : SOCK_DGRAM, 
        blocking ? SOCK_NONBLOCK : 0);
    
    //sockHandler = socket(AF_INET, SOCK_STREAM, 0);
    if(sockHandler < 0){
        fprintf(stderr, "Cannot create a socket\n");
        return -1;
    }

    // Allow reuse of ports
    returnVal = setsockopt(sockHandler, SOL_SOCKET, SO_REUSEADDR | SO_REUSEPORT, 
        &socketOptions, sizeof(socketOptions));
    if(returnVal < 0){
        fprintf(stderr, "Cannot set socket options\n");
        return -1;
    }

    // Bind socket to port
    addressStruct.sin_family = AF_INET;
    addressStruct.sin_addr.s_addr = INADDR_ANY;
    addressStruct.sin_port = htons(port);
    returnVal = bind(sockHandler, (const sockaddr*)&addressStruct, sizeof(addressStruct));
    if(returnVal < 0){
        fprintf(stderr, "Cannot bind the socket\n");
        return -1;
    }
    if(type == DATAGRAM){
        return sockHandler;
    }
    returnVal = listen(sockHandler, MAX_CONNECTION_BACKLOG);
    if(returnVal < 0){
        fprintf(stderr, "Cannot listen the socket\n");
        return -1;
    }
    return sockHandler;
}
// For stream socket
int socketHandler_acceptConnection(int sockHandler, void (*connectionHandler)(int sockHandler2)){
// To be added
    int connectedSocket = -1;
    socklen_t addrlen = 0;
    struct sockaddr_in address_in;
    int returnVal = 0;
    returnVal = accept(sockHandler, (sockaddr*)&address_in, &addrlen);
    if(returnVal < 0){
        fprintf(stderr, "Cannot accept incoming connection\n");
        return -1;
    }
    //do something
    connectionHandler(returnVal);
}

int socketHandler_closeConnection(int sockHandler){
    if(sockHandler > 0){
        fprintf(stderr, "Invalid Socket descriptor\n");
        return -1;
    }
    close(sockHandler);
    return 0;
}

int socketHandler_connect(const char* ip, int port, int type, int blocking){
    int sockHandler = -1;
    int socketOptions = 0;
    int returnVal=-1;
    struct sockaddr_in addressStruct;
    // Create a socket
    sockHandler = socket(AF_INET, (type == STREAM) ? SOCK_STREAM : SOCK_DGRAM, 
        blocking ? SOCK_NONBLOCK : 0);
    if(sockHandler < 0){
        fprintf(stderr, "Cannot create a socket\n");
        return -1;
    }
    
    addressStruct.sin_family = AF_INET;
    addressStruct.sin_port = htons(port);

    // Convert text address into native format
    returnVal = inet_pton(AF_INET, ip, &addressStruct.sin_addr);
    if(returnVal < 0){
        fprintf(stderr, "Cannot translate IP into native format\n");
        return -1;
    }
    returnVal = connect(sockHandler, (const sockaddr*)&addressStruct, sizeof(addressStruct));
    if(returnVal < 0){
        fprintf(stderr, "Cannot connect to the server\n");
        return -1;
    }
    return sockHandler;
}

int socketHandler_recv_bytes(int sockHandler, char* buffer, size_t bufferSize){
    //temp function body
    return read(sockHandler, buffer, bufferSize);
}
int socketHandler_recv_bytes_from(int sockHandler, char* buffer, size_t bufferSize, struct sockaddr_in* addressStruct, size_t* addressStruct_size){
    return recvfrom(sockHandler, buffer, bufferSize, 0, (sockaddr*)addressStruct, (socklen_t*)addressStruct_size);
}

int socketHandler_send_bytes(int sockHandler, char* buffer, size_t bufferSize){
    //temp function body
    return send(sockHandler, buffer, bufferSize, 0);
}

int socketHandler_send_bytes_to(int sockHandler, char* buffer, size_t bufferSize, const char* ip, int port){
    struct sockaddr_in addressStruct;
    addressStruct.sin_family = AF_INET;
    addressStruct.sin_addr.s_addr = INADDR_ANY;
    addressStruct.sin_port = htons(port);
    int native_dest_addr = inet_pton(AF_INET, ip, &addressStruct.sin_addr);
    if(native_dest_addr<1){
        fprintf(stderr, "Invalid Address\n");
    }
    return sendto(sockHandler, buffer, bufferSize, 0, (const sockaddr*)&addressStruct, sizeof(addressStruct));
}
// int socketHandler_recv_bytes_from(int sockHandler, char* buffer, size_t bufferSize, const char* )
#ifndef WIN32
int socketHandler_send_bytes_no_wait(int sockHandler, char* buffer, size_t bufferSize){
    //temp function body
    return send(sockHandler, buffer, bufferSize, MSG_DONTWAIT);
}
#endif
