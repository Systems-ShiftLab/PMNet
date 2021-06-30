#include <iostream>
#include <map>
#include <chrono>
using namespace std;
#include "../redisAdaptorCommon/common.h"
#include "../socketHandler/socketHandler.h"

#define CIRCULAR_BUFFER_SIZE 16

int numops=0;
int idx=0;

struct requestBufferEntry{
    uint32_t seq_no;
    size_t request_size;
    char request[5000];
};

void parsErr(){
    cerr << "Parsing error, exiting" << endl;
}

int initialIndex(){
    return idx*(1024*1024/64);
}

int main(int argc, char* argv[]){
    int pmSwitchPort = PMSWITCH_PORT;
    char* useErrorMsg = "Use: ./redisServerAdaptor PMSwitch_port numops idx\n";
    if(argc>1){
        if(argv[1][0]>'9'||argv[1][0]<'0'){
            cerr << useErrorMsg << endl;
            exit(1);
        }
        pmSwitchPort = atoi(argv[1]);
    }
    if(argc>2){
        if(argv[2][0]>'9'||argv[2][0]<'0'){
            cerr << useErrorMsg << endl;
            exit(1);
        }
        numops = atoi(argv[2]);
    }
    if(argc>3){
        if(argv[3][0]>'9'||argv[3][0]<'0'){
            cerr << useErrorMsg << endl;
            exit(1);
        }
        idx = atoi(argv[3]);
    }
    std::map<uint32_t, struct requestBufferEntry> reorderBuffer;
    // UDP socket for PMSwitch downstream;
    int PMSwitchupStreamUDPSock = socketHandler_listen(pmSwitchPort, DATAGRAM, BLOCKING);

    int serverSock_fd = socketHandler_connect("127.0.0.1", 6379, STREAM, BLOCKING);
    if(serverSock_fd==NULL){
        std::cerr << "Cannot connect to the server." << endl;
    }
    int ret=0;
    char pmSwitchBuff[5000];
    char toServerBuff[5000];
    int seqNumber = initialIndex();
    struct sockaddr_in addressStruct;
    size_t addr_struct_Size = sizeof(addressStruct);
    size_t recvSize = 0;
    size_t sendSize = 0;
    int port = -1;
    struct pmswitchHeader pmswitch_hds;
    cerr << "Pre-failure phase" << endl;
    while(1){
        
        ret = socketHandler_recv_bytes_from(PMSwitchupStreamUDPSock, pmSwitchBuff, sizeof(pmSwitchBuff), &addressStruct, &addr_struct_Size);
        if(ret==0){
            cerr << "Socket closed, exiting";
            exit(0);
        }
        recvSize = ret;
        port = ntohs(addressStruct.sin_port);
        char src_ip[40];
        inet_ntop(AF_INET, (void*)&addressStruct.sin_addr.s_addr, src_ip, sizeof(src_ip));
        // cerr << "recved from client" << endl;
        
        parseHeader(pmSwitchBuff, &pmswitch_hds, recvSize);
        int requestType = pmswitch_hds.type;
	// Just serve the request for now.
        if(0&&pmswitch_hds.seq_no > seqNumber){
            // There is a gap in sequence number. Either the packet arrives out of order or the packet is missing.
            assert(0);

            continue;
        }else{
            while(1){
                // Process current request.
                // This will do for now.
                size_t requestSize = recvSize;
                ////////////////////////
                size_t payload_size = stripHeader(toServerBuff, pmSwitchBuff, requestSize);
                socketHandler_send_bytes(serverSock_fd, toServerBuff, payload_size);
                // cerr << "sent to server" << endl;

                // Korakit
                // Receive the packet but throw them away to simulate failure.
                size_t server_response_size = socketHandler_recv_bytes(serverSock_fd, toServerBuff, sizeof(toServerBuff));
                
                // Do not respond the client, throw away packets
                //sendSize = pmSwitchEncapsulate(pmSwitchBuff, PMSWITCH_OPCODE_REPONSE, pmswitch_hds.session_id, pmswitch_hds.seq_no, toServerBuff, server_response_size);
                //socketHandler_send_bytes_to(PMSwitchupStreamUDPSock, pmSwitchBuff, sendSize, (const char*)src_ip, port);
                // cerr << "sent to client" << endl;

                seqNumber++;


                // Process requests in the reorder buffer.

                // To be implemented
                if(1){
                    break;
                }
                // DO NOT forget to populate pmswitch_hds with valid header.
            }
        }




        // socketHandler_send_bytes(serverSock_fd, pmSwitchBuff, ret);
        // cerr << "sent to server" << endl;
        // ret = socketHandler_recv_bytes(serverSock_fd, pmSwitchBuff, sizeof(pmSwitchBuff));
        // cerr << "recved from server" << endl;
        // socketHandler_send_bytes_to(PMSwitchupStreamUDPSock, pmSwitchBuff, ret, (const char*)src_ip, port);
        // cerr << "sent to client" << endl;
        if(seqNumber>=numops){
            break;
        }
    }
    cerr << "Recovery phase" << endl;
    seqNumber=initialIndex();
    auto start_time = chrono::high_resolution_clock::now();
    while(1){

                ////////////////////////
                size_t server_response_size = 0;
                char src_ip[40];
                // Send the recovery packets to the client, PMNet device should be able to catch those requests.
                inet_ntop(AF_INET, (void*)&addressStruct.sin_addr.s_addr, src_ip, sizeof(src_ip));
                cerr << "sending recovery" << endl;
                // Send Recovery packet
                size_t sendSize = pmSwitchEncapsulate(pmSwitchBuff, PMSWITCH_OPCODE_RECOVER, pmswitch_hds.session_id, seqNumber, toServerBuff, server_response_size);
                cerr << "recving recovery" << endl;
                socketHandler_send_bytes_to(PMSwitchupStreamUDPSock, pmSwitchBuff, sendSize, (const char*)src_ip, port);
                // cerr << "sent to client" << endl;
                // PMNet will respond with the original request from the client.
                int ret = socketHandler_recv_bytes_from(PMSwitchupStreamUDPSock, pmSwitchBuff, sizeof(pmSwitchBuff), &addressStruct, &addr_struct_Size);
                if(ret<=0){
                    cerr << "recv error" << endl;
                }
                cerr << "recovery received, size:" << ret << endl;
                size_t requestSize = ret;
                ////////////////////////
                // Process the requests normally.
                size_t payload_size = stripHeader(toServerBuff, pmSwitchBuff, requestSize);
                ret = socketHandler_send_bytes(serverSock_fd, toServerBuff, payload_size);
                if(ret<=0){
                    cerr << "recv error" << endl;
                }
                cerr << "sent to server" << endl;

                ret = socketHandler_recv_bytes(serverSock_fd, toServerBuff, sizeof(pmSwitchBuff));
                if(ret<=0){
                    cerr << "recv error" << endl;
                }
                cerr << "recv from server" << endl;
                // Remove invalidation: switch to sequence number based log indexing.
                // Invalidate log entries.
                // sendSize = pmSwitchEncapsulate(pmSwitchBuff, PMSWITCH_OPCODE_REPONSE, pmswitch_hds.session_id, pmswitch_hds.seq_no, toServerBuff, server_response_size);
                // socketHandler_send_bytes_to(PMSwitchupStreamUDPSock, pmSwitchBuff, sendSize, (const char*)src_ip, port);
                // Send invalidation
                /*
                sendSize = pmSwitchEncapsulate(pmSwitchBuff, PMSWITCH_OPCODE_REPONSE, pmswitch_hds.session_id, pmswitch_hds.seq_no, toServerBuff, 0);
                socketHandler_send_bytes_to(PMSwitchupStreamUDPSock, pmSwitchBuff, sendSize, (const char*)src_ip, port);
                cerr << "invalidation sent" << endl;
                */
                seqNumber++;
                if(numops==seqNumber){
                    break;
                }
            }
    auto end_time = chrono::high_resolution_clock::now();
    uint32_t timeTotal = (uint32_t)chrono::duration_cast<chrono::microseconds>(end_time - start_time).count();
    cerr << "RecoveryTime: " << timeTotal << endl;
}
