#include "xilinx_custom.p4"
#include "common_request.p4"

control PMSwitchRequestProcessing(inout headers hdr,
                  inout PMswitch_metadata_t ctrl) {
    action AccessMemory() {
        ctrl.PMSwitchOPS    = hdr.pmswitchhds.type;
        ctrl.hashedAddress  = hdr.pmswitchhds.PMAddress;
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