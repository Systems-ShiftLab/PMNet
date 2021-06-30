#include <string.h>
#include <assert.h>
#include <string>
#include <unordered_map>
#include <arpa/inet.h>
#define ERROR -1;
// Constants
#define PMSWITCH_OPCODE_INVALID             0x00    // Not used
#define PMSWITCH_OPCODE_PERSIST_NEED_ACK    0x01    // Persist using PMSwitch Protocol
#define PMSWITCH_OPCODE_ACK                 0x02    // Ack from other switch
#define PMSWITCH_OPCODE_REPONSE             0x03    // Response from the server
#define PMSWITCH_OPCODE_BYPASS              0x04    // Do not persist, let end host handle reTx.
#define PMSWITCH_OPCODE_RECOVER             0x05    // Response from the server
#define PMSWITCH_OPCODE_NOOP                0xFF    // NO-OP, just forward whatever in the pipeline
#define PMSWITCH_PORT 51000

enum RESPONSETYPE {
    SIMPLESTRING_RESPONSE = 0,
    INTEGER_RESPONSE = 1,
};

// List of command supported by the PMSwitch.
std::unordered_map<std::string, int> supportedCommand = {
    {"HMSET", SIMPLESTRING_RESPONSE},
    {"SET", SIMPLESTRING_RESPONSE},
    // "XADD"
};

struct pmswitchHeader{
    // We need to keep this exact order to avoid alignment issue.
    uint8_t type;
    uint8_t ackCount;
    uint16_t session_id;
    uint32_t seq_no;
    uint32_t PMAddress;
    uint16_t padding;   // Padding to make the payload 8-byte aligned.
};



void parsErr(int i){
    cerr << "Parsing error ("<< i << "), exiting" << endl;
    exit(-1);
}
void sendErr(int i){
    cerr << "Sending error ("<< i << "), exiting" << endl;
    exit(-1);
}

// Copy PMSwitch payload to outBuff
size_t stripHeader(char* outBuff, char* inBuff, size_t responseSize){
    size_t headerSize = sizeof(struct pmswitchHeader);
    if(responseSize < headerSize){
        return 0;
    }
    memcpy(outBuff, (void*)((uint64_t)inBuff+(uint64_t)headerSize), responseSize-headerSize);
    return responseSize-headerSize;
}


// Create PMSwitch header and append payload in output buffer.
// Returns total size of PMSwitch packet including the header.
size_t pmSwitchEncapsulate(char* output, uint8_t type, uint16_t session_id, uint32_t seq_no, char* payload, size_t payload_length){
    struct pmswitchHeader pmswitch_hds;
    pmswitch_hds.seq_no = seq_no;
    pmswitch_hds.session_id = session_id;
    pmswitch_hds.type = type;
    // Need to change here!
    uint32_t hashedAddr = seq_no;
    uint32_t effectivePMAddr = ((hashedAddr*2048)%0x80000000)+0x80000000;
    pmswitch_hds.PMAddress = ntohl(effectivePMAddr);
    pmswitch_hds.ackCount = 0xFB;
    // --------------------
    int sendSize=0;
    memcpy((void*)output, (void*)&pmswitch_hds, sizeof(pmswitch_hds));
    sendSize += (int)sizeof(pmswitch_hds);
    // Copy original request to output buffer after the PMSwitch header
    memcpy((void*)(((uint64_t)output)+((uint64_t)sendSize)), payload, (size_t)payload_length);
    sendSize += payload_length;
    return sendSize;
}

// Parse PMSwitch Header and put it into the struct
int parseHeader(char* input, struct pmswitchHeader* hds, size_t input_size){
    size_t headerSize = sizeof(struct pmswitchHeader);
    if(input_size < headerSize){
        return ERROR;
    }
    memcpy((void*)hds, (void*)input, headerSize);
    hds->PMAddress = htonl(hds->PMAddress); // DEBUG: Convert endianess back to native.
    return (int)headerSize;
}