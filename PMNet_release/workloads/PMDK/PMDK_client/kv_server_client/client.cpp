////////////////////////////////////////////////////////////////////////////////////////
// Korakit
// Client for datastore workload with PMDK backend.
////////////////////////////////////////////////////////////////////////////////////////


#include <iostream>
#include <fstream>
using namespace std;
#include "../socketHandler/socketHandler.h"
#include <errno.h>
#include <string.h>
#include "../kv_server_common/common.h"
#include "stdlib.h"
#include "stdio.h"
#include <unordered_set>
#include <vector>
#include <string>
#include <chrono>
#include <algorithm>
#include <sys/types.h>
#include <unistd.h>
#include "kv_protocol.h"
// Constants

#define PMSWITCH_REPLICATION 1
#define REMOTE_ADDRESS "192.168.1.41"
// Dummy constant
#define APPLICATION_PORT 1337
#define NUM_CONNECTIONS 1


int downStreamUDPSock[NUM_CONNECTIONS];
int seq_no_global = 0;
int last_server_seq = 0;
double writeRatio = 0.5;
int numOps = 100000;
int payloadSize = 200;
char randomPayload[2000];
int pmSwitch_port = PMSWITCH_PORT;
int usePMSwitch = 1;
uint32_t* timeArray;
unordered_set <string> usedkeys_set;
vector <string> usedkeys_vec;
int dumpTiming = 0;


void initializePayload(){
    int i;
    // Unlike Redis, the payload needs to be ASCII string safe.
    for(i=0;i<sizeof(randomPayload);i++){
        randomPayload[i] = (rand()%10)+'0';
    }
}

int seqnoToKey(int seqno){
    return seqno*1337%numOps;
}

size_t generateWriteRequest(char* appBuff, int seqno, size_t value_size){
    size_t retSize = 0;
    unsigned buffPtr=0;
    // Write the header of the command
    int* cmdPtr = (int*)(appBuff+buffPtr);
    cmdPtr[0] = CMSG_INSERT;
    buffPtr += sizeof(int);
    // Generate key
    int key = seqnoToKey(seqno);
    char keyString[20];
    size_t keyStringLen = sprintf(keyString, "%d", key);
    // Add generated keys to the set.
    if(usedkeys_set.find(std::string(keyString))==usedkeys_set.end()){
        usedkeys_set.insert(std::string(keyString));
        usedkeys_vec.push_back(std::string(keyString));
    }

    // write key
    memcpy((void*)appBuff+buffPtr, keyString, keyStringLen);
    buffPtr += keyStringLen;
    // add space
    char* cmdSpace = " ";
    size_t space_length = strnlen(cmdSpace, 5);
    memcpy((void*)appBuff+buffPtr, cmdSpace, space_length);
    buffPtr += space_length;

    // write value
    memcpy((void*)appBuff+buffPtr, randomPayload, value_size);
    buffPtr += value_size;
    // add terminator
    char* cmdTerminator = "\n";
    size_t terminator_length = strnlen(cmdTerminator, 5);
    memcpy((void*)appBuff+buffPtr, cmdTerminator, terminator_length);
    buffPtr += terminator_length;
    retSize = buffPtr;
    return retSize;
}

size_t generateReadRequest(char* appBuff, int seqno, int* key){
    size_t retSize = 0;
    unsigned buffPtr=0;
    // Write the header of the command
    // 3 args for the command, key and value.
    // Write the header of the command
    int* cmdPtr = (int*)(appBuff+buffPtr);
    cmdPtr[0] = CMSG_GET;
    buffPtr += sizeof(int);
    // get the key
    //int key = seqnoToKey(seqno);
    
    int randIdx = rand()%usedkeys_vec.size();
    
    sscanf(usedkeys_vec[randIdx].c_str(), "%d", key);
    char keyString[20];
    strncpy(keyString, usedkeys_vec[randIdx].c_str(),strnlen(usedkeys_vec[randIdx].c_str(),20));
    size_t keyStringLen = strnlen(usedkeys_vec[randIdx].c_str(),20);
    
    /*
    char keyString[20];
    strncpy(keyString, "aaa", strnlen("aaa",5));
    size_t keyStringLen = strnlen("aaa",5);
    */
    // write key
    memcpy((void*)appBuff+buffPtr, keyString, keyStringLen);
    buffPtr += keyStringLen;
    // add terminator
    char* cmdTerminator = "\n";
    size_t terminator_length = strnlen(cmdTerminator, 5);
    memcpy((void*)appBuff+buffPtr, cmdTerminator, terminator_length);
    buffPtr += terminator_length;
    retSize = buffPtr;
    return retSize;
}

int enoughSpace(int seq_no_global, int last_server_seq){
    return (seq_no_global-last_server_seq) >= 64 ? 0 : 1;
}

int runTest(){
    char appBuff[5000];
    char pmSwitchBuff[5000];
    int isSupported_command = 0;
    int isWriteRequest = 0;
    int requestSize = 0;
    int ctr = 0;
    int numWrite = 0;
    auto start_time = chrono::high_resolution_clock::now();
    auto lastReqEnd_time = start_time;
    auto thisReqEnd_time = start_time;
    int hashInput=0;
    while(1){

        if(ctr==0||(((double)numWrite)/ctr)-writeRatio<1e-6){
        //if(isWriteRequest){
        // Generate the request.
            // cerr << "generating write" << endl;
            requestSize = generateWriteRequest(appBuff, ctr, payloadSize);
            isSupported_command = usePMSwitch;
            numWrite++;
        } else {
            // cerr << "generating read" << endl;
            if(usedkeys_vec.size()<1){
                cerr << "Skipping empty read" << endl;
            }
            requestSize = generateReadRequest(appBuff, ctr, &hashInput);
            //int r1 = socketHandler_send_bytes(downStreamTCPSock, "aaa", 3);
            //cerr << "ret " << r1 << endl;
            isSupported_command = 0;
        }
        assert(requestSize>0);
        // Check if the command is supported by the PMSwitch or not.
        int ret=0;
            // The command is supported by the PMSwitch, wait for enough ACK and generate return to the application.
            // cerr << "The command is supported." << endl;
            struct pmswitchHeader pmswitch_hds;

            // toPMSwitchBuff
            // inboundRequestLength
            // Copy PMSwitch to output buffer
            size_t sendSize = 0;
            if(isSupported_command){
                sendSize = pmSwitchEncapsulate(pmSwitchBuff, PMSWITCH_OPCODE_PERSIST_NEED_ACK, APPLICATION_PORT, seq_no_global, appBuff, (size_t)requestSize);
                ret = socketHandler_send_bytes(downStreamUDPSock[ctr%NUM_CONNECTIONS], pmSwitchBuff, sendSize);
            }else{
                sendSize = pmSwitchEncapsulate(pmSwitchBuff, PMSWITCH_OPCODE_BYPASS, APPLICATION_PORT, seq_no_global, appBuff, (size_t)requestSize, hashInput);
                ret = socketHandler_send_bytes(downStreamUDPSock[hashInput%NUM_CONNECTIONS], pmSwitchBuff, sendSize);
            }
            
            
            if(ret != sendSize){
                sendErr(0);
            }
            // cerr << "sent to server" << endl;
            int responded=0;
            int responseSize=0;
            int drain = 0;
            while(drain || (isSupported_command&&(responded<PMSWITCH_REPLICATION))||((!isSupported_command)&&(responded<1))){
                if(isSupported_command){
                    ret = socketHandler_recv_bytes(downStreamUDPSock[ctr%NUM_CONNECTIONS], pmSwitchBuff, sizeof(pmSwitchBuff)); 
                 }else{
                    ret = socketHandler_recv_bytes(downStreamUDPSock[hashInput%NUM_CONNECTIONS], pmSwitchBuff, sizeof(pmSwitchBuff)); 
                 }
                responseSize = ret;
                // cerr << "recved from server" << endl;
                struct pmswitchHeader pm_hds;
                parseHeader(pmSwitchBuff, &pm_hds, responseSize);
                // Redis is strictly Request-Response. We only need to reject response/ACK of previous requests.
                if((pm_hds.seq_no==seq_no_global) && (APPLICATION_PORT==pm_hds.session_id)){
                    assert(pm_hds.type==PMSWITCH_OPCODE_ACK || pm_hds.type==PMSWITCH_OPCODE_REPONSE);
                    if(pm_hds.type==PMSWITCH_OPCODE_REPONSE){
                        last_server_seq = (pm_hds.seq_no>last_server_seq)?pm_hds.seq_no:last_server_seq;
                    }
                    responded++;

                    if(!enoughSpace(seq_no_global, last_server_seq)){
                        drain = 1;
                        continue;
                    }
                    drain = 0;
                }else if(pm_hds.type==PMSWITCH_OPCODE_CACHE_RESPONSE){
                    if(pm_hds.seq_no == hashInput){
                        //cerr << "recv switch cache" << endl;
                        responded++;
                    }else{
                        continue;
                    }
                    
                }else{
                    // do nothing, skip responded requests to prevent blocking.
                    // cerr << "Skipped" << endl;
                    if(pm_hds.type==PMSWITCH_OPCODE_REPONSE){
                        last_server_seq = (pm_hds.seq_no>last_server_seq)?pm_hds.seq_no:last_server_seq;
                    }
                    continue;
                }
                
            }
            seq_no_global++;


        thisReqEnd_time = chrono::high_resolution_clock::now();
        timeArray[ctr] = (uint32_t)chrono::duration_cast<chrono::microseconds>(thisReqEnd_time - lastReqEnd_time).count();
        lastReqEnd_time = thisReqEnd_time;
        ctr++;
        if(ctr>=numOps){
            break;
        }


        
    }
    ofstream statFile;
    
    
    auto end_time = chrono::high_resolution_clock::now();
    // timeArray
    std::vector<uint32_t>timeVectorMicroFull(timeArray, &timeArray[ctr]);
    std::vector<uint32_t>timeVectorMicro(&timeArray[ctr*10/100], &timeArray[ctr*95/100]);
    int64_t totalTimeMicro = 0;
    int dataPtsCount = (ctr*95/100) - (ctr*10/100);
    for(int k=0;k<timeVectorMicro.size();k++){
        totalTimeMicro += timeVectorMicro[k];
    }

    cout /*<< "AvgTime: "*/ << (double)totalTimeMicro/dataPtsCount << endl;

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
    char* useErrorMsg = "Use: ./client numOps writeRatio payloadSize usePMSwitch PMSwitch_port dumpTiming\n";
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
    //cerr << "Initialized" << endl;
    for(int i=0;i<NUM_CONNECTIONS;i++){
        downStreamUDPSock[i] = socketHandler_connect(REMOTE_ADDRESS, pmSwitch_port+i, DATAGRAM, BLOCKING);
    }
    
    runTest();

}
