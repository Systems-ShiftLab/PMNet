# PMNet Packet processing
Packet processing pipeline for PMNet implemented in P4 language.
PMNet packet processing manipulates incoming network packets and generates control signal for the Datapath in FPGA.

## Build Dependencies
1. Vivado 2018.2
2. Xilinx P4-SDNet toolchain (available through Xilinx University Program)

## Build Instruction
Run Makefile

### Bugs and limitations
- P4 module for Cache response generation is buggy: a temporary fix will only respond read requests from the same client that sends the update.