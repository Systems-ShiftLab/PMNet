//////////////////////////////////////////////////
// Korakit Seemakhupt (korakit@virginia.edu)
// Client for Twitter-clone workload.
// Based on https://redis.io/topics/twitter-clone
//////////////////////////////////////////////////
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

#define PMSWITCH_REPLICATION 1
#define REMOTE_ADDRESS "192.168.1.41"
#define APPLICATION_PORT 50000
#define REDIS_PORT 6379
int downStreamUDPSock = 0;
// int downStreamTCPSock = 0;
int seq_no_global = 0;
int last_server_seq = 0;
double writeRatio = 0.5;
int numOps = 100000;
int payloadSize = 200;
char randomPayload[2000];
int usePMSwitch = 1;
int pmSwitch_port = PMSWITCH_PORT;
uint32_t* timeArray;
int dumpTiming = 0;
int clientID = 0;
unordered_set <string> addedUsers_set;
vector <string> addedUsers_vec;
int totalOps = 0;
int totalNon_PMSwitchOPS = 0;
int numPosts = 1;
int atomicCMD = 0;
int totalRead = 0;

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



int enoughSpace(int seq_no_global, int last_server_seq){
    return (seq_no_global-last_server_seq) >= 64 ? 0 : 1;
}

string genFLUSHALL(){
    return string("*1\r\n$8\r\nFLUSHALL\r\n");
}

uint64_t getTimeStamp(){
    uint64_t timeStamp = chrono::duration_cast<chrono::microseconds>(chrono::system_clock::now().time_since_epoch()).count();
    return timeStamp;
}
int sendPMSwitchPayloadCommon(int socket_to_target, char* payload, size_t payloadSize, uint8_t type){
    char pmSwitchBuff[2048];
    struct pmswitchHeader pmswitch_hds;
    size_t sendingBytes = pmSwitchEncapsulate(pmSwitchBuff, type, APPLICATION_PORT, seq_no_global, payload, payloadSize);
    size_t sentBytes = socketHandler_send_bytes(downStreamUDPSock, pmSwitchBuff, sendingBytes);
    if(sendingBytes != sentBytes){
        sendErr(0);
    }
    int responded = 0;
    int drain = 0;
	//cerr << "Sent" << endl;
    // Wait for the ACK from the switch/server
    while(drain||((type==PMSWITCH_OPCODE_PERSIST_NEED_ACK)?responded<PMSWITCH_REPLICATION:responded<1)){
        int responseSize = socketHandler_recv_bytes(downStreamUDPSock, pmSwitchBuff, sizeof(pmSwitchBuff)); 
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
		//cerr << "resp:" << responded << endl;
            //Continue draining ack until there is enough space.
            if(!enoughSpace(seq_no_global, last_server_seq)){
                drain = 1;
                continue;
            }
            drain = 0;
            // cerr << "Responded" << endl;
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

	//cerr << "Ret" << endl;
    totalOps++;
    return 0;
}

int sendPMSwitchPayload(int socket_to_target, char* payload, size_t payloadSize){
    int ret=-1;
    if(usePMSwitch){
        ret = sendPMSwitchPayloadCommon(socket_to_target, payload, payloadSize, PMSWITCH_OPCODE_PERSIST_NEED_ACK);
    }else{
        totalNon_PMSwitchOPS++;
        ret = sendPMSwitchPayloadCommon(socket_to_target, payload, payloadSize, PMSWITCH_OPCODE_BYPASS);
    }
    return ret;
}

int sendNonPMSwitchPayload(int socket_to_target, char* payload, size_t payloadSize){
    totalNon_PMSwitchOPS++;
    totalRead++;
    return sendPMSwitchPayloadCommon(socket_to_target, payload, payloadSize, PMSWITCH_OPCODE_BYPASS);
}

string sendWithResponse(int socket_to_target, char* payload, size_t payloadSize){
    string retString;
    char recfBuff[2048];
    char pmSwitchBuff[2048];
    struct pmswitchHeader pmswitch_hds;
    size_t sendingBytes = pmSwitchEncapsulate(pmSwitchBuff, PMSWITCH_OPCODE_BYPASS, APPLICATION_PORT, seq_no_global, payload, payloadSize);
    size_t sentBytes = socketHandler_send_bytes(downStreamUDPSock, pmSwitchBuff, sendingBytes);
    if(sendingBytes != sentBytes){
        sendErr(0);
    }
    int responded = 0;
    // Wait for the ACK from the switch/server
    while(responded<1){
        int responseSize = socketHandler_recv_bytes(downStreamUDPSock, pmSwitchBuff, sizeof(pmSwitchBuff)); 
        struct pmswitchHeader pm_hds;
        parseHeader(pmSwitchBuff, &pm_hds, responseSize);
        // Redis is strictly Request-Response. We only need to reject response/ACK of previous requests.
        if((pm_hds.seq_no==seq_no_global) && (APPLICATION_PORT==pm_hds.session_id)){
            assert(pm_hds.type==PMSWITCH_OPCODE_ACK || pm_hds.type==PMSWITCH_OPCODE_REPONSE);
            if(pm_hds.type==PMSWITCH_OPCODE_REPONSE){
                last_server_seq = (pm_hds.seq_no>last_server_seq)?pm_hds.seq_no:last_server_seq;
            }
            size_t payload_size = stripHeader(recfBuff, pmSwitchBuff, responseSize);
            retString = string(recfBuff, payload_size);
            responded++;
            seq_no_global++;
            //Continue draining ack until there is enough space.
            if(!enoughSpace(seq_no_global, last_server_seq)){
                continue;
            }
            // cerr << "Responded" << endl;
        }else{
            // do nothing, skip responded requests to prevent blocking.
            // cerr << "Skipped" << endl;
            if(pm_hds.type==PMSWITCH_OPCODE_REPONSE){
                last_server_seq = (pm_hds.seq_no>last_server_seq)?pm_hds.seq_no:last_server_seq;
            }
            continue;
        }
    }
    totalNon_PMSwitchOPS++;
    totalOps++;
    totalRead++;
    return retString;
}

void sendPMSwitchStringCMD(string cmd, int socket_to_target){
    const char* cmd_payload = cmd.c_str();
    size_t payloadSize = (size_t)cmd.size();
    assert(payloadSize<1900);
    size_t ret = sendPMSwitchPayload(socket_to_target, (char*) cmd_payload, payloadSize);
}

void sendNonPMSwitchStringCMD(string cmd, int socket_to_target){
    const char* cmd_payload = cmd.c_str();
    size_t payloadSize = (size_t)cmd.size();
    assert(payloadSize<1900);
    size_t ret = sendNonPMSwitchPayload(socket_to_target, (char*) cmd_payload, payloadSize);
}

string sendStringCMD_with_response(string cmd, int socket_to_target){
    const char* cmd_payload = cmd.c_str();
    size_t payloadSize = (size_t)cmd.size();
    assert(payloadSize<1900);
    return sendWithResponse(socket_to_target, (char*) cmd_payload, payloadSize);
}
string parseINCR_response(string ret){
    atomicCMD++;
    const char* p = ret.c_str();
    int val = 0;

    p++;
    while(*p != '\r') {
        val = (val*10)+(*p - '0');
        p++;
    }
    return to_string(val);
}
// User entry format: user:$uid username $username password $password
// Command format: HMSET user:$uid username $username password $password
int addUser(){
    uint64_t uid = getTimeStamp();
    uint64_t username = uid;
    uint64_t password = uid;
    // Generate Redis's command
    // HMSET's header
    string genUserHMSET = string("*6\r\n$5\r\nHMSET\r\n");
    //key
    // Korakit
    // Pipeline several requests: improve baseline (non-PMNet) performance.
    // Get a unique userid
    string fullUserString = parseINCR_response(sendStringCMD_with_response(string("*2\r\n$4\r\nINCR\r\n$6\r\nnewUID\r\n"), downStreamUDPSock));
    //cerr << "New UID: " << fullUserString << endl;
    addedUsers_set.insert(fullUserString);
    addedUsers_vec.push_back(fullUserString);
    genUserHMSET = (genUserHMSET + "$") + to_string(fullUserString.size()+5) + "\r\n"+ "user:" + fullUserString + "\r\n";
    // username field identifier
    genUserHMSET = genUserHMSET + string("$8\r\nusername\r\n");
    // The username itself
    string usernameString = to_string(username);
    genUserHMSET = genUserHMSET + "$" + to_string(usernameString.size()) + "\r\n" + usernameString + "\r\n";
    // password field identifier
    genUserHMSET = genUserHMSET + string("$8\r\npassword\r\n");
    string passwordString = to_string(password);
    genUserHMSET = genUserHMSET + "$" + to_string(passwordString.size()) + "\r\n" + passwordString + "\r\n";
    const char* hmset_payload = genUserHMSET.c_str();
    size_t payloadSize = (size_t)genUserHMSET.size();
    // Send HMSET to the server
    int ret = sendPMSwitchPayload(downStreamUDPSock, (char*) hmset_payload, payloadSize);
    if(ret!=0){
        return -1;
        cerr << "Send to server failed" << endl;
    }

    return 0;
}

// Each user has a list of follower and following users.

// FORMAT
// follower:$userid $timestamp $followerID
// following:$userid $timestamp $followingID
// COMMAND FORMAT
// ZADD follower:$followingID $timestamp $followerID
// ZADD following:$followerID $timestamp $followingID

void flushDB(){
    string flushCMD = genFLUSHALL();
    const char* flushCMD_payload = flushCMD.c_str();
    size_t payloadSize = (size_t)flushCMD.size();
    // Bypass the flush command
    int ret = sendNonPMSwitchPayload(downStreamUDPSock, (char*) flushCMD_payload, payloadSize);
    if(ret!=0){
        cerr << "Send to server failed" << endl;
        exit(-1);
    }
}

string genZADD(string key, string timestamp, string value){
    string zadd_command = string("*4\r\n$4\r\nZADD\r\n");
    // Key
    zadd_command = zadd_command + "$" + to_string(key.size()) + "\r\n"+ key + "\r\n";
    // Timestamp = 0
    string timeStampString = timestamp;
    zadd_command = zadd_command + "$" + to_string(timestamp.size()) + "\r\n" + timeStampString + "\r\n";
    // Value
    zadd_command = zadd_command + "$" + to_string(value.size())+ "\r\n" + value + "\r\n";
    return zadd_command;
}

string genLPUSH(string key, string value){
    string command = string("*3\r\n$5\r\nLPUSH\r\n");
    command = command + "$" + to_string(key.size()) + "\r\n"+ key + "\r\n";
    command = command + "$" + to_string(value.size())+ "\r\n" + value + "\r\n";
    return command;
}

string genLRANGE(string key, string max, string min){
    string command = string("*4\r\n$6\r\nLRANGE\r\n");
    command = command + "$" + to_string(key.size()) + "\r\n"+ key + "\r\n";
    command = command + "$" + to_string(max.size())+ "\r\n" + max + "\r\n";
    command = command + "$" + to_string(min.size())+ "\r\n" + min + "\r\n";
    return command;
}

string genSET(string key, string value){
    string set_command = string("*3\r\n$3\r\nSET\r\n");
    // Following ID as a key
    set_command = set_command + "$" + to_string(key.size()) + "\r\n"+ key + "\r\n";
    // FollowerID
    set_command = set_command + "$" + to_string(value.size())+ "\r\n" + value + "\r\n";
    return set_command;
}

string genZPOPMAX(string key){
    string command = string("*2\r\n$7\r\nZPOPMAX\r\n");
    // key
    command = command + "$" + to_string(key.size()) + "\r\n"+ key + "\r\n";
    return command;
}
int followUsers(){
    // Iterate each user
    for(int i=0; i<addedUsers_vec.size(); i++){
        
        // N -> N connection
        for(int j=0;j<addedUsers_vec.size();j++){
            if(i==j)continue;
            string followerID =  addedUsers_vec[i];
            string followingID = addedUsers_vec[j];
            // ADD follower to followingID
            // Generate Redis's command
            // ZADD's header
            // Following ID as a key
            string key = "follower:" + followingID;
            // Timestamp = 0
            string timeStampString = "0";
            string zadd_command = genZADD(key, timeStampString, followerID).c_str();
            const char* zadd_payload = zadd_command.c_str();
            size_t payloadSize = (size_t)zadd_command.size();
            // Send ZADD to the server
            int ret = sendPMSwitchPayload(downStreamUDPSock, (char*)zadd_payload, payloadSize);
            if(ret!=0){
                cerr << "ZADD Send to server failed" << endl;
                return -1;
            }

            // ADD following to followerID
            // Follower ID as a key
            string key2 = "following:" + followerID;
            // Timestamp = 0
            string timeStampString2 = "0";
            string zadd_command2 = genZADD(key2, timeStampString2, followingID);
            const char* zadd_payload2 = zadd_command2.c_str();
            size_t payloadSize2 = (size_t)zadd_command2.size();
            // Send ZADD to the server
            ret = sendPMSwitchPayload(downStreamUDPSock, (char*)zadd_payload2, payloadSize2);
            if(ret!=0){
                cerr << "ZADD Send to server failed" << endl;
                return -1;
            }
        }
    }
    return 0;
}
// Assume that same user will not multi-tweet at the same time.
// user -> post: ZADD post:$uid $timeStamp $postid
// post -> content: SET postContent:$uid:$postid $content

int newPost(){
    // 10 posts
    for(int i=0; i<addedUsers_vec.size(); i++){
        for(int j=0; j<numPosts;j++){
            string userString =  addedUsers_vec[i];
            // ADD follower to followingID
            // Generate Redis's command
            // ZADD's header
            // Following ID as a key
            string key = "post:" + userString;
            // Timestamp = 0
            // string postIDString = parseINCR_response(sendStringCMD_with_response(string("*2\r\n$4\r\nINCR\r\n$6\r\nnewPostID\r\n"), downStreamUDPSock));
            //string postIDString = userString + ":" + to_string(getTimeStamp());
            // Use this instead
            string postIDString = userString + ":" + to_string(j);
            //cerr << "newPostID: " << postIDString << endl;
            string command1 = genLPUSH(key, postIDString);
            const char* command1_payload = command1.c_str();
            size_t payloadSize = (size_t)command1.size();
            // Send ZADD to the server
            int ret = sendPMSwitchPayload(downStreamUDPSock, (char*)command1_payload, payloadSize);
            if(ret!=0){
                cerr << "ZADD Send to server failed" << endl;
                return -1;
            }
            string post_content_key = "postContent:" + postIDString;
            string content = "lalala";
            string set_command = genSET(post_content_key, content);
            const char* set_payload = set_command.c_str();
            size_t payloadSize2 = (size_t)set_command.size();
            // Send SET to the server
            int ret2 = sendPMSwitchPayload(downStreamUDPSock, (char*)set_payload, payloadSize2);
            if(ret2!=0){
                cerr << "SET Send to server failed" << endl;
                return -1;
            }
        }
    }
    return 0;
}

int viewPosts(){
    // ZPOPMAX for all users
    int stop = 0;
    while(!stop){
        for(int i=0; i<addedUsers_vec.size(); i++){
            string commandstring = genLRANGE("post:"+addedUsers_vec[i], to_string(0), to_string(4));
            const char* payload = commandstring.c_str();
            size_t payloadSize = (size_t)commandstring.size();
            // Send SET to the server
            int ret2 = sendNonPMSwitchPayload(downStreamUDPSock, (char*)payload, payloadSize);
            if(ret2!=0){
                cerr << "ZPOPMAX Send to server failed" << endl;
                return -1;
            }
            for(int j=0;j<5;j++){
            string contentIndex = string("postContent:")+addedUsers_vec[i]+":"+to_string(j);
            sendNonPMSwitchStringCMD(string("*2\r\n$3\r\nGET\r\n")+"$"+to_string(contentIndex.length())+"\r\n"+contentIndex+"\r\n", downStreamUDPSock);
            }
            if(((double)totalRead)/totalOps > 1.0-writeRatio){
                stop=1;
            }
        }
    }
    return 0;
}

void initDB(){
    // Flush DB
    flushDB();
}

int runTest(){
    int isSupported_command = 0;
    int isWriteRequest = 0;
    int requestSize = 0;
    int ctr = 0;
    int numWrite = 0;
    auto start_time = chrono::high_resolution_clock::now();
    auto lastReqEnd_time = start_time;
    auto thisReqEnd_time = start_time;
    initDB();

    for(int k=0; k< numOps ; k++){
        addUser();
    }
    followUsers();
    for(int k=0; k<numPosts; k++){
        newPost();
    }
    viewPosts();

    
    auto end_time = chrono::high_resolution_clock::now();
    uint32_t totalTime = (uint32_t)chrono::duration_cast<chrono::microseconds>(end_time - start_time).count();
    cout << "TotalTime: " << totalTime << endl;
    cout << "Total non-PMSwitch ops: " << totalNon_PMSwitchOPS << endl;
    cout << "Total Ops: " << totalOps << endl;
    cout << "Avg latency: " << (double)totalTime/totalOps << endl;
    cout << "Total Atomic command: " << atomicCMD << endl;
    return 0;
}




int main(int argc, char* argv[]){
    // Parse parameters
    const char* useErrorMsg = "Use: ./redisClient numUsers numPosts usePMSwitch PMSwitch_port dumpTiming update-ratio\n";
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
        numPosts = atoi(argv[2]);
    }
    if(argc>3){
        if(argv[3][0]>'9'||argv[3][0]<'0'){
            cerr << useErrorMsg << endl;
            exit(1);
        }else{
            usePMSwitch = atoi(argv[3]);
        }
    }
    if(argc>4){
        if(argv[4][0]>'9'||argv[4][0]<'0'){
            cerr << useErrorMsg << endl;
            exit(1);
        }else{
            pmSwitch_port = atoi(argv[4]);
        }
    }
    if(argc>5){
        if(argv[5][0]>'9'||argv[5][0]<'0'){
            cerr << useErrorMsg << endl;
            exit(1);
        }
        dumpTiming = atoi(argv[5]);
    }
    if(argc>6){
        writeRatio = atof(argv[6]);
    }
    // Prepare space for timing.
    timeArray = (uint32_t*)malloc(numOps*sizeof(uint32_t));


    // Initialize payload.
    initializePayload();
    //cerr << "Initialized" << endl;
    downStreamUDPSock = socketHandler_connect(REMOTE_ADDRESS, pmSwitch_port, DATAGRAM, BLOCKING);

    runTest();
    return 0;
}
