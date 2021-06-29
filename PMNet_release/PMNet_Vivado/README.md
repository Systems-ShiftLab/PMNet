# PMNet Hardware
Main PMNet hardware implementation



## Build Dependencies
1. Vivado 2018.2
2. Xilinx SDK

## Build Instruction

### Using a prebuilt bitstream
There is a prebuilt bitstream for VCU118 evaluation platform in the tarball.
To use prebuilt bitstream open the vivado project "10g_baseR_dual.xpr", open hardware manager and program the bitstream.

### Regenerating a bitstream
Open the vivado project "10g_baseR_dual.xpr" and generate a bitstream.

## Running the design
1. Connecting the FPGA to the network (J96 to the client side, J1 to the server side).
2. Program the bitstream to the FPGA

Now, network packets from both sides should be able to passthrough the FPGA.
The FPGA should also logs incoming PMNet update requests from the client side and sends ACK to respond them.

### Enabling Recovery and Caching
3. After programming the bitstream in Vivado's hardware manager, two "hw_vio" devices will appear in hardware manager. Set output of those hw_vio to 1.
4. Open Xilinx SDK (File -> Launch SDK), you may need to export Hardware (File -> Export -> Export Hardware) again if the bitstream is regenerated.
5. Start Caching and/or Recovery programs       
5.1 select "caching" project, then Run -> Run As -> Launch on Hardware
5.2 select "recovery" project, then Run -> Run As -> Launch on Hardware