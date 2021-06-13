// Const defs
const bit<16>   TYPE_IPV4 = 0x0800;
const bit<8>    IPV4_PROTOCOL_UDP = 0x11;
const bit<8>    PMSWITCH_OPCODE_INVALID = 0x00;            // Not used
const bit<8>    PMSWITCH_OPCODE_PERSIST_NEED_ACK = 0x01;   // Persist using PMSwitch Protocol
const bit<8>    PMSWITCH_OPCODE_ACK = 0x02;                // Ack from other switch
const bit<8>    PMSWITCH_OPCODE_REPONSE = 0x03;            // Response from the server
const bit<8>    PMSWITCH_OPCODE_BYPASS = 0x04;
const bit<8>    PMSWITCH_OPCODE_RECOVER = 0x05;            // Recovery request
const bit<8>    PMSWITCH_OPCODE_CACHE_RESPONSE = 0x06;     // Response from the cache
const bit<8>    PMSWITCH_OPCODE_NOOP = 0xFF;               // NO-OP, just forward whatever in the pipeline
const bit<16>   PMSWITCH_PORT = 51000;                     // Reserved port number
const bit<16>   PMSWITCH_PORT_2 = 51001;                     // Reserved port number
const bit<16>   PMSWITCH_PORT_3 = 51002;                     // Reserved port number
const bit<16>   PMSWITCH_PORT_4 = 51003;                     // Reserved port number
// Valid address starts from 0x80000000 to 0xFFFFFFFF
const bit<32>   INVALID_ADDR = 0x7FFFFFFF;                 // For debug purpose.

typedef bit<48> MacAddress;
typedef bit<32> IPv4Address;
// Headers-------------------
// SOF
// Ethernet header 14 Bytes
header ethernet_h {
    MacAddress          dst;
    MacAddress          src;
    bit<16>             type;
}
// IPv4 header 20 Bytes
header ipv4_h {
    bit<4>              version;
    bit<4>              ihl;
    bit<8>              tos;
    bit<16>             len;
    bit<16>             id;
    bit<3>              flags;
    bit<13>             frag;
    bit<8>              ttl;
    bit<8>              proto;
    bit<16>             chksum;
    IPv4Address         src;
    IPv4Address         dst;
}
// 34 Bytes
// UDP header 8 Bytes
header udp_h {
    bit<16>             sport;
    bit<16>             dport;
    bit<16>             len;
    bit<16>             chksum;
}
// 42 Bytes
// PMSwitch header 12+2 Bytes
header pmswitchhds_h {
    bit<8>  type;           // Type of PMSwitch package: PERSIST_NEED_ACK, BYPASS or ACK_PERSIST
    bit<8>  ackCount;       // Ack count, used to track number of Ack required to remove the packet from the memory.
    bit<16> session_id;     // Session ID for each client           ---| Used as request identifier
    bit<32> seq_no;         // Sequence Number for each request     ---|
    // 50                   // The offset of PMAddress.
    bit<32> PMAddress;      // Hashed identifier, used as address
    // 54
    bit<16>  padding;       // Padding to make the payload 8-byte aligned
}
// 56 Bytes
// Payload
// Total size must not exceed 1024 bytes.
// EOF
// --------------------------

// Struct of all headers
struct headers {
    ethernet_h      ethernet;
    ipv4_h          ipv4;
    udp_h           udp;
    pmswitchhds_h   pmswitchhds;
}

// Common parser shared by both Request and Ack paths
// Xilinx-specific max packet size primitive
@Xilinx_MaxPacketRegion(1500*8)  // in bits
parser PMSwitch_ack_Parser(packet_in pkt,
                      out headers hdr){
    state start {
        pkt.extract(hdr.ethernet);
        transition select(hdr.ethernet.type) {
            TYPE_IPV4   :   parse_ipv4;
            default     :   accept;
        }
    }

    state parse_ipv4 {
        pkt.extract(hdr.ipv4);
        transition select(hdr.ipv4.proto) {
            IPV4_PROTOCOL_UDP   :   parse_udp_sport;
            default             :   accept;
        }
    }
    state parse_udp_sport {
        pkt.extract(hdr.udp);
        transition select(hdr.udp.sport){
            51000 : parse_PMSwitch;
            51001 : parse_PMSwitch;
            51002 : parse_PMSwitch;
            51003 : parse_PMSwitch;
            51004 : parse_PMSwitch;
            51005 : parse_PMSwitch;
            51006 : parse_PMSwitch;
            51007 : parse_PMSwitch;
            51008 : parse_PMSwitch;
            51009 : parse_PMSwitch;
            51010 : parse_PMSwitch;
            51011 : parse_PMSwitch;
            51012 : parse_PMSwitch;
            51013 : parse_PMSwitch;
            51014 : parse_PMSwitch;
            51015 : parse_PMSwitch;
            51016 : parse_PMSwitch;
            51017 : parse_PMSwitch;
            51018 : parse_PMSwitch;
            51019 : parse_PMSwitch;
            51020 : parse_PMSwitch;
            51021 : parse_PMSwitch;
            51022 : parse_PMSwitch;
            51023 : parse_PMSwitch;
            51024 : parse_PMSwitch;
            51025 : parse_PMSwitch;
            51026 : parse_PMSwitch;
            51027 : parse_PMSwitch;
            51028 : parse_PMSwitch;
            51029 : parse_PMSwitch;
            51030 : parse_PMSwitch;
            51031 : parse_PMSwitch;
            51032 : parse_PMSwitch;
            51033 : parse_PMSwitch;
            51034 : parse_PMSwitch;
            51035 : parse_PMSwitch;
            51036 : parse_PMSwitch;
            51037 : parse_PMSwitch;
            51038 : parse_PMSwitch;
            51039 : parse_PMSwitch;
            51040 : parse_PMSwitch;
            51041 : parse_PMSwitch;
            51042 : parse_PMSwitch;
            51043 : parse_PMSwitch;
            51044 : parse_PMSwitch;
            51045 : parse_PMSwitch;
            51046 : parse_PMSwitch;
            51047 : parse_PMSwitch;
            51048 : parse_PMSwitch;
            51049 : parse_PMSwitch;
            51050 : parse_PMSwitch;
            51051 : parse_PMSwitch;
            51052 : parse_PMSwitch;
            51053 : parse_PMSwitch;
            51054 : parse_PMSwitch;
            51055 : parse_PMSwitch;
            51056 : parse_PMSwitch;
            51057 : parse_PMSwitch;
            51058 : parse_PMSwitch;
            51059 : parse_PMSwitch;
            51060 : parse_PMSwitch;
            51061 : parse_PMSwitch;
            51062 : parse_PMSwitch;
            51063 : parse_PMSwitch;
            default         :   accept;
        }
    }
    // If either host or dest port match reserved port number, parse the PMSwtich packet.
    state parse_PMSwitch{
        pkt.extract(hdr.pmswitchhds);
        transition accept;
    }

}


// Common Deparser
@Xilinx_MaxPacketRegion(1500*8)  // in bits
control PMSwitch_ack_Deparser(in headers hdr, packet_out packet) {
    apply {
        packet.emit(hdr.ethernet);
        packet.emit(hdr.ipv4);
        packet.emit(hdr.udp);
        packet.emit(hdr.pmswitchhds);
    }
}