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

/*
 Based on 
         Vaibhav Gogte <vgogte@umich.edu>
         Aasheesh Kolli <akolli@umich.edu>
 TPCC implementation.
*/

// Constants




// For TPCC
int num_warehouses = 1;
int num_items = 1;

#define NUM_ORDERS 10000
#define NUM_THREADS 1 

#define NUM_WAREHOUSES 1
#define NUM_ITEMS 10000
#define NUM_LOCKS NUM_WAREHOUSES*10 + NUM_WAREHOUSES*NUM_ITEMS



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
unsigned g_seed = 1312515;
unsigned long rndm_seeds;
int numLocks = 0;
unsigned long get_random(int min, int max) {
  return min+(rand()%(max-min+1));
}
int rand_local(int min, int max) {
  return (min + (rand()%(max-min+1)));
}

int seqnoToKey(int seqno){
    return seqno;
}



int enoughSpace(int seq_no_global, int last_server_seq){
    return (seq_no_global-last_server_seq) >= 64 ? 0 : 1;
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
            //Continue draining ack until there is enough space.
            if(!enoughSpace(seq_no_global, last_server_seq)){
                drain=1;
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
    int drain = 0 ;
    // Wait for the ACK from the switch/server
    while(responded<1||drain){
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
            size_t payload_size = stripHeader(recfBuff, pmSwitchBuff, responseSize);
            retString = string(recfBuff, payload_size);
            responded++;
            seq_no_global++;
            //Continue draining ack until there is enough space.
            if(!enoughSpace(seq_no_global, last_server_seq)){
                drain=1;
                continue;
            }
            drain=0;
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
    return retString;
}

void sendStringCMD(string cmd, int socket_to_target){
    const char* cmd_payload = cmd.c_str();
    size_t payloadSize = (size_t)cmd.size();
    assert(payloadSize<1900);
    size_t ret = sendPMSwitchPayload(socket_to_target, (char*) cmd_payload, payloadSize);
}

string genHMGET(string key, string field){
    string command = string("*3\r\n$5\r\nHMGET\r\n");
    // Key
    command = command + "$" + to_string(key.size()) + "\r\n"+ key + "\r\n" + "$" + to_string(field.size()) + "\r\n"+ field + "\r\n";
    // Timestamp = 0
    return command;
}

string genHMSET(string key, string field, string value){
    string command = string("*4\r\n$5\r\nHMSET\r\n");
    // Key
    command = command + "$" + to_string(key.size()) + "\r\n"+ key + "\r\n" + "$" + to_string(field.size()) + "\r\n"+ field + "\r\n" + "$" + to_string(value.size()) + "\r\n"+ value + "\r\n";
    // Timestamp = 0
    return command;
}

void sendHMSET(string key, string field, string value){
    string cmd = genHMSET(key, field, value);
    const char* cmd_payload = cmd.c_str();
    size_t payloadSize = (size_t)cmd.size();
    size_t ret = sendPMSwitchPayload(downStreamUDPSock, (char*) cmd_payload, payloadSize);
    if(ret==-1){
        cerr << "Send failed 4" << endl;
    }
}

string get_field(string key, string field){
    string cmd = genHMGET(key, field);
    const char* cmd_payload = cmd.c_str();
    size_t payloadSize = (size_t)cmd.size();
    //cerr << "size:" << cmd.size() << endl;
    string ret = sendWithResponse(downStreamUDPSock, (char*) cmd_payload, payloadSize);
    // Parse the return value
    // Expect *1\r\n $len \r\n $value
    const char* ret_c_str = ret.c_str();
    char* p = (char*)&ret_c_str[5];
    int ret_length = 0;
    while(*p != '\r'){
        ret_length = (ret_length*10)+(*p - '0');
        p++;
    }
    //cerr << "retlength:" << ret_length << endl;
    // Skip \r\n
    p+=2;
    string retstring = string(p, ret_length);
    return retstring;
}

struct warehouse_entry {
  //int w_id;
  //char w_name[10];
  //char w_street_1[20];
  //char w_street_2[20];
  //char w_city[20];
  //char w_state[2];
  //char w_zip[9];
  float w_tax;
  //float w_ytd;
};

// warehouse:$widx $w_tax
void get_warehouse_entries(struct warehouse_entry* warehouse, int w_indx){
    string w_tax_str = get_field("warehouse:"+to_string(w_indx), string("w_tax"));
    float w_tax;
    sscanf(w_tax_str.c_str(), "%f", &w_tax);
    warehouse->w_tax = w_tax;
}

void set_warehouse_entries(struct warehouse_entry* warehouse, int w_indx){
    string w_tax_str = to_string(warehouse->w_tax);
    string cmd = genHMSET("warehouse:"+to_string(w_indx), string("w_tax"), w_tax_str);
    sendStringCMD(cmd, downStreamUDPSock);
}

struct district_entry {
  //short d_id;
  //int d_w_id;
  //char d_name[10];
  //char d_street_1[20];
  //char d_street_2[20];
  //char d_city[20];
  //char d_state[2];
  //char d_zip[9];
  float d_tax;
  //float d_ytd;
  int d_next_o_id;
};

// district:$d_idx d_tax $d_tax d_next_o_id $d_next_o_id
void get_district_entries(struct district_entry* district, int d_indx){
    string d_tax_str = get_field("district:"+to_string(d_indx), string("d_tax"));
    float d_tax;
    sscanf(d_tax_str.c_str(), "%f", &d_tax);
    district->d_tax = d_tax;
    string d_next_o_id_str = get_field("district:"+to_string(d_indx), string("d_next_o_id"));
    int d_next_o_id;
    sscanf(d_next_o_id_str.c_str(), "%d", &d_next_o_id);
    district->d_next_o_id = d_next_o_id;
}

void set_district_entries(struct district_entry* district, int d_indx){
    string d_tax_str = to_string(district->d_tax);
    string cmd = genHMSET("district:"+to_string(d_indx), string("d_tax"), d_tax_str);
    sendStringCMD(cmd, downStreamUDPSock);
    string d_next_o_id_str = to_string(district->d_next_o_id);
    cmd = genHMSET("district:"+to_string(d_indx), string("d_next_o_id"), d_next_o_id_str);
    sendStringCMD(cmd, downStreamUDPSock);
}


struct new_order_entry {
  int no_o_id;
  short no_d_id;
  int no_w_id;
};

// new_order_entry:$no_idx no_o_id $no_o_id no_d_id $no_d_id no_w_id $no_w_id
void get_new_order_entry(struct new_order_entry* new_order, int no_indx){
    //string wname = get_field("warehouse:"+to_string(w_indx), to_string("w_id"));
    //strncpy(warehouse->w_name, wname.c_str(), 10);
    string no_o_id_str = get_field("new_order_entry:"+to_string(no_indx), string("no_o_id"));
    int no_o_id;
    sscanf(no_o_id_str.c_str(), "%d", &no_o_id);
    new_order->no_o_id = no_o_id;

    string no_d_id_str = get_field("new_order_entry:"+to_string(no_indx), string("no_d_id"));
    int no_d_id;
    sscanf(no_d_id_str.c_str(), "%d", &no_d_id);
    new_order->no_d_id = (short)no_d_id;

    string no_w_id_str = get_field("new_order_entry:"+to_string(no_indx), string("no_w_id"));
    int no_w_id;
    sscanf(no_w_id_str.c_str(), "%d", &no_w_id);
    new_order->no_w_id = no_w_id;
}

void set_new_order_entry(struct new_order_entry* new_order, int no_indx){
    string no_o_id_str = to_string(new_order->no_o_id);
    string cmd = genHMSET("new_order_entry:"+to_string(no_indx), string("no_o_id"), no_o_id_str);
    sendStringCMD(cmd, downStreamUDPSock);
    string no_d_id_str = to_string(new_order->no_d_id);
    cmd = genHMSET("new_order_entry:"+to_string(no_indx), string("no_d_id"), no_d_id_str);
    sendStringCMD(cmd, downStreamUDPSock);
    string no_w_id_str = to_string(new_order->no_w_id);
    cmd = genHMSET("new_order_entry:"+to_string(no_indx), string("no_w_id"), no_w_id_str);
    sendStringCMD(cmd, downStreamUDPSock);
}



struct order_entry {
  int o_id;
  //short o_d_id;
  //int o_w_id;
  int o_c_id;
  long long o_entry_d;
  short o_carrier_id;
  float o_ol_cnt;
  float o_all_local;
};

// new_order_entry:$o_idx o_id $o_id o_c_id $o_c_id o_entry_d $o_entry_d o_carrier_id $o_carrier_id o_ol_cnt $o_ol_cnt o_all_local $o_all_local
void get_order_entry(struct order_entry* order, int o_indx){

    string o_id_str = get_field("order_entry:"+to_string(o_indx), string("o_id"));
    int o_id;
    sscanf(o_id_str.c_str(), "%d", &o_id);
    order->o_id = o_id;

    string o_c_id_str = get_field("order_entry:"+to_string(o_indx), string("o_c_id"));
    int o_c_id;
    sscanf(o_c_id_str.c_str(), "%d", &o_c_id);
    order->o_c_id = o_c_id;

    string o_entry_d_str = get_field("order_entry:"+to_string(o_indx), string("o_entry_d"));
    long long o_entry_d;
    sscanf(o_entry_d_str.c_str(), "%lld", &o_entry_d);
    order->o_entry_d = o_entry_d;

    string o_carrier_id_str = get_field("order_entry:"+to_string(o_indx), string("o_carrier_id"));
    int o_carrier_id;
    sscanf(o_carrier_id_str.c_str(), "%d", &o_carrier_id);
    order->o_carrier_id = o_carrier_id;

    string o_ol_cnt_str = get_field("order_entry:"+to_string(o_indx), string("o_ol_cnt"));
    float o_ol_cnt;
    sscanf(o_ol_cnt_str.c_str(), "%f", &o_ol_cnt);
    order->o_ol_cnt = o_ol_cnt;

    string o_all_local_str = get_field("order_entry:"+to_string(o_indx), string("o_all_local"));
    float o_all_local;
    sscanf(o_all_local_str.c_str(), "%f", &o_all_local);
    order->o_all_local = o_all_local;
}

void set_order_entry(struct order_entry* order, int o_indx){
    string o_id_str = to_string(order->o_id);
    string cmd = genHMSET("order_entry:"+to_string(o_indx), string("o_id"), o_id_str);
    sendStringCMD(cmd, downStreamUDPSock);
    string o_c_id_str = to_string(order->o_c_id);
    cmd = genHMSET("order_entry:"+to_string(o_indx), string("o_c_id"), o_c_id_str);
    sendStringCMD(cmd, downStreamUDPSock);
    string o_entry_d_str = to_string(order->o_entry_d);
    cmd = genHMSET("order_entry:"+to_string(o_indx), string("o_entry_d"), o_entry_d_str);
    sendStringCMD(cmd, downStreamUDPSock);

    string o_carrier_id_str = to_string(order->o_carrier_id);
    cmd = genHMSET("order_entry:"+to_string(o_indx), string("o_carrier_id"), o_carrier_id_str);
    sendStringCMD(cmd, downStreamUDPSock);
    string o_ol_cnt_str = to_string(order->o_ol_cnt);
    cmd = genHMSET("order_entry:"+to_string(o_indx), string("o_ol_cnt"), o_ol_cnt_str);
    sendStringCMD(cmd, downStreamUDPSock);
    string o_all_local_str = to_string(order->o_all_local);
    cmd = genHMSET("order_entry:"+to_string(o_indx), string("o_all_local"), o_all_local_str);
    sendStringCMD(cmd, downStreamUDPSock);
}

/*
struct order_line_entry {
  int ol_o_id;
  short ol_d_id;
  int ol_w_id;
  short ol_number;
  int ol_i_id;
  int ol_supply_w_id;
  long long ol_delivery_d;
  float ol_quantity;
  float ol_amount;
  char ol_dist_info[24];
};
*/
struct item_entry {
  //int i_id;
  //int i_im_id;
  //char i_name[24];
  float i_price;
  //char i_data[50];
};

void get_item_entry(struct item_entry* item, int i_indx){
    string i_price_str = get_field("item:"+to_string(i_indx), string("i_price"));
    float i_price;
    sscanf(i_price_str.c_str(), "%f", &i_price);
    item->i_price = i_price;
}

void set_item_entry(struct item_entry* item, int i_indx){
    string i_price_str = to_string(item->i_price);
    string cmd = genHMSET("item:"+to_string(i_indx), string("i_price"), i_price_str);
    sendStringCMD(cmd, downStreamUDPSock);
}

void set_price(int i_indx, float price){
    string i_price_str = to_string(price);
    string cmd = genHMSET("item:"+to_string(i_indx), string("i_price"), i_price_str);
    sendStringCMD(cmd, downStreamUDPSock);
}

struct stock_entry {
  //int s_i_id;
  //int s_w_id;
  float s_quantity;
  //char s_dist_01[24];
  //char s_dist_02[24];
  //char s_dist_03[24];
  //char s_dist_04[24];
  //char s_dist_05[24];
  //char s_dist_06[24];
  //char s_dist_07[24];
  //char s_dist_08[24];
  //char s_dist_09[24];
  //char s_dist_10[24];
  float s_ytd;
  float s_order_cnt;
  //float s_remote_cnt;
  //char s_data[50];
};

void get_stock_entry(struct stock_entry* stock, int s_indx){
    //string wname = get_field("warehouse:"+to_string(w_indx), to_string("w_id"));
    //strncpy(warehouse->w_name, wname.c_str(), 10);
    string s_quantity_str = get_field("stock_entry:"+to_string(s_indx), string("s_quantity"));
    float s_quantity;
    sscanf(s_quantity_str.c_str(), "%f", &s_quantity);
    stock->s_quantity = s_quantity;

    string s_ytd_str = get_field("stock_entry:"+to_string(s_indx), string("s_ytd"));
    float s_ytd;
    sscanf(s_ytd_str.c_str(), "%f", &s_ytd);
    stock->s_ytd = s_ytd;

    string s_order_cnt_str = get_field("stock_entry:"+to_string(s_indx), string("s_order_cnt"));
    float s_order_cnt;
    sscanf(s_order_cnt_str.c_str(), "%f", &s_order_cnt);
    stock->s_order_cnt = s_order_cnt;
}

void set_stock_entry(struct stock_entry* stock, int s_indx){
    string s_quantity_str = to_string(stock->s_quantity);
    string cmd = genHMSET("stock_entry:"+to_string(s_indx), string("s_quantity"), s_quantity_str);
    sendStringCMD(cmd, downStreamUDPSock);
    string s_ytd_str = to_string(stock->s_ytd);
    cmd = genHMSET("stock_entry:"+to_string(s_indx), string("s_ytd"), s_ytd_str);
    sendStringCMD(cmd, downStreamUDPSock);
    string s_order_cnt_str = to_string(stock->s_order_cnt);
    cmd = genHMSET("stock_entry:"+to_string(s_indx), string("s_order_cnt"), s_order_cnt_str);
    sendStringCMD(cmd, downStreamUDPSock);
}







void initializePayload(){
    int i;
    int* randomPayload_intPtr = (int*)randomPayload;
    for(i=0;i<sizeof(randomPayload)/sizeof(int);i++){
        randomPayload_intPtr[i] = rand();
    }
}


// Locks
void tryLock(string lockString){
    // SET lock:$lockString 1 nx
    while(1){
        string cmd = "*4\r\n$3\r\nSET\r\n";
        string lockKey = "lock:"+lockString;
        cmd = cmd + "$" + to_string(lockKey.size()) + "\r\n" + lockKey + "\r\n" + "$1\r\n1\r\n$2\r\nnx\r\n";
        const char* cmd_payload = cmd.c_str();
        size_t payloadSize = (size_t)cmd.size();
        assert(payloadSize<1900);
        string ret = sendWithResponse(downStreamUDPSock, (char*) cmd_payload, payloadSize);
        const char* ret_payload = ret.c_str();
        if(ret_payload[1]=='O' && ret_payload[2]=='K'){
            break;
        }
        // If cannot acquire lock, retry.
    }
}

void releaseLock(string lockString){
    string cmd = "*2\r\n$3\r\nDEL\r\n";
    string lockKey = "lock:"+lockString;
    cmd = cmd + "$" + to_string(lockKey.size()) + "\r\n" + lockKey + "\r\n";
    const char* cmd_payload = cmd.c_str();
    size_t payloadSize = (size_t)cmd.size();
    assert(payloadSize<1900);
    string ret = sendWithResponse(downStreamUDPSock, (char*) cmd_payload, payloadSize);
    numLocks++;
}


// Each user has a list of follower and following users.

// FORMAT
// follower:$userid $timestamp $followerID
// following:$userid $timestamp $followingID
// COMMAND FORMAT
// ZADD follower:$followingID $timestamp $followerID
// ZADD following:$followerID $timestamp $followingID

string genFLUSHALL(){
    return string("*1\r\n$8\r\nFLUSHALL\r\n");
}


float update_stock_entry(int _w_id, int _i_id, int _d_id, float amount, int itr) {
  int indx = (_w_id-1)*NUM_ITEMS + _i_id-1;
  tryLock("stock_entry:"+to_string(indx));
  // Load struct stock
  struct stock_entry stock;
  get_stock_entry(&stock, indx);
  int ol_quantity = 7;
  if(stock.s_quantity - ol_quantity > 10) {
    stock.s_quantity -= ol_quantity;
  }
  else {
    stock.s_quantity -= ol_quantity;
    stock.s_quantity += 91;
  }
  
  stock.s_ytd += ol_quantity;
  stock.s_order_cnt += 1;
  //store back
  set_stock_entry(&stock, indx);
  struct item_entry item;
  get_item_entry(&item, _i_id-1);
  releaseLock("stock_entry:"+to_string(indx));
  return amount + ol_quantity * item.i_price;
}

void fill_new_order_entry(int _no_w_id, int _no_d_id, int _no_o_id) {
  int indx = (_no_w_id-1)*10*900 + (_no_d_id-1)*900 + (_no_o_id-2101) % 900;
  // Load new_order_entry
  struct new_order_entry new_order;
  // Replace this.
  // new_order_entry type
  new_order.no_o_id = _no_o_id;
  new_order.no_d_id = _no_d_id;
  new_order.no_w_id = _no_w_id;
  // Store new_order_entry
  set_new_order_entry(&new_order, indx);
}

void update_order_entry(int _w_id, short _d_id, int _o_id, int _c_id, int _ol_cnt) {
  int indx = (_w_id-1)*10*3000 + (_d_id-1)*3000 + (_o_id-1)%3000;
  // Convert to write;
  struct order_entry order;
  // order_entry type
  order.o_id = _o_id;
  order.o_carrier_id = 0;
  order.o_all_local = 1;
  order.o_ol_cnt = _ol_cnt;
  order.o_c_id = _c_id;
  order.o_entry_d = 12112342433241;
  // Store order_entry
  set_order_entry(&order, indx);
}


void new_order_tx(int w_id, int d_id, int c_id) {


    int w_indx = (w_id-1);
    int d_indx = (w_id-1)*10 + (d_id-1);
    int c_indx = (w_id-1)*10*3000 + (d_id-1)*3000 + (c_id-1);
    int ol_cnt = get_random(5, 15);
    int item_ids[ol_cnt];
    for(int i=0; i<ol_cnt; i++) {
    int new_item_id;
    bool match;
    do {
        match = false;
        new_item_id = get_random(1, NUM_ITEMS);
        for(int j=0; j<i; j++) {
        if(new_item_id == item_ids[j]) {
            match = true;
            break;
        }
        }
    } while (match);
    item_ids[i] = new_item_id;
    }

    std::sort(item_ids, item_ids+ol_cnt);

    tryLock("district:"+to_string(d_indx));
    struct district_entry district;
    get_district_entries(&district, d_indx);
    struct warehouse_entry warehouse;
    get_warehouse_entries(&warehouse, w_indx);
    // Read
    float w_tax = warehouse.w_tax;
    float d_tax = district.d_tax;
    int d_o_id = district.d_next_o_id;
    ///////
    int no_indx = (w_id-1)*10*900 + (d_id-1)*900 + (d_o_id-2101) % 900;
    int o_indx = (w_id-1)*10*3000 + (d_id-1)*3000 + (d_o_id-1)%3000;
    // Update
    district.d_next_o_id++;
    set_district_entries(&district, d_indx);
    // Function call
    fill_new_order_entry(w_id, d_id, d_o_id);
    update_order_entry(w_id, d_id, d_o_id, c_id, ol_cnt);
    float total_amount = 0.0;
    for(int i=0; i<ol_cnt - 1; i++) {
    total_amount = update_stock_entry(w_id, item_ids[i], d_id, total_amount, i);
    }
    // The last update_stock_entry
    total_amount = update_stock_entry(w_id, item_ids[ol_cnt - 1], d_id, total_amount, ol_cnt - 1);

    // Release Locks
    releaseLock("district:"+to_string(d_indx));
    return;
}

void new_orders(){
      for(int i=0; i<NUM_ORDERS; i++) {
    int w_id = 1;
    //There can only be 10 districts, this controls the number of locks in tpcc_db, which is why NUM_LOCKS = warehouse*10
    int d_id = get_random(1, 10);
    int c_id = get_random(1, 3000);
    new_order_tx(w_id, d_id, c_id);

  }
}

void initialize(){
    // Initialize intems
    for(int i=0; i<NUM_ITEMS; i++) {
        set_price(i, rand_local(1,100)*(1.0));
    }
    for(int i=0; i<num_warehouses; i++) {
        struct warehouse_entry warehouse;
        warehouse.w_tax = (rand_local(0,20))/100.0;
        set_warehouse_entries(&warehouse, i);
        for(int j=0; j<NUM_ITEMS; j++) {
            int stock_indx = (i)*NUM_ITEMS + (j);
            struct stock_entry stock;
            stock.s_order_cnt = 0;
            stock.s_quantity = 0;
            stock.s_ytd = 0;
            set_stock_entry(&stock, stock_indx);
        }
        //std::cout<<"finished populating stock table"<<std::endl;
        for(int j=0; j<10; j++) {
            struct district_entry district;
            district.d_next_o_id = 3001;
            district.d_tax = (rand_local(0,20))/100.0;
            set_district_entries(&district, i*10+j);
        }
    }
}

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

int runTest(){
    flushDB();
    int isSupported_command = 0;
    int isWriteRequest = 0;
    int requestSize = 0;
    int ctr = 0;
    int numWrite = 0;
    auto start_time = chrono::high_resolution_clock::now();
    auto lastReqEnd_time = start_time;
    auto thisReqEnd_time = start_time;
    // Run the benchmark
    initialize();
    cerr << "Init done" << endl;
    new_orders();
    // Timing
    auto end_time = chrono::high_resolution_clock::now();
    uint32_t totalTime = (uint32_t)chrono::duration_cast<chrono::microseconds>(end_time - start_time).count();
    cout << "TotalTime: " << totalTime << endl;
    cout << "Total non-PMSwitch ops: " << totalNon_PMSwitchOPS << endl;
    cout << "Total Ops: " << totalOps << endl;
    cout << "Avg latency: " << (double)totalTime/totalOps << endl;
    cout << "Num Locks: " << numLocks << endl;

    return 0;
}




int main(int argc, char* argv[]){
    // Parse parameters
    const char* useErrorMsg = "Use: ./redisClient usePMSwitch PMSwitch_port dumpTiming\n";

    if(argc>1){
        if(argv[1][0]=='-'){
            ;
        }else{
            if(argv[1][0]>'9'||argv[1][0]<'0'){
                cerr << useErrorMsg << endl;
                exit(1);
            }else{
                usePMSwitch = atoi(argv[1]);
            }
        }
    }
    if(argc>2){
        if(argv[2][0]=='-'){
            ;
        }else{
            if(argv[2][0]>'9'||argv[2][0]<'0'){
                cerr << useErrorMsg << endl;
                exit(1);
            }else{
                pmSwitch_port = atoi(argv[2]);
            }
        }
    }
    if(argc>3){
        if(argv[3][0]>'9'||argv[3][0]<'0'){
            cerr << useErrorMsg << endl;
            exit(1);
        }
        dumpTiming = atoi(argv[3]);
    }
    // Initialize rand
    rndm_seeds = rand();
    // Prepare space for timing.
    timeArray = (uint32_t*)malloc(numOps*sizeof(uint32_t));


    // Initialize payload.
    initializePayload();
    //cerr << "Initialized" << endl;
    downStreamUDPSock = socketHandler_connect(REMOTE_ADDRESS, pmSwitch_port, DATAGRAM, BLOCKING);

    runTest();
    return 0;
}
