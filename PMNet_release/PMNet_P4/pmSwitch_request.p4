#include "xilinx_custom.p4"
#include "common_request.p4"


// Korakit ///////////
// For this implementation, we use sequence number to index the PM instead of real hashed address to avoid implementing log state checking in the slow softcore.
// Using sequence number allows PMNet to log the request without checking if that address contains non-ACKed log.
// The read caching part will use real hashedAddress to index the PM.
// This should maintain correctness without the performance impact of using a softcore in write (update log) path.
//////////////////////
control PMSwitchRequestProcessing(inout headers hdr,
                  inout PMswitch_metadata_t ctrl) {
    action AccessMemory() {
        ctrl.PMSwitchOPS    = hdr.pmswitchhds.type;
        ctrl.hashedAddress  = ((hdr.pmswitchhds.seq_no*2048)%0x80000000)+0x80000000;
        ctrl.ackCount       = hdr.pmswitchhds.ackCount;
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
                AccessMemory();
            }else{
                bypass();
            }
        }else{
            bypass();
        }
    }
}

XilinxSwitch(PMSwitch_request_Parser(), PMSwitchRequestProcessing(), PMSwitch_request_Deparser()) main;