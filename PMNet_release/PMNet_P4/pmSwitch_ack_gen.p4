#include "xilinx_custom.p4"
#include "common_ack_gen.p4"

// Converts request packet to switch ACK
control PMSwitchAckGenerator(inout headers hdr,
                  inout PMswitch_metadata_t ctrl) {
    action genAck() {
        /*ctrl.PMSwitchOPS    = hdr.pmswitchhds.type;
        ctrl.hashedAddress  = hdr.pmswitchhds.PMAddress;
        ctrl.ackCount       = hdr.pmswitchhds.ackCount;
        */
        // Swap src and dst of ethernet MAC and IPv4
        IPv4Address sourceAddr = hdr.ipv4.src;
        hdr.ipv4.src = hdr.ipv4.dst;
        hdr.ipv4.dst = sourceAddr;
        // Swap Src and Dst UDP port
        bit<16> sourcePort = hdr.udp.sport;
        hdr.udp.sport = hdr.udp.dport;
        hdr.udp.dport = sourcePort;
        // Disable UDP checksum, IPv4 only
        hdr.udp.chksum = 0;
        // Swap Src and Dst MAC address
        MacAddress srcMac = hdr.ethernet.src;
        hdr.ethernet.src = hdr.ethernet.dst;
        hdr.ethernet.dst = srcMac;

        // Change PMSwitch OPS to switch ACK
        hdr.pmswitchhds.type = PMSWITCH_OPCODE_ACK;

        // We will trim the packet, need to change length in IPv4 and UDP headers.
        hdr.udp.len = 30;
        bit<16> ipv4_old_len = hdr.ipv4.len;
        hdr.ipv4.len = 20+hdr.udp.len;
        


    
        // Recompute IPv4 checksum, RFC 1624
        //hdr.ipv4.chksum = ~(~hdr.ipv4.chksum + (~ipv4_old_len) + hdr.ipv4.len);
        bit<17>newchksum_INV = ((bit<17>)(~hdr.ipv4.chksum)) + ((bit<17>)(~ipv4_old_len)) + (bit<17>)hdr.ipv4.len;
        // One's compliment carry.
        //bit<16>newchksum_INV_CR = (bit<16>)(newchksum_INV+(newchksum_INV/65536));
        // Temp fix, may cause error in corner cases.
        bit<16>newchksum_INV_CR = (bit<16>)(newchksum_INV+1);
        bit<16>newchksum = ~((bit<16>)newchksum_INV_CR);
        hdr.ipv4.chksum = newchksum;
        

    }
    action bypass() {
        ctrl.PMSwitchOPS    = PMSWITCH_OPCODE_NOOP;
        ctrl.hashedAddress  = INVALID_ADDR;
        ctrl.ackCount       = 0xFF;
    }

     apply {
        //  We still need to filter out the packet from the processor.
        if (hdr.ipv4.isValid()&& hdr.udp.isValid() && hdr.pmswitchhds.isValid()){
            if((hdr.pmswitchhds.type == PMSWITCH_OPCODE_PERSIST_NEED_ACK)){
                genAck();
            }else{
                bypass();
            }
        }else{
            bypass();
        }
    }
}

XilinxSwitch(PMSwitch_ack_gen_Parser(), PMSwitchAckGenerator(), PMSwitch_ack_gen_Deparser()) main;