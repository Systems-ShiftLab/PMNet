#include "xilinx_custom.p4"
#include "common_request.p4"

control PMSwitchCacheRequestProcessing(inout headers hdr,
                  inout PMswitch_metadata_t ctrl) {
    action writePacket() {
        ctrl.PMSwitchOPS    = hdr.pmswitchhds.type;
        ctrl.hashedAddress  = hdr.pmswitchhds.PMAddress;
        ctrl.ackCount       = 0x01;
    }
    action readPacket(){
        ctrl.PMSwitchOPS    = hdr.pmswitchhds.type;
        ctrl.hashedAddress  = hdr.pmswitchhds.PMAddress;
        ctrl.ackCount       = 0x02;
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
                writePacket();
            }else if(hdr.pmswitchhds.type == PMSWITCH_OPCODE_BYPASS){
                readPacket();
            }else{
                bypass();
            }
        }else{
            bypass();
        }
    }
}

XilinxSwitch(PMSwitch_request_Parser(), PMSwitchCacheRequestProcessing(), PMSwitch_request_Deparser()) main;