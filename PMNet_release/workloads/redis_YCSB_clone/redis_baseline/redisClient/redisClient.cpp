///////////////////////////////////////////
// Korakit Seemakhupt (korakit@virginia.edu)
// Client for Redis based on YCSB workload.
// Please only uses this for Baseline setup
///////////////////////////////////////////


#include <iostream>
#include <fstream>
using namespace std;
#include "../socketHandler/socketHandler.h"
#include <errno.h>
#include <string.h>
#include "../redisAdaptorCommon/common.h"
#include "stdlib.h"
#include "stdio.h"
#include <unordered_set>
#include <vector>
#include <string>
#include <chrono>
#include <algorithm>
#include <sys/types.h>
#include <unistd.h>
// Constants

// Number of ACK needed to unblock the request.
// This control number of logs in different PMNet devices and does not control the replication on the server.
#define PMSWITCH_REPLICATION 1

// Address of Redis server or Redis proxy
#define REMOTE_ADDRESS "192.168.1.41"
#define APPLICATION_PORT 50000
#define REDIS_PORT 6379
int downStreamUDPSock = 0;
int downStreamTCPSock = 0;
int seq_no_global = 0;
int last_server_seq = 0;
// These defaults are overwritten by runtime args.
double writeRatio = 0.5;
int numOps = 100000;
int payloadSize = 200;
int usePMSwitch = 1;
int pmSwitch_port = PMSWITCH_PORT;
int dumpTiming = 0;
//////////////////////////////////////////////////
char randomPayload[2000];
uint32_t* timeArray;
unordered_set <string> usedkeys_set;
vector <string> usedkeys_vec;



void initializePayload(){
    int i;
    int* randomPayload_intPtr = (int*)randomPayload;
    for(i=0;i<sizeof(randomPayload)/sizeof(int);i++){
        randomPayload_intPtr[i] = rand();
    }
}

int seqnoToKey(int seqno){
    return seqno;
}

size_t generateWriteRequest(char* appBuff, int seqno, size_t value_size){
    size_t retSize = 0;
    unsigned buffPtr=0;
    // Write the header of the command
    // 3 args for the command, key and value.
    char* setCommandHDS = "*3\r\n$3\r\nSET\r\n";
    // The strnlen does not include the null terminator.
    size_t newStrLen = strnlen(setCommandHDS, 20);
    memcpy((void*)appBuff+buffPtr, setCommandHDS, newStrLen);
    buffPtr += newStrLen;
    // Generate key
    int key = seqnoToKey(seqno);
    char keyString[20];
    size_t keyStringLen = sprintf(keyString, "%d", key);
    // Add generated keys to the set.
    if(usedkeys_set.find(std::string(keyString))==usedkeys_set.end()){
        usedkeys_set.insert(std::string(keyString));
        usedkeys_vec.push_back(std::string(keyString));
    }
    // write size of key
    char keyLengthString[20];
    size_t keyLength_length = sprintf(keyLengthString, "$%d\r\n", keyStringLen);
    memcpy((void*)appBuff+buffPtr, keyLengthString, keyLength_length);
    buffPtr += keyLength_length;
    // write key
    memcpy((void*)appBuff+buffPtr, keyString, keyStringLen);
    buffPtr += keyStringLen;
    // add terminator
    char* cmdTerminator = "\r\n";
    size_t terminator_length = strnlen(cmdTerminator, 5);
    memcpy((void*)appBuff+buffPtr, cmdTerminator, terminator_length);
    buffPtr += terminator_length;

    // write value size

    char valueLengthString[20];
    size_t valueLength_length = sprintf(valueLengthString, "$%d\r\n", value_size);
    memcpy((void*)appBuff+buffPtr, valueLengthString, valueLength_length);
    buffPtr += valueLength_length;

    // write value
    memcpy((void*)appBuff+buffPtr, randomPayload, value_size);
    buffPtr += value_size;
    // add terminator
    //char* cmdTerminator = "\r\n";
    //size_t terminator_length = strnlen(cmdTerminator, 5);
    memcpy((void*)appBuff+buffPtr, cmdTerminator, terminator_length);
    buffPtr += terminator_length;
    retSize = buffPtr;
    return retSize;
}

// Generate GET command in Redis protocol
size_t generateReadRequest(char* appBuff, int seqno){
    size_t retSize = 0;
    unsigned buffPtr=0;
    // Write the header of the command
    // 3 args for the command, key and value.
    char* setCommandHDS = "*2\r\n$3\r\nGET\r\n";
    // The strnlen does not include the null terminator.
    size_t newStrLen = strnlen(setCommandHDS, 20);
    memcpy((void*)appBuff+buffPtr, setCommandHDS, newStrLen);
    buffPtr += newStrLen;
    // get the key
    //int key = seqnoToKey(seqno);
    int randIdx = rand()%usedkeys_vec.size();
    usedkeys_vec[randIdx].c_str();
    char keyString[20];
    strncpy(keyString, usedkeys_vec[randIdx].c_str(),strnlen(usedkeys_vec[randIdx].c_str(),20));
    size_t keyStringLen = strnlen(usedkeys_vec[randIdx].c_str(),20);

    // write size of key
    char keyLengthString[20];
    size_t keyLength_length = sprintf(keyLengthString, "$%d\r\n", keyStringLen);
    memcpy((void*)appBuff+buffPtr, keyLengthString, keyLength_length);
    buffPtr += keyLength_length;
    // write key
    memcpy((void*)appBuff+buffPtr, keyString, keyStringLen);
    buffPtr += keyStringLen;
    // add terminator
    char* cmdTerminator = "\r\n";
    size_t terminator_length = strnlen(cmdTerminator, 5);
    memcpy((void*)appBuff+buffPtr, cmdTerminator, terminator_length);
    buffPtr += terminator_length;
    retSize = buffPtr;
    return retSize;
}

// Check if there are ACK coming from the server, if there is no incoming Server-ACK, stop sending more requests.
int enoughSpace(int seq_no_global, int last_server_seq){
    return (seq_no_global-last_server_seq) >= 64 ? 0 : 1;
}

int runTest(){
    char appBuff[5000];
    char pmSwitchBuff[5000];
    int isWrite = 0;
    int isWriteRequest = 0;
    int requestSize = 0;
    int ctr = 0;
    int numWrite = 0;
    auto start_time = chrono::high_resolution_clock::now();
    auto lastReqEnd_time = start_time;
    auto thisReqEnd_time = start_time;
    while(1){
        if(ctr==0||(((double)numWrite)/ctr)-writeRatio<1e-6){
        //if(isWriteRequest){
        // Generate the request.
            // cerr << "generating write" << endl;
            requestSize = generateWriteRequest(appBuff, ctr, payloadSize);
            isWrite = usePMSwitch;
            numWrite++;
        } else {
            // cerr << "generating read" << endl;
            if(usedkeys_vec.size()<1){
                cerr << "Skipping empty read" << endl;
            }
            requestSize = generateReadRequest(appBuff, ctr);
            //int r1 = socketHandler_send_bytes(downStreamTCPSock, "aaa", 3);
            //cerr << "ret " << r1 << endl;
            isWrite = 0;
        }
        assert(requestSize>0);

        int ret=0;
        if(isWrite){
            // Send Update (SET) request
            // The command is supported by the PMSwitch, wait for enough ACK and continues to the next operation.
            struct pmswitchHeader pmswitch_hds;

            // toPMSwitchBuff
            // inboundRequestLength
            // Copy PMSwitch to output buffer
            size_t sendSize = 0;
            // Send PMNet packet, use sequence number as hashAddr to index the PM in PMNet device.
            sendSize = pmSwitchEncapsulate(pmSwitchBuff, PMSWITCH_OPCODE_PERSIST_NEED_ACK, APPLICATION_PORT, seq_no_global, appBuff, (size_t)requestSize);
            // On our setup, this does okay without timeout+retrans.
            ret = socketHandler_send_bytes(downStreamUDPSock, pmSwitchBuff, sendSize);
            if(ret != sendSize){
                sendErr(0);
            }
            // cerr << "sent to server" << endl;
            int responded=0;
            int responseSize=0;
            int drain = 0;
            while(drain || (responded<PMSWITCH_REPLICATION)){
                ret = socketHandler_recv_bytes(downStreamUDPSock, pmSwitchBuff, sizeof(pmSwitchBuff)); 
                responseSize = ret;
                // cerr << "recved from server" << endl;
                struct pmswitchHeader pm_hds;
                parseHeader(pmSwitchBuff, &pm_hds, responseSize);
                //We need to reject response/ACK of previous requests.
                if((pm_hds.seq_no==seq_no_global) && (APPLICATION_PORT==pm_hds.session_id)){
                    assert(pm_hds.type==PMSWITCH_OPCODE_ACK || pm_hds.type==PMSWITCH_OPCODE_REPONSE);
                    if(pm_hds.type==PMSWITCH_OPCODE_REPONSE){
                        last_server_seq = (pm_hds.seq_no>last_server_seq)?pm_hds.seq_no:last_server_seq;
                    }
                    responded++;
                    
                    //Keep draining ack until we issue more PMNet response from the server.
                    if(!enoughSpace(seq_no_global, last_server_seq)){
                        drain = 1;
                        continue;
                    }
                    drain = 0;
                    // cerr << "Responded" << endl;
                }else{
                    // do nothing, skip responded requests
                    // cerr << "Skipped" << endl;
                    if(pm_hds.type==PMSWITCH_OPCODE_REPONSE){
                        last_server_seq = (pm_hds.seq_no>last_server_seq)?pm_hds.seq_no:last_server_seq;
                    }
                    continue;
                }
            }
            seq_no_global++;

        }else{
            // Send Read (GET) request
            // The command is NOT supported by the PMSwitch, wait for the return from the server.
            struct pmswitchHeader pmswitch_hds;
            size_t sendSize = 0;
            ret = socketHandler_send_bytes(downStreamTCPSock, appBuff, requestSize);
            if(ret != requestSize){
                sendErr(1);
            }
            // cerr << "sent to server" << endl;
            int responded=0;
            int responseSize=0;
            // Expect single response from the server here.

            ret = socketHandler_recv_bytes(downStreamTCPSock, appBuff, sizeof(appBuff)); 
        }
        thisReqEnd_time = chrono::high_resolution_clock::now();
        timeArray[ctr] = (uint32_t)chrono::duration_cast<chrono::microseconds>(thisReqEnd_time - lastReqEnd_time).count();
        lastReqEnd_time = thisReqEnd_time;
        ctr++;
        if(ctr>=numOps){
            break;
        }       
    }
    ofstream statFile;
    
    // if(usePMSwitch){
    //     statFile.open ("stats_" + to_string(writeRatio) + "_pmSwitch_size_" + to_string(payloadSize) + ".txt", ostream::trunc);
    // }else{
    //     statFile.open ("stats_" + to_string(writeRatio) + "_baseline_size_" + to_string(payloadSize) + ".txt", ostream::trunc);
    // }
    
    auto end_time = chrono::high_resolution_clock::now();
    // timeArray
    std::vector<uint32_t>timeVectorMicroFull(timeArray, &timeArray[ctr]);
    std::vector<uint32_t>timeVectorMicro(&timeArray[ctr*10/100], &timeArray[ctr*95/100]);
    int64_t totalTimeMicro = 0;
    int dataPtsCount = (ctr*95/100) - (ctr*10/100);
    for(int k=0;k<timeVectorMicro.size();k++){
        totalTimeMicro += timeVectorMicro[k];
    }
    cout << "totalTime: " << totalTimeMicro << endl;
    cout << "AvgTime: " << (double)totalTimeMicro/dataPtsCount << endl;
    // std::sort(timeVectorMicro.begin(), timeVectorMicro.end());
    // cout << "LowestTime " << timeVectorMicro[0] << ", Longest Time " << timeVectorMicro.back() << endl; 
    // cout << "P95: " << timeVectorMicro[timeVectorMicro.size()*95/100] << ", P99: " << timeVectorMicro[timeVectorMicro.size()*99/100] << endl;
    if(dumpTiming){
        ofstream dumpFile;
        if(usePMSwitch){
            dumpFile.open ("distribution_" + to_string(writeRatio) + "_pmSwitch_size_" + to_string(payloadSize) + ".txt", ostream::trunc);
        }else{
            dumpFile.open ("distribution_" + to_string(writeRatio) + "_baseline_size_" + to_string(payloadSize) + ".txt", ostream::trunc);
        }
        for(int itr=0;itr<timeVectorMicro.size();itr++){
            dumpFile <<timeVectorMicro[itr] << endl;
        }
        dumpFile.close();
        return 0;
    }
    return 0;
}




int main(int argc, char* argv[]){
    // Parse parameters
    char* useErrorMsg = "Use: ./redisClient numOps writeRatio payloadSize usePMNet PMNet_port dumpTiming\n";
    if(argc>1){
        if(argv[1][0]>'9'||argv[1][0]<'0'){
            cerr << useErrorMsg << endl;
            exit(1);
        }
        numOps = atoi(argv[1]);
    }
    if(argc>2){
        if(argv[2][0]>'9'||argv[2][0]<'0'){
            cerr << useErrorMsg << endl;
            exit(1);
        }
        writeRatio = atof(argv[2]);
    }
    if(argc>3){
        if(argv[3][0]>'9'||argv[3][0]<'0'){
            cerr << useErrorMsg << endl;
            exit(1);
        }
        payloadSize = atoi(argv[3]);
    }
    if(argc>4){
        if(argv[4][0]=='-'){
            ;
        }else{
            if(argv[4][0]>'9'||argv[4][0]<'0'){
                cerr << useErrorMsg << endl;
                exit(1);
            }else{
                usePMSwitch = atoi(argv[4]);
            }
        }
    }
    if(argc>5){
        if(argv[5][0]=='-'){
            ;
        }else{
            if(argv[5][0]>'9'||argv[5][0]<'0'){
                cerr << useErrorMsg << endl;
                exit(1);
            }else{
                pmSwitch_port = atoi(argv[5]);
            }
        }
    }
    if(argc>6){
        if(argv[6][0]>'9'||argv[6][0]<'0'){
            cerr << useErrorMsg << endl;
            exit(1);
        }
        dumpTiming = atoi(argv[6]);
    }

    // Prepare space for timing.
    timeArray = (uint32_t*)malloc(numOps*sizeof(uint32_t));


    // Initialize payload.
    initializePayload();
    // Korakit
    // The baseline setup no-PMNet uses direct TCP connection to Redis server. (It's faster, tested.)
    downStreamUDPSock = socketHandler_connect(REMOTE_ADDRESS, pmSwitch_port, DATAGRAM, BLOCKING);
    // The PMNet setup uses UDP to connect to Redis-proxy on the server side.
    downStreamTCPSock = socketHandler_connect(REMOTE_ADDRESS, REDIS_PORT, STREAM, BLOCKING);
    if(downStreamTCPSock==NULL){
        std::cerr << "Cannot connect to the server." << endl;
        exit(1);
    }
    runTest();

}
