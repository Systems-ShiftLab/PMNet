////////////////////////////////////////////
// Korakit Seemakhupt (korakit@virginia.edu)
// Server-sided Redis proxy
////////////////////////////////////////////
#include <iostream>
#include <map>
#include <thread>
#include <queue>
#include <string.h>
using namespace std;
#include "../redisAdaptorCommon/common.h"
#include "../socketHandler/socketHandler.h"


// Korakit
// PMNet needs exactly one response packet for each request. However, TCP can merge several responses into a single packet.
// While we can force socket not to wait, if there are messages left in the Tx queue, the responses are still merged.
// Work around: Split each requests to seperate sockets.
#define NUM_CONNECTIONS 64

struct requestMetadata{
    char src_ip[16];
    uint32_t seq_no;
    uint16_t port;
    uint16_t session_id;
};




std::queue<struct requestMetadata> metadataQueue;
volatile int PMSwitchupStreamUDPSock=0;
volatile int serverSock_fd[NUM_CONNECTIONS];
volatile uint64_t indx = 0;
void parsErr(){
    cerr << "Parsing error, exiting" << endl;
}



void send_funct(){
    char pmSwitchBuff[4096];
    char toServerBuff[4096];
    size_t recvSize = 0;
    size_t sendSize = 0;
    uint64_t last = 0;
    while(1){
        struct requestMetadata sendingMeta;
        int isEmpty = 1;
        //cerr << "waiting" << endl;
        while(indx<=last);
        // Deque metadata
        assert(!metadataQueue.empty());
        {
            sendingMeta = metadataQueue.front();
            metadataQueue.pop();
        }
        size_t server_response_size = socketHandler_recv_bytes(serverSock_fd[last%NUM_CONNECTIONS], toServerBuff, sizeof(toServerBuff));
        assert(server_response_size);
        sendSize = pmSwitchEncapsulate(pmSwitchBuff, PMSWITCH_OPCODE_REPONSE, sendingMeta.session_id, sendingMeta.seq_no, toServerBuff, server_response_size);
        assert(sendSize);
        //cerr << sendingMeta.src_ip << endl;
        //cerr << htons(sendingMeta.port) << endl;
        // Korakit
        // Requests are still responded in-order.
        socketHandler_send_bytes_to(PMSwitchupStreamUDPSock, pmSwitchBuff, sendSize, (const char*)sendingMeta.src_ip, (sendingMeta.port));
        //cerr << "done" << endl;
        last++;
    }
}






int main(int argc, char* argv[]){
    int pmSwitchPort = PMSWITCH_PORT;
    char* useErrorMsg = "Use: ./redisServerAdaptor PMSwitch_port\n";
    if(argc>1){
        if(argv[1][0]>'9'||argv[1][0]<'0'){
            cerr << useErrorMsg << endl;
            exit(1);
        }
        pmSwitchPort = atoi(argv[1]);
    }

    // UDP socket for PMSwitch downstream;
    PMSwitchupStreamUDPSock = socketHandler_listen(pmSwitchPort, DATAGRAM, BLOCKING);
    for(int k=0;k<NUM_CONNECTIONS;k++){
        serverSock_fd[k] = socketHandler_connect("127.0.0.1", 6379, STREAM, BLOCKING);
        assert(serverSock_fd[k]);
    }
    
    int ret=0;
    char pmSwitchBuff[4096];
    char toServerBuff[4096];
    int seqNumber = 0;
    struct sockaddr_in addressStruct;
    size_t addr_struct_Size = sizeof(addressStruct);
    //create send thread
    std::thread sender(send_funct);

    //First pass to populate the structs.
    while(1){
        size_t addr_struct_Size = sizeof(addressStruct);
        size_t recvSize = 0;
        size_t sendSize = 0;
        ret = socketHandler_recv_bytes_from(PMSwitchupStreamUDPSock, pmSwitchBuff, sizeof(pmSwitchBuff), &addressStruct, &addr_struct_Size);
        if(ret==0){
            cerr << "Socket closed, exiting";
            exit(0);
        }
        recvSize = ret;
        int port = ntohs(addressStruct.sin_port);
        char src_ip[40];
        inet_ntop(AF_INET, (void*)&addressStruct.sin_addr.s_addr, src_ip, sizeof(src_ip));
        // cerr << "recved from client" << endl;
        struct pmswitchHeader pmswitch_hds;
        parseHeader(pmSwitchBuff, &pmswitch_hds, recvSize);
        size_t requestSize = recvSize;
        ////////////////////////
        size_t payload_size = stripHeader(toServerBuff, pmSwitchBuff, requestSize);
        size_t sent_bytes = socketHandler_send_bytes(serverSock_fd[indx%NUM_CONNECTIONS], toServerBuff, payload_size);
        assert(sent_bytes);
        //cerr << "sent" << endl;
        struct requestMetadata recvingMeta;
        strncpy(recvingMeta.src_ip, src_ip, sizeof(recvingMeta.src_ip));
        recvingMeta.port = port;
        recvingMeta.session_id = pmswitch_hds.session_id;
        recvingMeta.seq_no = pmswitch_hds.seq_no;
        {
            metadataQueue.push(recvingMeta);
        }

        indx++;


    }



}
